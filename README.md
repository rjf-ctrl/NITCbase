# Stage 12 - Join on Relations
## Goals
- NITCBase allows a combo of join and pproject in a single command to join and save in a target relation with specified attribuets
- SELECT * FROM Rel1 JOIN Rel2 INTO TargetRel WHERE Rel1.Attr1 = Rel2.Attr2
- SELECT Attr1,Attr2 FROM Rel1 JOIN Rel2 INTO TargetRel WHERE Rel1.Attr1 = Rel2.Attr2

---

## Algebra.cpp
#### join(screl1, screl2, targterel, attr1, attr2)
	- relId1 = getRelId(srcRelation1), relId2 = getRelId(srcRelation2)
	- if either relId is out of range (<0 or >= MAX_OPEN), return E_RELNOTOPEN   //both relations must be open
	- get attrCatEntry1 (relId1, attribute1) and attrCatEntry2 (relId2, attribute2)
		- if either returns E_ATTRNOTEXIST, return E_ATTRNOTEXIST
	- if attrCatEntry1.attrType != attrCatEntry2.attrType, return E_ATTRTYPEMISMATCH
	- get relCatEntry1 and relCatEntry2 (need numAttrs of both)
	- check for duplicate attribute names in the target
		- for j over attributes of rel2, skip j == attrCatEntry2.offset   //attribute2 is dropped from the target
			- for i over attributes of rel1
				- if attrName of rel2[j] == attrName of rel1[i], return E_DUPLICATEATTR
	- numOfAttributes1 = rel1.numAttrs, numOfAttributes2 = rel2.numAttrs
	- if attrCatEntry2.rootBlock == -1   //no index on the inner relation's join attribute
		- ret = bPlusCreate(relId2, attribute2)
		- if ret != SUCCESS, return E_DISKFULL   //inner lookups become O(log n) instead of a full scan
	- numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1
	- build targetRelAttrNames[] and targetRelAttrTypes[]
		- all attributes of rel1, in order
		- attributes of rel2 before attribute2, placed at numOfAttributes1 + j
		- attributes of rel2 after attribute2, placed at numOfAttributes1 + j - 1   //shifted left by one
	- ret = Schema::createRel(targetRelation, numOfAttributesInTarget, names, types)
		- if ret != SUCCESS, return ret
	- targetRelId = OpenRelTable::openRel(targetRelation)
		- if targetRelId < 0 || >= MAX_OPEN
			- Schema::deleteRel(targetRelation)
			- return targetRelId
	- declare record1[numOfAttributes1], record2[numOfAttributes2], targetRecord[numOfAttributesInTarget]
	- RelCacheTable::resetSearchIndex(relId1)
	- while BlockAccess::project(relId1, record1) == SUCCESS   //outer loop, one rel1 record at a time
		- RelCacheTable::resetSearchIndex(relId2)
		- AttrCacheTable::resetSearchIndex(relId2, attribute2)   //restart the search for each rel1 record
		- while BlockAccess::search(relId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS   //inner loop, every rel2 match
			- targetRecord = record1 (all attrs)
			- then record2 attrs before attribute2, at numOfAttributes1 + i
			- then record2 attrs after attribute2, at numOfAttributes1 + i - 1
			- ret = BlockAccess::insert(targetRelId, targetRecord)
			- if ret != SUCCESS
				- closeRel(targetRelId), Schema::deleteRel(targetRelation)   //undo the half-built result
				- return E_DISKFULL
	- closeRel(targetRelId)
	- return SUCCESS