#include "OpenRelTable.h"

#include <cstring>
#include <cstdlib>

#define STUDENTS_RELID 2

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

//constructor
OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; i++) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
  }
  for (int i = 0; i < MAX_OPEN; i++) {
  tableMetaInfo[i].free = true;
  }
  /************ 1. Setting up Relation Cache entries ************/

  /**** a.setting up Relation Catalog relation in the Relation Cache Table****/
  RecBuffer relCatBlock(RELCAT_BLOCK);

  Attribute relCatRecord[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

  struct RelCacheEntry relCacheEntry;
  RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
  relCacheEntry.recId.block = RELCAT_BLOCK;
  relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

  RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

  /**** b.setting up Attribute Catalog relation in the Relation Cache Table ****/
  Attribute relCatRecord2[RELCAT_NO_ATTRS];
  relCatBlock.getRecord(relCatRecord2, RELCAT_SLOTNUM_FOR_ATTRCAT);

  struct RelCacheEntry relCacheEntry2;
  RelCacheTable::recordToRelCatEntry(relCatRecord2, &relCacheEntry2.relCatEntry);
  relCacheEntry2.recId.block = RELCAT_BLOCK;
  relCacheEntry2.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

  RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
  *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry2;

  /************ Setting up Attribute cache entries ************/

  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

  /**** setting up Relation Catalog relation in the Attribute Cache Table ****/
  AttrCacheEntry* headRelCat = nullptr;
  AttrCacheEntry* prevRelCat = nullptr;

  for (int i = 0; i < RELCAT_NO_ATTRS; i++) {
    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = i;
    attrCacheEntry->next = nullptr;

    if (headRelCat == nullptr) {
      headRelCat = attrCacheEntry;
    } else {
      prevRelCat->next = attrCacheEntry;
    }
    prevRelCat = attrCacheEntry;
  }

  AttrCacheTable::attrCache[RELCAT_RELID] = headRelCat;

  /**** setting up Attribute Catalog relation in the Attribute Cache Table ****/
  AttrCacheEntry* headAttrCat = nullptr;
  AttrCacheEntry* prevAttrCat = nullptr;

  for (int i = RELCAT_NO_ATTRS; i < RELCAT_NO_ATTRS + ATTRCAT_NO_ATTRS; i++) {
    attrCatBlock.getRecord(attrCatRecord, i);

    AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId.block = ATTRCAT_BLOCK;
    attrCacheEntry->recId.slot = i;
    attrCacheEntry->next = nullptr;

    if (headAttrCat == nullptr) {
      headAttrCat = attrCacheEntry;
    } else {
      prevAttrCat->next = attrCacheEntry;
    }
    prevAttrCat = attrCacheEntry;
  }

  AttrCacheTable::attrCache[ATTRCAT_RELID] = headAttrCat;

  /************ Setting up Students relation (Exercise Q1) ************/

  /**** find and cache the Students entry in the Relation Cache Table ****/
  struct HeadInfo relCatHeader;
  relCatBlock.getHeader(&relCatHeader);

  int studentsSlot = -1;
  Attribute studentsRelRecord[RELCAT_NO_ATTRS];

  for (int i = 0; i < relCatHeader.numEntries; i++) {
    Attribute tempRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(tempRecord, i);

    if (strcmp(tempRecord[RELCAT_REL_NAME_INDEX].sVal, "Students") == 0) {
      studentsSlot = i;
      memcpy(studentsRelRecord, tempRecord, sizeof(tempRecord));
      break;
    }
  }

  if (studentsSlot != -1) {
    struct RelCacheEntry studentsRelCacheEntry;
    RelCacheTable::recordToRelCatEntry(studentsRelRecord, &studentsRelCacheEntry.relCatEntry);
    studentsRelCacheEntry.recId.block = RELCAT_BLOCK;
    studentsRelCacheEntry.recId.slot = studentsSlot;

    RelCacheTable::relCache[STUDENTS_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[STUDENTS_RELID]) = studentsRelCacheEntry;

    /**** find and cache Students' attributes in the Attribute Cache Table ****/
    /**** scan across attribute catalog blocks in case it spans more than one ****/
    AttrCacheEntry* headStudents = nullptr;
    AttrCacheEntry* prevStudents = nullptr;

    int attrBlockNum = ATTRCAT_BLOCK;
    while (attrBlockNum != -1) {
      RecBuffer currAttrCatBlock(attrBlockNum);
      struct HeadInfo currAttrCatHeader;
      currAttrCatBlock.getHeader(&currAttrCatHeader);

      for (int i = 0; i < currAttrCatHeader.numEntries; i++) {
        Attribute attrRecord[ATTRCAT_NO_ATTRS];
        currAttrCatBlock.getRecord(attrRecord, i);

        if (strcmp(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Students") == 0) {
          AttrCacheEntry* entry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
          AttrCacheTable::recordToAttrCatEntry(attrRecord, &entry->attrCatEntry);
          entry->recId.block = attrBlockNum;
          entry->recId.slot = i;
          entry->next = nullptr;

          if (headStudents == nullptr) {
            headStudents = entry;
          } else {
            prevStudents->next = entry;
          }
          prevStudents = entry;
        }
      }

      attrBlockNum = currAttrCatHeader.rblock;  // -1 if no next block
    }

    AttrCacheTable::attrCache[STUDENTS_RELID] = headStudents;
  }
}

OpenRelTable::~OpenRelTable() {
  for (int relId = 0; relId <= STUDENTS_RELID; relId++) {
    // free relCache entry, if it exists
    if (RelCacheTable::relCache[relId] != nullptr) {
      free(RelCacheTable::relCache[relId]);
    }

    // free attrCache linked list, if it exists
    AttrCacheEntry* entry = AttrCacheTable::attrCache[relId];
    while (entry != nullptr) {
      AttrCacheEntry* temp = entry;
      entry = entry->next;
      free(temp);
    }
  }
}

/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  // search every occupied slot for a matching relation name
  for (int i = 0; i < MAX_OPEN; ++i) {
    if (!tableMetaInfo[i].free && strcmp(tableMetaInfo[i].relName, relName) == 0) {
      return i;
    }
  }
  return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {
  // traverse the tableMetaInfo array, skipping the first two slots since
  // RELCAT (0) and ATTRCAT (1) are permanently occupied
  for (int i = 2; i < MAX_OPEN; ++i) {
    if (tableMetaInfo[i].free) {
      return i;
    }
  }
  return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {
  // if the relation is already open, just return its existing rel-id
  int relId = OpenRelTable::getRelId(relName);
  if (relId != E_RELNOTOPEN) {
    return relId;
  }

  // find a free slot to open this relation into
  relId = OpenRelTable::getFreeOpenRelTableEntry();
  if (relId == E_CACHEFULL) {
    return E_CACHEFULL;
  }

  /****** find the relation's entry in the Relation Catalog ******/

  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);

  if (relcatRecId.block == -1 && relcatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }

  /****** load the relation's entry into relCache ******/

  RecBuffer relcatBlock(relcatRecId.block);
  Attribute relcatRecord[RELCAT_NO_ATTRS];
  relcatBlock.getRecord(relcatRecord, relcatRecId.slot);

  RelCacheEntry *relCacheEntry = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
  RelCacheTable::recordToRelCatEntry(relcatRecord, &relCacheEntry->relCatEntry);
  relCacheEntry->recId = relcatRecId;
  relCacheEntry->searchIndex = {-1, -1};

  RelCacheTable::relCache[relId] = relCacheEntry;

  /****** find and load every attribute of this relation into attrCache ******/

  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  Attribute attrcatRelNameAttr;
  strcpy(attrcatRelNameAttr.sVal, relName);

  AttrCacheEntry *head = nullptr;
  AttrCacheEntry *tail = nullptr;

  for (int i = 0; i < relCacheEntry->relCatEntry.numAttrs; ++i) {
    RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, attrcatRelNameAttr, EQ);

    if (attrcatRecId.block == -1 && attrcatRecId.slot == -1) {
      break;
    }

    RecBuffer attrcatBlock(attrcatRecId.block);
    Attribute attrcatRecord[ATTRCAT_NO_ATTRS];
    attrcatBlock.getRecord(attrcatRecord, attrcatRecId.slot);

    AttrCacheEntry *attrCacheEntry = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    AttrCacheTable::recordToAttrCatEntry(attrcatRecord, &attrCacheEntry->attrCatEntry);
    attrCacheEntry->recId = attrcatRecId;
    attrCacheEntry->next = nullptr;

    if (head == nullptr) {
      head = attrCacheEntry;
      tail = attrCacheEntry;
    } else {
      tail->next = attrCacheEntry;
      tail = attrCacheEntry;
    }
  }

  AttrCacheTable::attrCache[relId] = head;

  /****** update tableMetaInfo for this relId ******/

  tableMetaInfo[relId].free = false;
  strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}

int OpenRelTable::closeRel(int relId) {
  if (relId == RELCAT_RELID || relId == ATTRCAT_RELID) {
    return E_NOTPERMITTED;
  }

  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (tableMetaInfo[relId].free) {
    return E_RELNOTOPEN;
  }

  // free the memory allocated in the relation and attribute caches which was
  // allocated in the OpenRelTable::openRel() function
  free(RelCacheTable::relCache[relId]);

  AttrCacheEntry *entry = AttrCacheTable::attrCache[relId];
  while (entry != nullptr) {
    AttrCacheEntry *next = entry->next;
    free(entry);
    entry = next;
  }

  // update `tableMetaInfo` to set `relId` as a free slot
  // update `relCache` and `attrCache` to set the entry at `relId` to nullptr
  tableMetaInfo[relId].free = true;
  RelCacheTable::relCache[relId] = nullptr;
  AttrCacheTable::attrCache[relId] = nullptr;

  return SUCCESS;
}