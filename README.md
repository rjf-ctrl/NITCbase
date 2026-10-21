# Stage 7: Record Insertion and Cache Write-back
## Goal

  - Previously: insert records into exising relations

- Now: Create and delete relations

  
- Creating a relation -> inserting records into the relation catalog and the attribute catalog

- Implemented in the Schema Layer and the Block Access Layer -> CREATE TABLE command.

 
- Deleting a relation -> DROP TABLE command.

- Freeing all the blocks used to store the record of the relation and removing all the records corresponding to the relation from the catalogs.

- Note: NITCbase only allows you to delete a relation as a whole and does not allow the deletion of individual records from a relation
---

### 1. Frontend. cpp 
	- create_table -> calls Schema Layer createRel(relname, no_attrs, attributes, type_attrs)
	- drop_table -> calls Schema Layer deleteRel(relname);

### 2.  Schema.cpp
##### creatRel 
	- check if relName exist already or not (linear search relaion catalog)
	- see if any attribute names submitted repeat (regular for loop)
	- create a record with the relation catalog, with relName, nAttrs, rest default
	- insert the record into relation catalog (use BlockAcess::insert())
	- loop and insert attribute catalog records with relName, attrName, attrType, rest default (BlockAccess:: insert())

##### deleteRel
	- if relname == attrCat or relCat, not allowed
	- check if relation opened (only delete closed relations)
	- call BlockAccess::deleteRelation
	
#### 3. OpenReltable.cpp

##### ~OpenRelTable
	Previously:
	- close all open relations from rel-id=2 (OpenRelTable::closeRel())
	- free relationCache and Attribute Cache
	
	Now Additionally:
	- before freeing, write back the relation cache entries of ATTRCAT (rel-id 1) and RELCAT (rel-id 0) if their dirty flag is set
	  - write it back with RecBuffer(recId.block).setRecord(relCatRecord, recId.slot)
	- then free relCache[ATTRCAT_RELID] and relCache[RELCAT_RELID]
	- free the attribute cache entries of rel-id 0 and 1 (no write-back needed, since the attributes of the catalogs never change)

#### 4. BlockBuffer.cpp

##### releaseBlock
	 - check for valid block number
	 - if block is in buffer, free the buffer : set metainfo.free=true
	 - then free blocj on disk(always) :set blockAllocMap=UNUSED_BLK
	 - mark the object as invalid(this->blockNum = INVALID_BLOCKNUM;)
#### 5. BlockAccess.cpp
##### search(int relId, Attribute  record, char attrName[ATTR_SIZE], Attribute attrVal, int op);
	- copy the result of a linearsearch into a "record" argument
	- linear search with the arguments passed
	- getRecord the corresponding returned RecId into record

##### deleteRel
	- cannot delete attrCat and RelCat
	- linearsearch RelCta for the relation and getRecord it
	- get firstBlk and no. ofattributes
	- go through linked list of blocks, delete each one with releaseBlock
	- loop:
		-linearsearch attrCat for attribute of the relation (break if done)
		- getheader adn getrecord for that RecId
		- update slotMap to unoccupied for that slot
		- decrement numEntries in block header (if it becoes 0, linked list delete the block: get left bloc and set rblock as rblock of current, if it is the lastBlk update RelCat Entry for LastBlk, then release block)
	- getHeader of RelCat, decrement no of entries, set slotmap unoccupied, update the same in relationCacheTable
	- updateattrCache by decrementing no. of entries by no. of attributes deleted


#### EXCERCISE 
tests: 
- Creates 4 relations (Products, Sales, Bills, Stores) and checks their RELCAT and ATTRCAT entries
- Opens and populates Products and Stores from CSVs, then queries Stores by owner
- Closes and drops Products, Sales, Bills
- Confirms their RELCAT entries are gone
- Overall: tests that `createRel` and `deleteRelation` keep the catalogs consistent