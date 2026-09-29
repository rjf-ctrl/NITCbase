#include "BlockAccess.h"


#include <cstring>


RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
  RecId prevRecId;
  // get the previous search index of the relation relId from the relation cache
  RelCacheTable::getSearchIndex(relId, &prevRecId);

  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);

  int block, slot;

  if (prevRecId.block == -1 && prevRecId.slot == -1) {
    // no hits from previous search; start from the first record itself
    block = relCatEntry.firstBlk;
    slot = 0;
  } else {
    // there is a hit from previous search; continue from the next record
    block = prevRecId.block;
    slot = prevRecId.slot + 1;
  }

  while (block != -1) {
    RecBuffer recBuffer(block);

    struct HeadInfo head;
    recBuffer.getHeader(&head);

    if (slot >= head.numSlots) {
      // no more slots in this block; move to next block
      block = head.rblock;
      slot = 0;
      continue;
    }

    unsigned char slotMap[head.numSlots];
    recBuffer.getSlotMap(slotMap);

    if (slotMap[slot] == SLOT_UNOCCUPIED) {
      slot++;
      continue;
    }

    union Attribute record[relCatEntry.numAttrs];
    recBuffer.getRecord(record, slot);

    // get the attribute offset for attrName from the attribute cache
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    int cmpVal = compareAttrs(record[attrCatEntry.offset], attrVal, attrCatEntry.attrType);

    if ((op == NE && cmpVal != 0) ||
        (op == LT && cmpVal < 0) ||
        (op == LE && cmpVal <= 0) ||
        (op == EQ && cmpVal == 0) ||
        (op == GT && cmpVal > 0) ||
        (op == GE && cmpVal >= 0)) {
      RecId searchIndex = {block, slot};
      RelCacheTable::setSearchIndex(relId, &searchIndex);
      return RecId{block, slot};
    }

    slot++;
  }

  return RecId{-1, -1};
}



int BlockAccess::renameRelation(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){

    // reset the search index of the relation catalog
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute newRelationName; // set newRelationName with newName
    strcpy(newRelationName.sVal, newRelName);

    // search the relation catalog for an entry with "RelName" = newRelationName
    char *relCatAttrRelName;
    strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, newRelationName, EQ);
    // check if relation already exist
    if(recId.block != -1 && recId.slot != -1){
        return E_RELEXIST;
    }

    // reset the search index of the relation catalog again
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute oldRelationName; // set oldRelationName with oldName
    strcpy(oldRelationName.sVal, oldRelName);

    // search the relation catalog for an entry with "RelName" = oldRelationName
    recId = BlockAccess::linearSearch(RELCAT_RELID, relCatAttrRelName, oldRelationName, EQ);
    // check if relation does not exist
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }

    /*
        get the relation catalog record of the relation to rename using RecBuffer on
        the relation catalog [RELCAT_BLOCK] and RecBuffer.gerRecord function.
    */
    Attribute record[RELCAT_NO_ATTRS];
    RecBuffer recBuffer(RELCAT_BLOCK);
    recBuffer.getRecord(record,recId.slot);

    /*
        update the relation name attribute in the record with newName
        (use RELCAT_REL_NAME_INDEX)
    */
    // then set back the reord value using RecBuffer.setRecord()
    strcpy(record[RELCAT_REL_NAME_INDEX].sVal, newRelName);
    recBuffer.setRecord(record, recId.slot);

    /*
        update all the attribute catalog entries in the attribute catalog corresponding
        to the relation with relation name oldName to the relation name newName.
    */

    //  reset the search index of the attribute catalog
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
            // Relation 'Student' can have 10 attributes, thus Attribute Catalog will have 10 entries correspoding to 'Student'
    int numOfAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    char *attrCatAttrRelName;
    strcpy(attrCatAttrRelName, ATTRCAT_ATTR_RELNAME);
    for(int i = 0; i < numOfAttrs; i++){
        RecId rec_id = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatAttrRelName, oldRelationName, EQ);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];
        RecBuffer attrBuffer(rec_id.block);
        attrBuffer.getRecord(attrRecord, rec_id.slot);

        attrRecord[ATTRCAT_REL_NAME_INDEX] = newRelationName;
        attrBuffer.setRecord(attrRecord, rec_id.slot);
    }

    return SUCCESS;
    
}


int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){

    // 1.   first of all we should check whether the given relation exist or not

    /* reset the searchIndex of the relation catalog*/
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; // set relNameAtrr to relName
    strcpy(relNameAttr.sVal, relName);
    
    // Search for the relation with name relName in the Relation Catalog using linearSearch()
    char *relCatAttrRelName;
    strcpy(relCatAttrRelName, RELCAT_ATTR_RELNAME);
    RecId recId = BlockAccess::linearSearch(RELCAT_RELID,relCatAttrRelName,relNameAttr, EQ);

    // check if it doesn't exist
    if(recId.block == -1 && recId.slot == -1){
        return E_RELNOTEXIST;
    }

    /* reset the searchIndex of the attribute catalog */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    RecId attrToRenameRecId;
    attrToRenameRecId.block = -1;
    attrToRenameRecId.slot = -1;
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    /* 
        1   Iterating over all the entries in the attribute catalog to find relation with relname = relName (paramter)
        2.  If found then the check if attrname == oldAttrname and take action accordingly.
    */
    char *attrCatAttrRelName;
    strcpy(attrCatAttrRelName, ATTRCAT_ATTR_RELNAME);
    while(true){
        // linearSearch
        RecId rec_id = BlockAccess::linearSearch(ATTRCAT_RELID, attrCatAttrRelName, relNameAttr, EQ);

        // if there is no more attribute left, then break
        if(rec_id.block == -1 && rec_id.slot == -1){
            break;
        }

        /* Get the record from the attribute catalog using RecBuffer.getRecord() */
        RecBuffer attrBuffer(rec_id.block);
        attrBuffer.getRecord(attrCatEntryRecord, rec_id.slot);

        // if attrCatEntryRecord's attrName == oldName
        //      set attrToRenameRecId with rec_id
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0){
            attrToRenameRecId.block = rec_id.block;
            attrToRenameRecId.slot = rec_id.slot;
        }

        // if attrCatEntryRecord.attrName = newName
        //      return E_ATTREXIST
        if(strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0){
            return E_ATTREXIST;
        }
    }

    // if attrToRenameRecId = {-1,-1} => E_ATTRNOTEXIST
    if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1){
        return E_ATTRNOTEXIST;
    }

    // Update the entry corresponding to the attribute in the Attribute Catalog Relation.
    RecBuffer attrToRenameBuff(attrToRenameRecId.block);
    attrToRenameBuff.getRecord(attrCatEntryRecord, attrToRenameRecId.slot);
    strcpy(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName);
    // set back the record with RecBuffer.setRecord
    attrToRenameBuff.setRecord(attrCatEntryRecord, attrToRenameRecId.slot);

    return SUCCESS;
}