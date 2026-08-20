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