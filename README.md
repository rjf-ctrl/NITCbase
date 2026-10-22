# Stage 10 - B+ Tree Search on Relations

## B+ Trees
- index == a set of (key, pointer) pairs that lets you find a record without scanning the whole disk
- fewer blocks accessed -> faster lookup
- **B+ tree** keeps itself **balanced**, ->  every lookup walks the same number of levels  & each node is one disk block.

- two types of nodes: 
1. *Internal Nodes* -> each entry (left child, key, right child) -> no record pointers
2. *Leaf Nodes* -> (key, record pointer) -> hold real data -> evry key in the tree appears in the leaf nodes (the key in internal nides is justa copy for navigation) -> all leaves are on the same level and are linked together in sorted order
 - since internal nodes have no record pointer, more keys fit per node -> shorter tree
 
> For a 1024-byte block with 6-byte pointers and 10-byte keys:
- **B+ tree internal node:** 10n + 6(n+1) ≤ 1024, so **63 keys**
- **B tree internal node** (each key also carries a record pointer): 22n + 6 ≤ 1024, so **46 keys**

- RULE: Each key in an internal node = the largest value in its left child.
- left side holds values ≤ key and the right side holds values > key

**Search for k**
- Start at the root, compare k with keys left to right.
- At the first key **≥ k**, go to its **left** child. If none, go to the **rightmost** child.
- Repeat until you hit a leaf, then scan it.
**Range query (e.g. > 25):** find 25 in its leaf, then follow the leaf links right.

**Inserting**
Insertion always happens in a **leaf**. Find the right leaf, then:
1. If the leaf has room, put the key in (keep it sorted). Done.
 2. If it’s full, **split the leaf**. Sort all the keys including the new one and give the first half to the old leaf and the second half to a new leaf. **Copy** the largest key of the left half up into the parent.
3. If the parent is now overfull, **split the parent**. Here the middle key **moves up** and is not kept below.
4. If the root splits, make a new root. This is the only way the tree grows taller.

This is the main difference to remember: a **leaf split copies** the key up, and an **internal split moves** it up.

---
## GOALS
- indexing improves search 
- nitcbase internal node size: 100, leaf node: 63
- each node, one disk block
- attribute catalog stores wether a particular attribute is indexed or not
- if yes, Rootblock field of attrCat contains the root block no, else-1
---
**Internal Index Block** 
- Max **100 keys** and **101 child pointers** (the right child of one key = the left child of the next).
- Block = **32-byte header** + keys and child pointers stored alternately.
- Header fields:
    - First 4 bytes: `INDINT`
    - `PBlock`: parent block number
    - `#Entries`: actual number of keys stored (max 100)
    - `LBlock`, `RBlock`, `#Attrs`, `#Slots`: unused here
- Accessed via the `IndInternal` class; each entry via `struct InternalEntry`.

**Leaf Index Block** (stores a leaf node)
- Max **63 indices**, each a 3-tuple `(attribute value, block number, slot number)`.
- Each index is **32 bytes** (last 8 unused).
- Block = **32-byte header** + indices.
- Header fields:
    - First 4 bytes: `INDLEAF`
    - `PBlock`: parent block number (always an **internal** index block)
    - `LBlock` / `RBlock`: left and right sibling leaves (the linked list)
    - `#Entries`: actual number of indices stored (max 63)
    - `#Attrs`, `#Slots`: unused
- Accessed via the `IndLeaf` class; each entry via `struct Index`.

**Both:** must be loaded into memory through the Buffer Layer before reading or modifying, and the header is held in `struct HeadInfo`.

---
#### 1. BlockAcces.cpp
##### search
	- get attrCache entry of searched attribute
	- if rootblock==-1 (not indexed attrbute) linearseach()
	- else BPlusTree::bPlusSearch
	- getRecord the result

#### 2. AttrCacheTable.cpp
##### getSearchIndex (relID, attrName)
	- outofbounds check, relation must be opne
	- loop thru attribute cache LL of that RelId till strcmp(attrName,entry->attrCatEntry.attrName) == 0 and return the seacrh index

##### getSearchIndex(relId, attroffset)
	- same as above just entry->attrCatEntry.offset == attrOffset

#### setSearchIndex (relId, attrName)
	- same as above just entry->searchIndex = *searchIndex;

#### setSearchIndex (relId, attrOffset)
	-same
#### llr implemntation of resetSearch index to {-1, -1}

#### Algebra.cpp
##### select
	- ONE UPDATE: now, when resetSearchIndex is done, we do both
		- RelCacheTable::resetSearchIndex(srcRelId);
		- AttrCacheTable::resetSearchIndex(srcRelId,attr);
	- since select uses search, we could call either linear search or bplussearch

#### BlockBuffer.cpp
##### constructors
	- IndBuffer, IndInternal, IndLeaf

##### IndInternal: getEntry
	- reads one entry (left child, key, right child) of an internal block into the caller's struct
	- index must be valid for an internal block (max 100 keys), else out of bound
	- entries are packed 20 bytes apart, not 24, because the right child of one key is the left child of the next (that's why 101 pointers for 100 keys)

##### IndInternal: setEntry
	- stub for now, writing entries comes in a later stage

##### IndLeaf: getEntry
	- reads one leaf entry (attr value, block#, slot#) = the key and the record-id it points to
	- index must be valid for a leaf (max 63 entries), else out of bound
		- NOTE: bound is the leaf limit, not the internal limit
	- each leaf entry is a fixed 32 bytes, so entry i sits at a fixed offset after the header
	- whole entry copied as-is into the caller's struct

##### IndLeaf: setEntry
	- stub for now


#### BPlusTree.cpp
##### bPlusSearch(relId, attrName, attrVal, op)
	- position of the previous hit lives in the attr cache's searchIndex (leaf block, index)

	- decide where to start
		- searchIndex == (-1,-1) -> first call, start at the root (no B+ tree -> (-1,-1))
		- else -> resume at the leaf entry right after the previous hit
			- if that runs past the leaf's entries, hop to the right sibling
			- no right sibling -> done

	- descend through internal nodes (only when starting from the root, a resumed search is already at a leaf)
		- NE / LT / LE -> always go left, matches sit at the left end of the leaves (NE needs the whole list anyway) (here we're always trying to get to the first leafnode)
		- EQ / GE / GT -> go to the left child of the first key that is >= attrVal (> for GT)
			- no such key -> rightmost child
		- left because the left child may still hold earlier matches

	- scan the leaf level
		- compare each entry to (op, attrVal), first match -> save its position in searchIndex, return its (block, slot)
		- EQ / LE / LT: values are ascending, so once an entry is bigger than attrVal, stop
		- only NE walks the leaf linked list. Other ops stop after the current leaf
		- nothing found -> (-1,-1)

	- tree traversal happens only on the first call, later calls just walk the leaf entries (same idea as the stage notes)

