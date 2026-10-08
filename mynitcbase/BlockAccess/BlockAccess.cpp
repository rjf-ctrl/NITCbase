#include "BlockAccess.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op){

    // get the previous search index of the relation relId from the relation cache
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId,&prevRecId);

    // let block and slot denote the record id of the record beign currently checked
    int block, slot;
    // if the current search index record is invalid
    if(prevRecId.block == -1 && prevRecId.slot == -1){
        // it means no hits from the previous search, search should start from the first record itself
        // get the first record block of the relation from the relation cache using getRelCatEntry()
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        // block = first record block of the relation
        // slot = 0
        block = relCatEntry.firstBlk;
        slot = 0;

    }
    else{
        // it means there is a hit from the previous search, should start from the record next to the search index record
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /*
        Now the following code will search for the next record in the relation
        that satisfies the given condition
        We will start from the record id (block, slot) and iterate over the
        remaining records of the relation
    */
    while( block != -1){
        // create a RecBuffer object for the block
        RecBuffer recBuffer(block);

        // get the record with id (block,slot) using RecBuffer::getRecord()
        // get header of the block using RecBuffer::getHeader()
        // get slot map of the block using RecBuffer::getSlotMap()
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        Attribute record[head.numAttrs];
        recBuffer.getRecord(record,slot);
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // if slot >= the number of slots per block
        if(slot >= head.numSlots){
            // update block = right block of block
            // update slot = 0
            block = head.rblock;
            slot = 0;
            continue;
        }
        /*
            my notes: if it is end of the record in a relation, this 
            loop will continue to reach the end of the block,
            eventually block = head.rblock = -1 and loop will terminate.
        */

        // if slot is free skip the loop
        // i.e. check if slot'th entry in slot map of block constains SLOT_UNOCCUPIED
        if(slotMap[slot] == SLOT_UNOCCUPIED){
            slot++;
            continue;
        }

        // now compare record's attribute value to the given attrVal as below:
        /*
            firstly get the attribute offset for the attrName attribute
            from the attribute cache entry of the relation using getAttrCatEntry
        */
        /* use the attribute offset to get the value of the attribute from
           current record */
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

        Attribute currentAttrVal = record[attrCatEntry.offset];
        
        // store the difference b/w the attributes.
        int cmpVal = compareAttrs(currentAttrVal, attrVal, attrCatEntry.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if(
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <=0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ){
            /*
                set the search index in the relation cache as
                the record id of the record that satisfies the given condition
                (use RelCacheTable::setSearchIndex function)
            */
            RecId recid = {block,slot};
            RelCacheTable::setSearchIndex(relId, &recid);

            return recid;
        }

        slot++;
    }

    // no record in the relation with Id relid satisfies the given condition
    return RecId{-1,-1};
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

/*
    insert(): function to insert a record into a relation
*/
int BlockAccess::insert(int relId, Attribute *record){
    // get the relation catalog entry from the relation cache table
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    //modification: print the number of blocks accessed while inserting the record into the relation
    //int accessedBlockCount = 0;

    int blockNum = relCatEntry.firstBlk; // first record block of the relation.

    // rec_id will be used to store where the new record will be inserted.
    RecId rec_id = {-1, -1};

    int numOfSlots = relCatEntry.numSlotsPerBlk;    // number of slots per record block
    int numOfAttributes = relCatEntry.numAttrs;     // number of attributes of the relation
    int prevBlockNum = -1;                          // block number of the last element in the linked list

    /*
        Traversing the linked list of existing record block of the relation
        until a free slot is found or until the end of the list is reached.
    */
    while(blockNum != -1){
        //get the slotmap of the block and search for a free slot.
        //accessedBlockCount++;
        RecBuffer recBuffer(blockNum);
        struct HeadInfo head;
        recBuffer.getHeader(&head);
        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // iterate over slotmap and store the free in the rec_id
        for(int i = 0; i < numOfSlots; i++){
            if(slotMap[i] == SLOT_UNOCCUPIED){
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        // if slot is found, then break the traversal.
        if(rec_id.block != -1 && rec_id.slot != -1){
            break;
        }

        // otherwise, continue to check the next block by updating the block number
        prevBlockNum = blockNum;
        blockNum = head.rblock;
    }

    // if slot not found
    if(rec_id.block == -1 && rec_id.slot == -1){
        // if relation is RELCAT, do not allocate any more blocks
        if(relId == RELCAT_RELID){
            return E_MAXRELATIONS; // DOUBTFULL
        }

        // else, get a new record block
        // get the block number of newly allocated block
        RecBuffer newRecBuffer;
        int ret = newRecBuffer.getBlockNum();

        if(ret == E_DISKFULL){
            return E_DISKFULL;
        }

        // Assign rec_id.block = new block number (i.e. ret) and rec_id.slot = 0
        rec_id.block = ret;
        rec_id.slot = 0;

        /*
            set the header of the new record block such that it links with the existing record
            blocks of the relation
         */
        struct HeadInfo newHead;
        newHead.blockType = REC;
        newHead.pblock = -1;
        newHead.lblock = prevBlockNum;
        newHead.rblock = -1;
        newHead.numEntries = 0;
        newHead.numSlots = numOfSlots;
        newHead.numAttrs = numOfAttributes;

        newRecBuffer.setHeader(&newHead);

        /*
            set block's slot map with all slots marked as free
        */
        unsigned char newSlotMap[numOfSlots];
        for(int i = 0; i < numOfSlots; i++){
            newSlotMap[i] = SLOT_UNOCCUPIED;
        }
        newRecBuffer.setSlotMap(newSlotMap);

        // now if there was a previous block, then it's rblock should be set as rec_id.block
        if(prevBlockNum != -1){
            // create a RecBuffer object for prevBlockNum
            // get the header and update the rblock field.
            // then set the header using setHeader()
            RecBuffer prevRecBuffer(prevBlockNum);
            struct HeadInfo prevHead;
            prevRecBuffer.getHeader(&prevHead);
            prevHead.rblock = rec_id.block;
            prevRecBuffer.setHeader(&prevHead);
        }
        else{
            // means it is the 1st block of the relation
            // update first block field in the relCatEntry to the new block
            RelCatEntry relCatEntry;
            RelCacheTable::getRelCatEntry(relId, &relCatEntry);
            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);
        }

        // update the lastBlk field of the relCatEntry to the new block
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry); // is it neccessary because we are setting it up again at the end of this function.
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    // Now create a RecBuffer object for rec_id.block
    // and insert the record into the rec_id.slot using the setRecord()
    RecBuffer recBuffer(rec_id.block); // i am using the old recBuffer I created initially.
    recBuffer.setRecord(record, rec_id.slot);

    /*
        update the slotmap of the block by making the entry of the slot to which
        record was inserted as occupied
        Get the slotMap using getSlotMap() ==> update the rec_id.slot-th index as OCCUPIED ==> set the slotMap using setSlotMap()
    */
    struct HeadInfo head;
    recBuffer.getHeader(&head);
    unsigned char slotMap[head.numSlots];
    recBuffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    recBuffer.setSlotMap(slotMap);

    /*
        increment the numEntries field in the header of the block to
        which record was inserted. Then set the header back using BlockBuffer::setHeader()
    */
    head.numEntries += 1;
    recBuffer.setHeader(&head);

    /*
        Increment the number of records field in the relation cache entry for the relation.
    */
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    relCatEntry.numRecs += 1;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    //std::printf("Number of blocks accessed while inserting the record into the relation: %d\n", accessedBlockCount);

    return SUCCESS;
    
}