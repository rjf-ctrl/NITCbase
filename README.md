# Stage 5 — Opening Arbitrary Relations

## Goal

Stage 4 left a hard limitation: `SELECT` only worked on `RELATIONCAT` and `ATTRIBUTECAT`, because `OpenRelTable::getRelId` was hardcoded to recognize just those two names. This stage removes that restriction — it builds real relation-opening machinery, so any relation on disk (`Students`, `Events`, etc.) can be opened into a free cache slot, searched, and closed again, using the same `BlockAccess`/`Algebra` layers built in Stage 4 without any changes to them.

## Phase 1: Tracking Which Relations Are Open

### `Cache/OpenRelTable.cpp` (updated)

*What changed: alongside the existing `relCache`/`attrCache` arrays, this stage introduces `tableMetaInfo` — bookkeeping for which of the 12 cache slots are free and which relation name occupies each occupied one.*

- Constructor: now also initializes every `tableMetaInfo` slot to free, then immediately marks `RELCAT_RELID` and `ATTRCAT_RELID` as permanently occupied with their names set — these two never get closed or reassigned, since every other operation depends on them staying loaded.
- Destructor: unchanged in spirit — still frees slots 0 and 1 directly, and now also closes any other relation left open (rel-id ≥ 2) via `closeRel` before the program exits, so nothing leaks regardless of what the user opened during the session.

## Phase 2: Opening and Closing Relations

### `Cache/OpenRelTable.cpp` (new functions)

- `getFreeOpenRelTableEntry()`: scans `tableMetaInfo` from slot 2 onward (0 and 1 are reserved for the catalogs) and returns the first free slot, or `E_CACHEFULL` if all 12 are occupied.

- `getRelId(relName)`: rewritten from Stage 4's two-name hardcode into a real lookup — scans every occupied `tableMetaInfo` slot for a name match and returns its rel-id, or `E_RELNOTOPEN` if the relation isn't currently open. This is what unblocks `Algebra::select` for arbitrary relations, since `select` was already written generically against `getRelId` — it never needed to change.

- `openRel(relName)`: the core addition this stage.
  - Returns the existing rel-id immediately if the relation is already open, so nothing gets opened twice.
  - Grabs a free slot via `getFreeOpenRelTableEntry`.
  - Uses `BlockAccess::linearSearch` on `RELCAT_RELID` to find the relation's own entry in the relation catalog by name — reusing Stage 4's search layer rather than manually scanning blocks.
  - Loads that entry into `relCache` at the new rel-id.
  - Loops `linearSearch` on `ATTRCAT_RELID`, once per expected attribute (`numAttrs` from the entry just loaded), building a linked list of that relation's attributes into `attrCache` — same resumable-search pattern `select` itself uses to walk multiple matches.
  - Marks the slot occupied in `tableMetaInfo` with the relation's name.

- `closeRel(relId)`:
  - Refuses to close `RELCAT_RELID` or `ATTRCAT_RELID` (`E_NOTPERMITTED`) — the catalogs must stay open for the program's lifetime.
  - Validates `relId` is in range and actually open, failing with `E_OUTOFBOUND` / `E_RELNOTOPEN` otherwise.
  - Frees the `relCache` entry and walks/frees the entire `attrCache` linked list for that rel-id — every `malloc` from `openRel` gets exactly one matching `free`.
  - Resets the slot to free and nulls out both cache pointers, so the slot is safely reusable by a future `openRel` call.

### `Schema/Schema.cpp` (new)

- `Schema::openRel(relName)`: thin wrapper — calls `OpenRelTable::openRel`, translates a non-negative rel-id into `SUCCESS`, and passes any error code straight through. This is what the frontend's `OPEN TABLE` command actually calls.
- `Schema::closeRel(relName)`: blocks closing either catalog by name before even doing a lookup, otherwise resolves the name via `getRelId` and delegates to `OpenRelTable::closeRel`.

## Key Data Structures

- **`OpenRelTableMetaInfo`** – per-slot bookkeeping added this stage: whether the slot is free, and if not, which relation's name occupies it. This is what makes `getRelId` and `getFreeOpenRelTableEntry` possible — Stage 4 had no concept of "currently open relations" beyond two hardcoded IDs.
- **`relCache` / `attrCache`** – same structures from Stage 3, now populated dynamically by `openRel` for any relation instead of only being hand-filled in the constructor for the catalogs (and `Students`, as a one-off Stage 3 exercise).

## Outcome

- Any relation on disk can now be opened (`OPEN TABLE <name>`), searched (`SELECT`), and closed (`CLOSE TABLE <name>`) — `Algebra::select` from Stage 4 needed zero changes, since it was always written against the generic `getRelId`/`linearSearch` interface.
- Opening a relation that doesn't exist correctly fails, rather than silently reporting success — a real check now backs the operation instead of a placeholder message.
- Opening an already-open relation returns the same rel-id rather than duplicating cache entries.
- Closing a relation properly frees every allocation made when it was opened — no leaks, verified by matching every `malloc` in `openRel` to a `free` in `closeRel`.
- The catalogs remain permanently open and cannot be closed, matching the constraint that every other cache lookup depends on them.
- Two easy-to-hit setup bugs worth remembering: forgetting to actually define the static `tableMetaInfo` array (`OpenRelTable::tableMetaInfo`, not a stray `static` file-scope variable of the same name) causes linker errors; forgetting to initialize `tableMetaInfo` slots as free in the constructor makes every `OPEN TABLE` fail with `E_CACHEFULL` immediately, since uninitialized struct memory reads as "not free."