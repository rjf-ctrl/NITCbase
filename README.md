# Stage 11 - Index Creation and Deletion

## Goals
- CREATE INDEX command - create index on any attribute of any relation
- DROP INDEX to delelte it
- NITCBase doesnt allow indexing on AttrCat and RelCat

---
## Cache Layer
- when index created, RootBlock of the attribute is updated with block number
- this is updated in cache and then written back to buffer when relation is closed
- so, need *attribute cache update and write back* 

#### AttrCacheTabel.cpp
##### setAttrCatEntry(relId, attrName, attrCatBuff)
	- check if relId valid(>=0&&<=MAX_OPEN)
	- check if relation open (attrCache[relId]!=NULL)
	- loop thru the linked list of atttributes til relName=attrCate[relId].relName
		- set the AttrCateEntry to attrCatBuff
		- set dirty=true
##### setAttrCatEntry(relId, attrOffset, attrCatBuff)
	- same with offset

## Buffer Layer
- need to update IndLeaf and IndInternal classes 
- implement setEntry for them, previously we implemnted a getEntry for them

#### BlockBuffer.cpp
##### IndInternal::setEntry(void* ptr, indexNum)
	- check if indexNum is in range (>=0 && <=MAX_KEYS_INTERNAL)
	- loadBlockAndGetBufferPtr
	- entryPtr = bufferPtr + HEADER_SIZE + (indexNum*20)
	- copy 4 bytes of lChild, ATTR_SIZE(16 bytes) of Child, 4 bytes of Child
	- setDirtyBit(this->blockNum)

##### IndLead::setEntry(void* ptr, int indexNum)
	- check if indexNum is in range (>=0 && <=MAX_KEYS_LEAF)
	- loadBlockAndGetBufferPtr
	- entryPtr = bufferPtr + HEADER_SIZE + (indexNum*32)
	- copy 32 bytes of LEAF ENTRY
	- setDirtyBit(this->blockNum)

## Frontend Layer
#### Frontend.cpp
##### create_index(relName, attrName) 
	- Schema Layer::createIndex
##### drop_index(relName, attrName)
	- Schema::dropIndex


## Schema Layer
#### Schema.cpp
##### createIndex()
	- AttrCat and RelCat not permitted
	- getRelId
	- make sure its open
	- return BPlusTree::bPlusCreate(relId, attrName)

##### dropIndex()
	- Attrcat and RelCat not permitted 
	- make sure its open
	- get the corresponding attrCatEbtry
	- check if it has an index
	- BPlusTree::bPlusDestroy
	- rootBlock=-1, setAttrCatEntry



## BPlusTree
#### BPlusTree.cpp
##### bPlusCreate(relId, AttrName)
	- relCat, attrCat not permitted
	- get attrCatEntry of attrName
	- check if index already exists
	- create Indleaf rootBlockBuf; //gets a free leaf block
	- get its blocknumber, rootBlock=rootBlockBuf.getBlockNum();
	- check for DISKFULL
	- set rootblock in attrCacheTable
	- load relCatEntry, get firstBlk
	-  while(block!=-1) loop through the blocks of the relation
		- get the block, slotmap
		- loop through every slot, get record bPlusInsert(relId, attrName, record, recid), if DIKS_FULL return DISK_FULL
		- getHeader, rblock

##### bPlusInsert(relId, attrName, attrVal, recId)
	- get AttrCatEntry
	- get rootBlk
	- get LeafBlkNumber = findLeafToInsert(rootblock, attrVal, attrType)
	- the insertIntoLeaf() function will propagate the insertion to the required internal nodes by calling the required helper functions like insertIntoInternal() or createNewRoot()
	- insertIntoLeaf(relId, attrName, leafBlkNumber, indexEntry)
	- (indexentry is s struct with attrval, block and slot)
	- if InsertIntoLeaf return E_DISKFULL then bPlusDestroy, set rootBlock-1 in attrCache

##### findLeafToInsert(rootBlock, attrVal, attrType)
	- blockNum = rootBlock
	- while(getStaticBlockType(blockNum) != IND_LEAF)  {walk down internal blocks}
		- IndInternal internalBlock(blockNum), getHeader
		- index = 0
		- while(index < numEntries)
			- getEntry(index)
			- if compareAttrs(attrVal, entry.attrVal) <= 0, break   
			- (we're trying to find and attrVal that's just greater than our attrVal)
			- index++
		- if index == numEntries (no such entry)
			- blockNum = rChild of the last entry (numEntries - 1)
		- else
			- blockNum = lChild of entry at index
	- return blockNum   //the leaf where attrVal belongs


##### insertIntoLeaf(relId, attrName, leafBlockNum, indexEntry)
	- get attrCatEntry 
	- IndLeaf leafBlock(leafBlockNum), getHeader
	- Index indices[numEntries + 1]   //existing entries + the new one
	
	- loop i over existing entries, copy them into indices[] in sorted order
		- getEntry(i)
		- if compareAttrs(entry.attrVal, indexEntry.attrVal) <= 0
			- indices[i] = entry   //entry is smaller or equal, goes before the new one
		- else
			- indices[i] = indexEntry, inserted = true   //new one goes here
			- copy all the remaining entries one slot to the right (indices[j] = entry j-1)
			- break
	- if !inserted, indices[numEntries] = indexEntry   //new one is the biggest, goes at the end
	
	- if numEntries < MAX_KEYS_LEAF   //room in the block, no split
		- numEntries++, setHeader
		- write all indices[] back with setEntry
		- return SUCCESS
		
	- newRightBlk = splitLeaf(leafBlockNum, indices)   //block is full, split into two leaves
	- if newRightBlk == E_DISKFULL, return E_DISKFULL
	- if pblock != -1   //leaf has a parent
		- middleEntry {attrVal = indices[MIDDLE_INDEX_LEAF].attrVal, lChild = leafBlockNum, rChild = newRightBlk}
		- return insertIntoInternal(relId, attrName, pblock, middleEntry)
	- else   //leaf was the root
		- if createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, leafBlockNum, newRightBlk) == E_DISKFULL
			- return E_DISKFULL
	- return SUCCESS

##### splitLeaf(leafBlockNum, indices)
	
	- IndLeaf rightBlock           //constructor 1, grabs a brand new free leaf block
	- IndLeaf leftBlock(leafBlockNum)   //the existing full leaf
	- rightBlockNum = rightBlock.getBlockNum(), leftBlockNum = leftBlock.getBlockNum()
	- if rightBlockNum == E_DISKFULL, return E_DISKFULL   //no block for the new leaf
	- getHeader of both leftBlock and rightBlock
	- set up rightBlock header
		- numEntries = (MAX_KEYS_LEAF + 1) / 2 = 32
		- pblock = left's pblock   //same parent as the left block
		- lblock = leftBlockNum    //left block is to its left
		- rblock = left's rblock   //takes over whatever was to the right of left
		- setHeader
	- set up leftBlock header
		- numEntries = 32
		- rblock = rightBlockNum   //new block is now its right neighbour
		- setHeader
	- for i = 0 to MIDDLE_INDEX_LEAF (0..31)
		- leftBlock.setEntry(indices[i], i)                         //first 32 go left
		- rightBlock.setEntry(indices[i + MIDDLE_INDEX_LEAF + 1], i)  //next 32 (32..63) go right
	- return rightBlockNum

##### insertIntoInternal(relId, attrName, intBlockNum, intEntry)
	
	- get attrCatEntry of attrName 
	- IndInternal internalBlock(intBlockNum), getHeader
	- InternalEntry internalEntries[numEntries + 1]   //existing entries + the new one
	- insertedIndex = -1
	- loop i over existing entries, copy them into internalEntries[] in sorted order
		- getEntry(i)
		- if compareAttrs(entry.attrVal, intEntry.attrVal) <= 0
			- internalEntries[i] = entry   //entry is smaller or equal, goes before the new one
		- else
			- internalEntries[i] = intEntry, insertedIndex = i   //new one goes here
			- copy all the remaining entries one slot to the right (internalEntries[j] = entry j-1)
			- break
	- if insertedIndex == -1   //new one is the biggest, goes at the end
		- internalEntries[numEntries] = intEntry, insertedIndex = numEntries
	- fix the child pointers around the new entry
		- if insertedIndex > 0, internalEntries[insertedIndex-1].rChild = intEntry.lChild
		- if insertedIndex < numEntries, internalEntries[insertedIndex+1].lChild = intEntry.rChild
	- if numEntries < MAX_KEYS_INTERNAL   //room in the block, no split
		- numEntries++, setHeader
		- write all internalEntries[] back with setEntry
		- return SUCCESS
	- newRightBlk = splitInternal(intBlockNum, internalEntries)   //block is full, split into two internal blocks
	- if newRightBlk == E_DISKFULL
		- bPlusDestroy(intEntry.rChild)   //subtree built so far isn't attached to the tree yet
		- return E_DISKFULL
	- if pblock != -1   //block has a parent
		- middleEntry {attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, lChild = intBlockNum, rChild = newRightBlk}
		- return insertIntoInternal(relId, attrName, pblock, middleEntry)   //recursion up the tree
	- else   //block was the root
		- return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk)

##### splitInternal(intBlockNum, internalEntries)
	- IndInternal rightBlock           //constructor 1, grabs a brand new free internal block
	- IndInternal leftBlock(intBlockNum)   //the existing full internal block
	- leftBlockNum = leftBlock.getBlockNum(), rightBlockNum = rightBlock.getBlockNum()
	- if rightBlockNum == E_DISKFULL, return E_DISKFULL   //no block for the new right half
	- getHeader of both leftBlock and rightBlock
	- set up rightBlock header
		- numEntries = MAX_KEYS_INTERNAL / 2 = 50
		- pblock = left's pblock   //same parent as the left block
		- setHeader
	- set up leftBlock header
		- numEntries = 50
		- setHeader   //no rblock update, internal blocks aren't linked
	- for i = 0 to MIDDLE_INDEX_INTERNAL - 1 (0..49)
		- leftBlock.setEntry(internalEntries[i], i)                              //entries 0..49 stay left
		- rightBlock.setEntry(internalEntries[i + MIDDLE_INDEX_INTERNAL + 1], i)  //entries 51..100 go right
		- (entry 50 goes up to the parent, in neither block)
	- fix pblock of every child of the new right block   //they now live under rightBlock, not leftBlock
		- BlockBuffer child(internalEntries[MIDDLE_INDEX_INTERNAL + 1].lChild)   //leftmost child of right block
			- getHeader, pblock = rightBlockNum, setHeader
		- for i = 0 to MIDDLE_INDEX_INTERNAL - 1
			- BlockBuffer child(internalEntries[i + MIDDLE_INDEX_INTERNAL + 1].rChild)
			- getHeader, pblock = rightBlockNum, setHeader
	- return rightBlockNum

##### createNewRoot(relId, attrName, attrVal, lChild, rChild)
	
	- get attrCatEntry 
	- IndInternal newRootBlock
	- newRootBlkNum = newRootBlock.getBlockNum()
	- if newRootBlkNum == E_DISKFULL
		- bPlusDestroy(rChild)   //right subtree isn't attached to the tree yet, free it
		- return E_DISKFULL
	- getHeader of newRootBlock, numEntries = 1, setHeader
	- build InternalEntry {lChild, attrVal, rChild} from the arguments
	- newRootBlock.setEntry(entry, 0)    //the only entry of the new root
	- fix pblock of both children   //they now live under the new root
		- BlockBuffer leftChildBlock(lChild), BlockBuffer rightChildBlock(rChild)
		- getHeader of each, pblock = newRootBlkNum, setHeader of each
	- attrCatEntry.rootBlock = newRootBlkNum
	- setAttrCatEntry(relId, attrName, attrCatEntry)   //tree now starts at the new root
	- return SUCCESS

##### bPlusDestroy(rootBlockNum)

	- if rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS, return E_OUTOFBOUND
	- type = getStaticBlockType(rootBlockNum)
	- if type == IND_LEAF   //base case
		- IndLeaf leafBlock(rootBlockNum)
		- leafBlock.releaseBlock()
		- return SUCCESS
	- else if type == IND_INTERNAL   //recursive case
		- IndInternal internalBlock(rootBlockNum), getHeader
		- getEntry(0), bPlusDestroy(entry.lChild)   //leftmost child, only the first entry's lChild
		- for i = 0 to numEntries - 1
			- getEntry(i), bPlusDestroy(entry.rChild)   //every entry's rChild
		- internalBlock.releaseBlock()   //free this block after all its children are gone
		- return SUCCESS
	- else
		- return E_INVALIDBLOCK   //not an index block, refuse to free it

- **`BlockAccess::linearSearch`/`search`:** when the attribute has an index (`rootBlock != -1`), call `BPlusTree::bPlusSearch` instead of scanning linearly. The returned `RecId` is then used to read the record.
- **`BlockAccess::insert`:** after a record is inserted, add each indexed attribute’s value to its B+ tree with `bPlusInsert`.
- **`BlockAccess::deleteRelation`:** call `bPlusDestroy` on each attribute’s `rootBlock` before the relation is removed, so index blocks aren’t leaked.
- **`BlockAccess::renameAttribute` and related functions:** these don’t need index changes, as far as I remember.