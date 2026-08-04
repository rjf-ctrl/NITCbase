# Stage 2 — Buffer Layer & Reading Records

## Goal

Implement the **Buffer Layer** on top of the Physical Layer to support reading **block headers** and **records**. Use these APIs to reconstruct the database schema stored in the **Relation Catalog** and **Attribute Catalog**.

## Implementation

### `Buffer/BlockBuffer.cpp`

- Implemented `BlockBuffer::BlockBuffer(int blockNum)` to associate a buffer object with a specific disk block.
- Implemented `RecBuffer::RecBuffer(int blockNum)` by invoking the parent `BlockBuffer` constructor.
- Implemented `BlockBuffer::getHeader()`:
  - Reads a disk block into memory using `Disk::readBlock()`.
  - Extracts block metadata (`numEntries`, `numAttrs`, `numSlots`, `lblock`, `rblock`) into a `HeadInfo` structure.
- Implemented `RecBuffer::getRecord()`:
  - Reads the block header to determine the record layout.
  - Computes the byte offset of the requested record using the header information.
  - Copies the requested record into an `Attribute[]`.

### `main.cpp`

- Initialized the disk subsystem.
- Created `RecBuffer` objects for the Relation Catalog (Block 4) and Attribute Catalog (Block 5).
- Read both catalog headers using `getHeader()`.
- Iterated through every Relation Catalog record using `getRecord()`.
- For each relation, scanned the Attribute Catalog.
- Matched Attribute Catalog entries using the relation name.
- Converted internal attribute type (`NUMBER`/`STRING`) to `"NUM"` or `"STR"`.
- Printed the complete database schema (relation → attributes → types).

## Key Data Structures

- **`BlockBuffer`** – Represents a disk block and provides block-level operations.
- **`RecBuffer`** – Extends `BlockBuffer` with record-level operations.
- **`HeadInfo`** – Stores block header metadata.
- **`Attribute`** – Represents a single attribute value (`NUM`/`STR`); an `Attribute[]` represents one complete record.

## Assignment 1 — Reading Across Multiple Attribute Catalog Blocks

- Modified `main()` to support **multi-block Attribute Catalogs**.
- Traversed the Attribute Catalog as a **linked list** using the `rblock` pointer in each block header.
- Created a new `RecBuffer` for every Attribute Catalog block encountered.
- Processed every record in the current block before moving to the next.
- Verified the implementation by creating additional relations (`Events`, `Locations`, `Participants`) so that the Attribute Catalog spanned multiple blocks and confirming that all attributes were printed correctly. :contentReference[oaicite:0]{index=0}

## Assignment 2 — Updating Records

- Implemented `RecBuffer::setRecord()` as the write counterpart of `getRecord()`.
- Read the target block into memory.
- Located the required record using the same offset calculation as `getRecord()`.
- Overwrote the record bytes using `memcpy()`.
- Wrote the updated block back to disk using `Disk::writeBlock()`.
- Modified `main()` to:
  - Search the Attribute Catalog for the `Student` relation's `Class` attribute.
  - Rename the attribute to `Batch`.
  - Write the updated record back to disk using `setRecord()`.
  - Print the schema again to verify the modification. :contentReference[oaicite:1]{index=1}

## Outcome

- Built the first abstraction over raw disk I/O through the **Buffer Layer**.
- Enabled reading block metadata and records from disk.
- Reconstructed the database schema using the system catalogs.
- Extended catalog traversal to support linked lists of record blocks.
- Added support for updating records and persisting changes back to disk.

