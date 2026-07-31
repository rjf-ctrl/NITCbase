# Stage 1 — Reading & Writing a Disk Block

## Goal

Get the basic disk I/O primitive working — every later layer (buffer, cache, B+ tree, algebra) is built on top of `Disk::readBlock()` / `Disk::writeBlock()`, so this stage confirms that path works before anything else is implemented on it.

## Implementation
`main.cpp`
- Instantiated `Disk disk_run` to create the run copy of the disk for the session.
- Allocated a 2048-byte (`BLOCK_SIZE`) buffer to hold one block in memory.
- Called `Disk::readBlock(buffer, 7000)` to load block 7000 (unused) into the buffer.
- Wrote `"hello"` into the buffer at byte offset 20 via `memcpy()`.
- Called `Disk::writeBlock(buffer, 7000)` to persist the modified buffer back to block 7000.
- Re-read block 7000 into a second buffer and printed bytes 20–25 to confirm the write — output `hello` confirms correctness.
- **Assignment:** read block **0** (the Block Allocation Map) and printed its first 8 bytes as raw integers to inspect free/occupied block tracking.

## Build & run

```bash
cd mynitcbase
make
./nitcbase
```