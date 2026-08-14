# Stage 3 — The Disk Buffer & Catalog Caches

## Goal

Disk reads are slow. This stage adds a memory buffer so we don't hit the disk every single time we need a block, and builds an in-memory cache of the two system catalogs (Relation Catalog, Attribute Catalog) so basic schema info is instantly available without re-reading records over and over.

## Phase 1: The Disk Buffer

### `Buffer/StaticBuffer.cpp` (new file)

*What this file does: holds 32 "slots" of memory, each one big enough to hold a copy of a disk block. Keeps track of which slots are empty and which disk block each occupied slot is holding.*

- Set up the memory pool itself — `blocks[32][BLOCK_SIZE]` — and a second array, `metainfo[32]`, that just remembers "is this slot free?" and "if not, which block number is sitting in it?"
- Constructor: marks all 32 slots as empty when the program starts.
- Destructor: does nothing for now. We're only reading through the buffer this stage, not writing to it, so there's nothing to save back to disk yet. That comes in Stage 6.
- `getFreeBuffer(blockNum)`: finds the first empty slot, marks it as now holding `blockNum`, and hands back which slot number it used.
- `getBufferNum(blockNum)`: checks if a given block is already sitting in one of the 32 slots. If yes, returns which slot. If not, says so (`E_BLOCKNOTINBUFFER`).

### `Buffer/BlockBuffer.cpp` (existing file, updated)

*Before this stage, every read went straight to disk. Now it checks memory first.*

- New function `loadBlockAndGetBufferPtr()`: this is the actual "get me this block" function everything else now calls. It first asks the buffer "do you already have this block?" If yes, use it. If no, find a free slot, actually read from disk into that slot, then use it.
- `getHeader()`: instead of reading disk directly like it did in Stage 2, it now calls the function above and reads from whatever memory it hands back.
- `getRecord()`: same change — reads via the buffer instead of the disk.

### `main.cpp`

- Added one line: `StaticBuffer buffer;` right after `Disk disk_run;`. This just makes sure the buffer is switched on (all slots marked empty) before anything tries to use it.
- Nothing else changed — the program prints the exact same output as Stage 2, just faster under the hood since repeat reads no longer touch disk.

## Key Data Structures

- **`StaticBuffer`** – the buffer itself. One shared instance for the whole program (it's a static class — no separate objects needed).
- **`BufferMetaInfo`** – per-slot bookkeeping: is it free, and which block does it hold.
- **`BlockBuffer` / `RecBuffer`** – same external behavior as Stage 2, just internally rewired to go through the buffer.

## Outcome

- Reads now check memory before touching disk.
- No cleanup logic for a full buffer yet (what happens when all 32 slots are used? — not handled until Stage 6).
- No writing back to disk yet — buffer is read-only this stage.
- Confirmed: output identical to Stage 2, so the change is invisible from the outside — it's purely a speed/internals improvement.

---

## Phase 2: The Catalog Caches

### `Cache/RelCacheTable.cpp` (new file)

*This file lets you look up a relation's catalog info quickly. It doesn't fill the cache — it just reads from it once something else has filled it in.*

- `relCache[12]` — an array with 12 slots, one per relation that can be "open" at once. Each slot holds a pointer to that relation's cached info (or nothing, if the slot's unused).
- `getRelCatEntry(relId, ...)`: hands back the cached info for a given relation, if it exists. Returns an error if the slot is out of range or empty.
- `recordToRelCatEntry(...)`: takes a raw record (the way data comes straight off disk) and turns it into a clean, easy-to-use struct — pulling out the relation's name, number of attributes, number of records, which blocks it lives in, etc.

### `Cache/AttrCacheTable.cpp` (new file)

*Same idea as above, but for attributes. Since a relation can have any number of attributes, this isn't a flat array — it's a linked list per relation.*

- `attrCache[12]` — 12 linked-list heads, one per relation slot.
- `getAttrCatEntry(relId, attrOffset, ...)`: walks the linked list for that relation looking for the attribute at a given position (offset), and returns its info.
- `recordToAttrCatEntry(...)`: same idea as the relation-catalog version — converts a raw record into a clean struct (attribute name, type, whether it's a primary key, etc).

### `Cache/OpenRelTable.cpp` (new file)

*This is where the caches actually get filled in. The two files above just read — this one writes, once, at startup.*

- Constructor:
  - Clears out both caches first (marks every slot empty).
  - Reads the Relation Catalog's own entry off disk, converts it, and stores it in `relCache` slot 0.
  - Does the same for the Attribute Catalog's entry, stored in slot 1.
  - Reads all 6 attributes belonging to the Relation Catalog, builds a linked list out of them, stores the list in `attrCache` slot 0.
  - Does the same for the Attribute Catalog's own 6 attributes, stored in `attrCache` slot 1.
- Destructor: frees everything that was allocated above (both relation entries, both attribute linked lists), so nothing leaks when the program exits.

### `main.cpp`

- Added `OpenRelTable cache;` — this is what actually triggers the cache-filling logic above. Has to come after `Disk` and `StaticBuffer` since it depends on both.
- Changed the print loop to pull data through `RelCacheTable::getRelCatEntry()` and `AttrCacheTable::getAttrCatEntry()` instead of reading raw records — output looks identical, just sourced from the cache now instead of a fresh read every time.

## Key Data Structures

- **`RelCacheEntry`** – one relation's catalog info (name, attribute count, record count, block range) plus where it lives on disk.
- **`AttrCacheEntry`** – one attribute's info (name, type, etc), plus a pointer to the next attribute in the list — because a relation can have any number of them.
- **`RelCacheTable` / `AttrCacheTable`** – just hold the arrays and provide lookup/conversion functions. No setup logic of their own.
- **`OpenRelTable`** – the only class that actually does something at startup (fills the caches) and cleanup at shutdown (frees them).

## Outcome

- Relation Catalog and Attribute Catalog info is now sitting in memory from the start, instead of being re-read every time it's needed.
- Only these two catalogs are cached so far — being able to cache *any* relation comes in Stage 5.
- Everything allocated gets properly freed — no memory leaks for what's implemented here.
- Output still matches Stage 2/Phase 1 exactly.

---

## Exercise Q1 — Also Caching the `Students` Relation

*Goal: make the cache hold a normal user-created relation too, not just the two system catalogs. Unlike RELCAT/ATTRCAT, `Students` doesn't live at a known, fixed spot — so we have to search for it.*

### `Cache/OpenRelTable.cpp`

- Gave `Students` slot number 2 in the cache (right after RELCAT=0 and ATTRCAT=1).
- Added to the constructor, after the existing setup:
  - **Finding Students in the Relation Catalog:** loop through every record in the Relation Catalog block, checking each one's name against `"Students"`. When found, remember which slot it was in.
  - Convert and store that entry in `relCache` slot 2, same way as before.
  - **Finding Students' attributes:** this time we can't assume everything fits in one block — if enough relations have been created, the Attribute Catalog might overflow into a second (or third) block. So we follow the "next block" pointer in each block's header and check every block, not just the first, for any attribute belonging to `Students`.
  - Build a linked list out of whatever attributes we find, store it in `attrCache` slot 2.
  - All of this only runs if `Students` was actually found — if it wasn't created yet, this part is skipped safely instead of crashing.
- Updated the destructor to loop over all three used slots (0, 1, 2) instead of hardcoding just the first two, so `Students`' memory gets freed too.

### `main.cpp`

- Extended the print loop to also cover slot 2, so `Students` gets printed along with the two catalogs.
- Added a safety check — if a slot turns out to be empty (relation wasn't found), skip it instead of crashing.
- Didn't add any manual linked-list walking here — attribute lookups still go through `getAttrCatEntry()`, which already knows how to search the list. `main.cpp` doesn't need to know or care that attributes are stored as a linked list under the hood.

## Outcome

- `Students` now gets cached and printed automatically at startup, found by searching rather than assuming a fixed location.
- Works correctly even if the Attribute Catalog has grown past one block (from creating other relations earlier).
- No memory leaks — cleanup now covers all three cached relations.
- Verified: program prints RELATIONCAT, ATTRIBUTECAT, and Students (with all its attributes) in one run.