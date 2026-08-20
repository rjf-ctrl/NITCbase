# Stage 4 — The Block Access Layer

## Goal

So far, working with a relation meant manually reading block by block, checking headers and slot maps by hand. This stage builds a layer that hides all of that — `BlockAccess` — so higher-level code can just ask "give me the next record that matches this condition" without knowing anything about blocks, slots, or how relations are laid out on disk. On top of it, this stage also implements the first real relational operation: `SELECT`.

## Phase 1: Linear Search

### `BlockAccess/BlockAccess.cpp` (new file)

*What this file does: scans through a relation's blocks one record at a time, looking for records that satisfy a condition, and remembers where it left off so it can be called again for the next match.*

- `linearSearch(relId, attrName, attrVal, op)`:
  - Checks the relation cache for a previous search position (`searchIndex`). If there isn't one (`{-1, -1}`), starts from the relation's first block. If there is one, resumes from the very next slot after the last hit — this is what lets the function be called repeatedly to walk through all matches one at a time, instead of returning everything at once.
  - Walks the relation's blocks using each block's header (`rblock`) to find the next one, the same way a linked list is traversed.
  - For each block, checks the slot map first — skips any slot marked unoccupied without wasting time reading it.
  - For each occupied slot, reads the record and looks up where the target attribute sits inside it (`AttrCacheTable::getAttrCatEntry`), then compares that value against `attrVal` using `compareAttrs`.
  - If the comparison satisfies whichever operator was passed in (`EQ`, `NE`, `LT`, `LE`, `GT`, `GE`), it saves this position as the new search index (so the *next* call resumes from here) and returns the record's location.
  - If it runs out of blocks without a match, returns `{-1, -1}`.

## Phase 2: The SELECT Operation

### `Algebra/Algebra.cpp` (new file)

*This is where relational operations live. Stage 4 adds the first one — SELECT — plus two small helpers it depends on.*

- `compareAttrs(attr1, attr2, attrType)`: compares two attribute values and returns negative, zero, or positive — using `strcmp` for STRING attributes and plain subtraction for NUMBER attributes. This one function is what lets `linearSearch` handle both types uniformly instead of branching everywhere.
- `isNumber(str)`: checks whether a string is purely a valid float, with nothing extra before or after it — used to validate user input before treating it as a NUMBER value.
- `Algebra::select(srcRel, targetRel, attr, op, strVal)`:
  - Resolves `srcRel`'s name to an ID via `OpenRelTable::getRelId` — fails with `E_RELNOTOPEN` if it isn't open.
  - Looks up the condition attribute's catalog entry to get its type — fails with `E_ATTRNOTEXIST` if the attribute doesn't exist.
  - Converts `strVal` (always a string from user input) into a proper `Attribute` union — parsed as a float if the attribute is NUMBER (validated with `isNumber`, failing with `E_ATTRTYPEMISMATCH` if it isn't a real number), or copied as-is if it's STRING.
  - Resets the search index (`RelCacheTable::resetSearchIndex`) so this query starts fresh instead of continuing from some earlier unrelated search.
  - Prints a header row of attribute names.
  - Repeatedly calls `BlockAccess::linearSearch` in a loop, printing each matching record until it returns `{-1, -1}`.
  - Note: this stage only prints results to the console — actually creating `targetRel` as a new relation isn't implemented yet, since that's not permitted by the real spec and comes later.

### `Cache/OpenRelTable.cpp` (updated)

- `getRelId(relName)`: hardcoded to recognize only two names — the Relation Catalog and Attribute Catalog — returning their fixed IDs. Any other name returns `E_RELNOTOPEN`. Proper lookup of arbitrary relations (searching, opening, assigning a real cache slot) isn't implemented until a later stage.

### `Cache/AttrCacheTable.cpp` (updated)

- Added the name-based overload of `getAttrCatEntry(relId, attrName, attrCatBuf)`, alongside the existing offset-based one from Stage 3. Walks the same per-relation linked list, but matches on `attrName` via `strcmp` instead of a numeric offset — this is what `linearSearch` and `select` use to find an attribute by name rather than position.

## Key Data Structures

- **`RecId`** – a `{block, slot}` pair identifying exactly one record's location. What `linearSearch` returns.
- **`Attribute`** – the union already in use from earlier stages; here it's what condition values and record fields are compared as.
- **`searchIndex`** – per-relation state in `RelCacheTable` that makes `linearSearch` resumable across repeated calls.

## Outcome

- Can now search a relation for records matching a condition (`=`, `≠`, `<`, `≤`, `>`, `≥`) without manually touching blocks or slots.
- `SELECT` works, but **only on `RELATIONCAT` and `ATTRIBUTECAT`** — user-created relations like `Students` can't be opened or queried yet, since `getRelId` is still hardcoded to just the two catalogs.
- Results are printed straight to the console, not written into a new relation — that's a later-stage limitation, consistent with the spec note that direct console output isn't the final behavior.
- No changes to how relations are created, opened, or modified — this stage is purely about *reading* via search.