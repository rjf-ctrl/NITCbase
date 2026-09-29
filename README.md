# Stage 6 — Buffer Management and Disk Write-back

## Goal

Everything up to Stage 5 was read-only: blocks were loaded from disk into `StaticBuffer` and never modified. This stage introduces the first operations that change data and persist it. It implements `ALTER TABLE RENAME` (rename a relation) and `ALTER TABLE RENAME COLUMN` (rename an attribute), and adds the buffer machinery they depend on: a dirty bit, LRU block replacement, and write-back of modified blocks to disk when they are evicted or when the program exits.

## Background

### Block Replacement (LRU)

- `StaticBuffer` holds 32 blocks at a time. When a new block is needed and every slot is occupied, an existing slot must be reused.
- NITCbase uses **LRU (Least Recently Used)**. Each buffer entry carries a `timeStamp` measuring how long it has gone unused, and the entry with the highest timestamp is the one replaced.
- Each entry also has a boolean `dirty` field.
  - `dirty = true` means the block was modified after being loaded.
  - A dirty block is written back to disk when it is replaced or when the system exits.
- Key idea: evict the least recently used block, but persist its changes first if it is dirty.
# Stage 6 — Buffer Management and Disk Write-back

## Goal

Until Stage 5 everything was read-only. This stage adds the first operations that modify data and persist it: `ALTER TABLE RENAME` (relation) and `ALTER TABLE RENAME COLUMN` (attribute). To support them, the buffer gets a dirty bit, LRU block replacement, and write-back of modified blocks on eviction or exit.

Call flow: `Frontend → Schema → BlockAccess → Buffer`. Renaming is a schema operation, so the relation must be **closed** first.

## Phase 1: LRU and Dirty Bit in the Buffer Layer

### `Buffer/StaticBuffer.cpp` (updated)

- Buffer holds 32 blocks. Each slot tracks `free`, `dirty`, `timeStamp` (time since last use) and `blockNum`.
- Constructor: initializes all slots as free and clean.
- Destructor: writes back every dirty occupied slot via `Disk::writeBlock`.
- `getFreeBuffer(blockNum)`: ages all occupied slots, uses a free slot if any, otherwise evicts the slot with the highest timestamp (writing it back first if dirty). Resets the new slot's timestamp to 0.
- `setDirtyBit(blockNum)`: marks the slot holding that block dirty; `E_BLOCKNOTINBUFFER` if it isn't buffered.

### `Buffer/BlockBuffer.cpp` and `RecBuffer.cpp` (updated)

- `loadBlockAndGetBufferPtr()`: on a hit, resets that slot's timestamp and ages the rest; on a miss, gets a slot from `getFreeBuffer` and reads the block from disk.
- `RecBuffer::setRecord(record, slotNum)`: validates the slot, copies the record into the buffer, calls `setDirtyBit`.

## Phase 2: Renaming

### `BlockAccess/BlockAccess.cpp` (new functions)

Both reuse `linearSearch` on the catalogs plus `getRecord`/`setRecord`.

- `renameRelation(old, new)`: fails with `E_RELEXIST` if `new` exists or `E_RELNOTEXIST` if `old` doesn't. Rewrites the name in the `RELCAT` record, then in every matching `ATTRCAT` record.
- `renameAttribute(rel, old, new)`: scans the relation's `ATTRCAT` entries; `E_ATTREXIST` if `new` already exists, `E_ATTRNOTEXIST` if `old` isn't found, otherwise rewrites that entry.

### `Schema/Schema.cpp` (new functions)

- `Schema::renameRel` / `Schema::renameAttr`: reject the catalogs (`E_NOTPERMITTED`), reject open relations (`E_RELOPEN`), then delegate to BlockAccess.
- `Frontend::alter_table_rename()` and `alter_table_rename_column()` call these.

## Outcome

- Both rename commands work and persist across restarts.
- Bad renames (duplicate name, missing relation/attribute, open relation, catalogs) fail cleanly with an error.
- Modified blocks reach disk on eviction or in the `StaticBuffer` destructor.
- Common bugs: no `setDirtyBit` in `setRecord` (rename lost after exit); timestamps not reset on a cache hit (LRU evicts hot blocks); search index not reset between `linearSearch` calls (misses entries).
### Call Flow

`Frontend → Schema → Block Access → Buffer`

- `Frontend::alter_table_rename()` → `Schema::renameRel()` → `BlockAccess::renameRelation()`
- `Frontend::alter_table_rename_column()` → `Schema::renameAttr()` → `BlockAccess::renameAttribute()`

Renaming is a **schema-level** operation, so it is handled by the Schema Layer. The relation must be **closed** before its schema can be modified.

## Phase 1: Dirty Bit and LRU in the Buffer Layer

### `Buffer/StaticBuffer.cpp` (updated)

*What changed: the buffer now tracks recency and modification per slot, and can evict a block instead of only filling free slots.*

- Constructor: initializes every `metainfo` slot as free, not dirty, with no block assigned and timestamp reset.
- Destructor: now walks all slots and writes back every occupied slot whose `dirty` bit is set via `Disk::writeBlock`, so nothing modified is lost at exit.
- `getFreeBuffer(blockNum)`:
  - Increments the timestamp of every occupied slot, since one more access has passed for them.
  - Uses a free slot if one exists; otherwise picks the occupied slot with the highest timestamp (the LRU victim).
  - If the victim is dirty, writes it back to disk before reusing the slot.
  - Marks the chosen slot as holding `blockNum`, not dirty, with timestamp 0, and returns its buffer index.
- `setDirtyBit(blockNum)`: looks up the buffer slot holding `blockNum` and sets its `dirty` flag. Returns `E_BLOCKNOTINBUFFER` if the block isn't in the buffer and `E_OUTOFBOUND` for an invalid index.

### `Buffer/BlockBuffer.cpp` (updated)

- `loadBlockAndGetBufferPtr()`:
  - If the block is already in the buffer, it counts as a fresh access: its timestamp is reset to 0 and every other occupied slot's timestamp is incremented.
  - If it is not in the buffer, it calls `StaticBuffer::getFreeBuffer` (which may trigger an eviction plus write-back) and then reads the block from disk into that slot.

### `Buffer/RecBuffer.cpp` (new function)

- `RecBuffer::setRecord(record, slotNum)`:
  - Gets the block's buffer pointer through `loadBlockAndGetBufferPtr`.
  - Validates `slotNum` against the block's slot count.
  - Copies the record into that slot's position in the buffer.
  - Calls `StaticBuffer::setDirtyBit` so the change is persisted on eviction or exit.
  - The mirror image of `getRecord`, which only reads.

## Phase 2: Renaming in the Block Access Layer

### `BlockAccess/BlockAccess.cpp` (new functions)

Both functions reuse Stage 4's `linearSearch` to locate catalog entries instead of scanning blocks by hand, and use `getRecord` / `setRecord` for the read-modify-write.

- `renameRelation(oldName, newName)`:
  - Resets the search index on `RELCAT_RELID`, then searches `RELCAT` for `newName`. If it exists, fails with `E_RELEXIST`.
  - Resets the search index again and searches `RELCAT` for `oldName`. If not found, fails with `E_RELNOTEXIST`.
  - Reads that record, overwrites the relation-name field with `newName`, and writes it back with `setRecord`.
  - Loops `linearSearch` on `ATTRCAT` matching `oldName` in the relation-name field, and rewrites every attribute entry of that relation with `newName`. The loop runs until `linearSearch` reports no more matches.
- `renameAttribute(relName, oldName, newName)`:
  - Confirms `relName` exists in `RELCAT` (`E_RELNOTEXIST` otherwise).
  - Walks all `ATTRCAT` entries of `relName` with `linearSearch`, checking each attribute name.
    - If an entry already has `newName`, fails with `E_ATTREXIST`.
    - Remembers the location of the entry whose name equals `oldName`.
  - If no entry matched `oldName`, fails with `E_ATTRNOTEXIST`.
  - Otherwise overwrites the attribute-name field of the remembered entry and writes it back with `setRecord`.

The modified catalog blocks are not written to disk here; they are marked dirty and flushed later by the Buffer Layer.

## Phase 3: Schema and Frontend

### `Schema/Schema.cpp` (new functions)

- `Schema::renameRel(oldName, newName)`:
  - Blocks renaming either catalog (`E_NOTPERMITTED`).
  - Checks whether the relation is currently open via `OpenRelTable::getRelId`. If it is, fails with `E_RELOPEN`, because the cached catalog entries would go stale.
  - Delegates to `BlockAccess::renameRelation` and passes its result straight back.
- `Schema::renameAttr(relName, oldAttr, newAttr)`:
  - Blocks modifying the catalogs (`E_NOTPERMITTED`).
  - Requires the relation to be closed (`E_RELOPEN` otherwise).
  - Delegates to `BlockAccess::renameAttribute`.

### `Frontend/Frontend.cpp` (updated)

- `Frontend::alter_table_rename()` calls `Schema::renameRel`.
- `Frontend::alter_table_rename_column()` calls `Schema::renameAttr`.
- Both print a success message on `SUCCESS` and otherwise map the error code to a readable message.

## Key Data Structures

- **`StaticBuffer::metainfo` (`BufferMetaInfo`)** – per-slot bookkeeping: whether the slot is free, whether it is dirty, its `timeStamp` for LRU, and which disk block it holds. The Stage 3/4 buffer only needed the free flag and block number; `dirty` and `timeStamp` are what make write-back and replacement possible.
- **Catalog records** – unchanged in layout. Renaming just overwrites the name field of an existing `RELCAT` record and of every matching `ATTRCAT` record.

## Outcome

- A relation can be renamed with `ALTER TABLE RENAME <old> TO <new>`, and a column with `ALTER TABLE RENAME <rel> COLUMN <old> TO <new>`. The change is visible in `RELCAT` / `ATTRCAT` and survives after the program exits.
- Renaming to a name that already exists, or renaming something that doesn't exist, fails with the matching error instead of corrupting the catalogs.
- Renaming an open relation is refused, and the catalogs themselves cannot be renamed.
- When all 32 buffer slots are full, the least recently used block is evicted, and it is written back first if dirty.
- Every block modified during the session reaches disk by the end of the run, either at eviction time or in the `StaticBuffer` destructor.
- Nothing above Block Access needed structural changes: rename reuses `linearSearch` and `getRecord` from earlier stages, and the only truly new primitives are `setRecord`, `setDirtyBit`, and LRU in `getFreeBuffer`.
- Easy-to-hit bugs worth remembering:
  - Forgetting to call `setDirtyBit` in `setRecord` makes the rename appear to work in-session but vanish after restart, since the block is never flushed.
  - Forgetting to reset timestamps in `loadBlockAndGetBufferPtr` on a cache hit makes LRU evict blocks that were just used.
  - Not resetting the search index before each `linearSearch` in the rename functions makes the search resume from the previous hit and miss entries.