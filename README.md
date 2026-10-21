# Stage 9: Selection and Projection on Relations
### GOALS
Previous stages:  linear search , rudimentary version of SELECT
Now: Complete implentation fo SELECT + PROJECTION

 NITCbase specification 0f 
 SELECT : selecting records from a relation that satisfy a specific condition and writing those records into a newly created relation of a specified name.
  PROJECT : pick a subset of columns from the relation,  result in the creation of a new relation with a subset of the attributes of the source relation. 


Following commands added

|Frontend User Interface Command|Operation|
|---|---|
|SELECT * FROM RelName INTO TargetName WHERE Attribute `op` value|selection|
|SELECT * FROM RelName INTO TargetName|projection (clone relation)|
|SELECT Attr1,Attr2 FROM RelName INTO TargetName|projection|
|SELECT Attr1,Attr2 FROM RelName INTO TargetName WHERE Attr `op` value|selection **and** projection|
---

#### 1. Frontend.cpp
	- select_from_table -> project(relname_source, relname_target);
	- select_attrlist_from_table -> project(relname_source, relname_target, attr_count, attr_list);
	- select_from_table_where -> select(relname_source, relname_target, attribute, op, value);
	- select_attrlist_from_table_where -> select(relname_source, TEMP, attribute, op, value), then openRel(TEMP), then project(TEMP, relname_target, attr_count, attr_list), then closeRel(tempRelId) and Schema::deleteRel(TEMP)

#### 2. Algebra.cpp
##### select
	OLD:
	- get RelId os the relation (OpenRelTable : getRelId)
	- get attrCat entry for the attribute in comparison (AttrCacheTable:getAttrcatEntry)
	- do appropriate type conversion of the value to compare against
	- get RelCacheTable: RelCateEntry (use it to get no. of attributes )
	- print a header for the table
	- loop: 
			- linearsearch, print each time RecId is returned

	NEW:
	- get RelId os the relation (OpenRelTable : getRelId)
	- get attrCat entry for the attribute in comparison (AttrCacheTable:getAttrcatEntry)
	- do appropriate type conversion of the value to compare against
	- create and open target relation:
		- create a RelCatEntry for it, set numEntries, same as srcRel
		- make a 2d array with attributes of srcRel and 1d array with their types
		- then Schema:creatRel(targetRelName, src_nAttr, attr_names, attr_type)
		- if it fails deleteRel
		- then OpenRel(targetRelName)
	- Loop:
		- BlockAcess:search, insert the record into TargetRel insert()
		- then close targetRel

##### project  (no attributes listed)
	- open srcRel, get RelCatEntry and get numAttrs
	- make 2d, 1d copy of attribute and and type
	- create and open target rel using these
	- loop: 
		- if project(srcRelId, record) is successful: insert record into targetRel
	- close targetRel
##### project (with a list of particular attributes (overload))
	- open srcRel, get RelCatEntry and get numAttrs
	- loop thru the passed list of attributes, getAttrCateEntry using their names, and make a list of attr_types and attr_offsets of those attrs
	- Create and open target relation
	- Loop:
		- if project(srcRelId, record) is successful, INSERT ONLY ATTRIBUTES OF THAT RECORD corresponding to attr_offset into targetRel
	- close TargetRel

#### BlockAcess.cpp 

##### project(relId, record)  //just returns the next full row in the table
	- get the previous search index (RecId) of the relation from the relation cache
	- if it is {-1, -1} (search was reset), start from the first record block of the relation, slot 0
	- else continue from the same block, slot + 1
	- loop from (block, slot) to find the next occupied slot:
		- if slot >= numSlots, move to the next block (rblock), slot = 0
		- else if slotMap[slot] is SLOT_UNOCCUPIED, slot++
		- else, found, break
	- if block == -1, all records are over, return E_NOTFOUND
	- set the search index to the found RecId, so the next call continues after it
	- get the record at that RecId into `record` (always the full record), return SUCCESS

//here we're using set search index to remeber the last valid recird that we accessed


## EXCERCISE: 1
CREATE TABLE Toys(id NUM, name STR, colour STR, stock NUM)
OPEN TABLE Toys
INSERT INTO Toys VALUES FROM toys.csv
SELECT name,colour FROM Toys INTO ToyColours
SELECT * FROM Toys INTO ToysForSale WHERE stock > 3000
SELECT * FROM Toys INTO ToysToReorder WHERE stock <= 1000
SELECT * FROM Toys INTO ToysCopy
SELECT id,name FROM Toys INTO BlueToys WHERE colour = blue


