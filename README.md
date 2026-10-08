# Stage 7: Record Insertion and Cache Write-back

## Goal

Stage 6 let us modify existing catalog records. This stage adds the first operation that creates data: `INSERT INTO <rel> VALUES (...)`. Inserting changes more than the data block. The relation's record count, the block's slot map and header, and possibly the block chain all change. To keep the catalogs consistent, the caches get a dirty bit and are written back to the catalogs when a relation is closed.

## Call Flow

`Frontend → Algebra → Block Access → Buffer`

- `Frontend::insert_into_table_values()` → `Algebra::insert()` → `BlockAccess::insert()`
- `CLOSE TABLE` → `Schema::closeRel()` → `OpenRelTable::closeRel()` (now writes the caches back)

The relation must be **open** for insert, since `BlockAccess::insert` works through the cache entries. `Algebra::insert` opens it if needed.

## Phase 1: Buffer Layer Additions

### `Buffer/BlockBuffer.cpp` and `RecBuffer.cpp` (new or updated)

- `RecBuffer::getSlotMap` / `setSlotMap`: read or write the slot occupancy bitmap of a record block. `setSlotMap` marks the block dirty.
- `BlockBuffer::setHeader`: writes a block header (`numEntries`, `numAttrs`, `numSlots`, `lblock`, `rblock`, `pblock`) into the buffer and marks it dirty.
- `BlockBuffer::getFreeBlock(blockType)`: finds a free block on disk through the block allocation map, claims it, and gets a buffer slot for it.
- `RecBuffer()` (no-argument constructor): allocates a new record block with `getFreeBlock`. If the disk is full, it records the error in `blockNum`.

### `Buffer/StaticBuffer.cpp` (updated)

- Stage 6's LRU and dirty-bit logic is reused unchanged.
- The block allocation map is updated whenever a block is claimed or released.

## Phase 2: Cache Dirty Bits and Write-back

### `Cache/RelCacheTable.cpp` (updated)

- `setRelCatEntry(relId, relCatBuf)`: overwrites the cached `RelCatEntry` and sets the entry's `dirty` flag.
- Insert updates `numRecs`, `firstBlk` and `lastBlk` through this function.

### `Cache/AttrCacheTable.cpp` (new function)

- `attrCatEntryToRecord(attrCatEntry, record)`: the inverse of `recordToAttrCatEntry`. It converts a cached `AttrCatEntry` back to an `Attribute` record so it can be written to the attribute catalog.

### `Cache/OpenRelTable.cpp` (updated)

- `closeRel(relId)` now writes back before freeing:
  - If the relation cache entry is dirty, it converts the entry with `relCatEntryToRecord` and writes it to its `recId` in the relation catalog with `setRecord`.
  - It walks the attribute cache linked list. Every dirty `AttrCacheEntry` is converted with `attrCatEntryToRecord` and written to its `recId` in the attribute catalog.
  - It then frees both caches and marks the `tableMetaInfo` slot free.
- Every cache entry is created with `dirty = false`. `malloc` does not zero memory.

## Phase 3: Insertion in the Block Access Layer

### `BlockAccess/BlockAccess.cpp` (new function)

- `BlockAccess::insert(relId, record)`:
  - Reads the relation's `RelCatEntry` to get `firstBlk`, `numAttrs` and `numSlotsPerBlk`.
  - Walks the block chain, reading each slot map, to find a free slot.
  - If every block is full or the relation has no blocks yet:
    - It allocates a new `RecBuffer`. `E_DISKFULL` if that fails.
    - It writes the header and an all-free slot map to the new block, and links it with `rblock` and `lblock` to the previous last block.
    - If it is the first block, it sets `firstBlk`.
    - It updates `lastBlk`.
  - It writes the record with `setRecord` and marks the slot occupied in the slot map.
  - It increments `numEntries` in the block header and `numRecs` in the relation cache entry, which sets the cache dirty bit.

### `Algebra/Algebra.cpp` (new function)

- `Algebra::insert(relName, nAttrs, record)`:
  - Rejects the catalogs (`E_NOTPERMITTED`).
  - Gets the rel-id with `OpenRelTable::getRelId`, returning `E_RELNOTOPEN` if the relation is not open.
  - Checks that `nAttrs` matches `numAttrs` (`E_NATTRMISMATCH`).
  - Converts each input string by attribute type. Numbers are parsed, and a non-numeric value for a NUM attribute gives `E_ATTRTYPEMISMATCH`.
  - Delegates to `BlockAccess::insert`.

### `Frontend/Frontend.cpp` (updated)

- `Frontend::insert_into_table_values()` calls `Algebra::insert` and prints a success message or the matching error.

## Key Data Structures

- **Slot map**: one byte per slot at the start of a record block's body. It marks each slot as occupied or free, and insert uses it to find where to write.
- **Block header**: `numEntries` and the `lblock` / `rblock` links are what chain a relation's blocks together.
- **`RelCacheEntry` / `AttrCacheEntry` `dirty` flag**: tracks whether the cached catalog data differs from the catalog on disk. It decides what `closeRel` writes back.

## Outcome

- `INSERT INTO Locations VALUES (elhc,300);` adds a record, allocating a new block when needed.
- `SELECT * FROM Locations INTO null WHERE capacity>0;` returns every inserted row.
- After `CLOSE TABLE Locations;`, `SELECT * FROM RELATIONCAT INTO null WHERE RelName=Locations;` shows `#Records` equal to the number of inserted rows.
- Inserts with the wrong attribute count, a wrong type, or into a catalog fail cleanly with an error.
- Everything reaches disk through the dirty bit mechanism, either on eviction or on a clean `exit`.

## Common Bugs

- **`dirty` not initialized:** a `malloc`'d entry holds garbage in `dirty`, so `closeRel` writes junk back to the catalogs. Set `dirty = false` in the constructor and in `openRel` for both caches.
- **`#Records` stays 0 after close:** insert updated `numRecs` without going through `setRelCatEntry`, or `closeRel` never reaches the dirty branch.
- **Missing `attrCatEntryToRecord` definition:** if it is declared in the header but not defined in the `.cpp`, you get a linker error (`undefined reference`).
- **Slot map or header not updated:** the record is written but never shows up in `SELECT`, because the slot is still marked free.
- **Block chain links:** forgetting to set `rblock` on the old last block, or `lblock` on the new one, hides the new block from sequential scans.
- **Not exiting cleanly:** leave with `exit` at the prompt, or buffered changes may never reach disk.