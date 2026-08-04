#include <iostream>
#include <cstring>
#include <iostream>

#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

/*

int main(int argc, char *argv[]) {
    Disk disk_run;

    RecBuffer relCatBuffer(RELCAT_BLOCK);
    RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

    HeadInfo relCatHeader;
    HeadInfo attrCatHeader;

    relCatBuffer.getHeader(&relCatHeader);
    attrCatBuffer.getHeader(&attrCatHeader);

    for (int i = 0; i < relCatHeader.numEntries; i++) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        relCatBuffer.getRecord(relCatRecord, i);

        printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

        for (int j = 0; j < attrCatHeader.numEntries; j++) {
            Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
            attrCatBuffer.getRecord(attrCatRecord, j);

            if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                       relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
                const char *attrType =
                    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
                printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
            }
        }
        printf("\n");
    }

    return 0;
}
*/

//ASSIGNMENT 1

int main(int argc, char *argv[]) {
  Disk disk_run;

  // create objects for the relation catalog and its header
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);


  for (int i=0; i<relCatHeader.numEntries; i++) { //looping thru block 4

    Attribute relCatRecord[RELCAT_NO_ATTRS]; // stores relation, catalog record: a record is an array of attributes
    relCatBuffer.getRecord(relCatRecord, i);

    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    int currAttrBlock=ATTRCAT_BLOCK;
    while(currAttrBlock!=-1) {

        //objects for attribute catalog block
        //cant use a global RecBuffer as the attribute catalog spans multiple blocks
        RecBuffer currAttrBuff(currAttrBlock);
        HeadInfo currAttrCatHeader;
        currAttrBuff.getHeader(&currAttrCatHeader);

        for(int j=0; j<currAttrCatHeader.numEntries; j++){ //loopong thru current attribute catalog block

          Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
          currAttrBuff.getRecord(attrCatRecord, j);
          
          if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal,
                    relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0) {
            const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
            printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
          }
        }
  
      currAttrBlock=currAttrCatHeader.rblock; //move to next bloxk in linked list
    }
    printf("\n");
  }

  return 0;
}

/*ASSIGNMENT 2
int main(int argc, char *argv[]) {
  Disk disk_run;

  // create objects for the relation catalog and header
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;
  relCatBuffer.getHeader(&relCatHeader);

  for (int i=0; i<relCatHeader.numEntries; i++) { //looping thru reln catalog

    Attribute relCatRecord[RELCAT_NO_ATTRS]; // stores relation catalog record
    relCatBuffer.getRecord(relCatRecord, i);

    if(strcmp(relCatRecord[RELCAT_REL_NAME_INDEX].sVal, "Student")!=0) continue;

    // --- update: find "Class" attr of Student, rename to "Batch" ---
    int currAttrBlock=ATTRCAT_BLOCK;
    while(currAttrBlock!=-1) {

        RecBuffer currAttrBuff(currAttrBlock);
        HeadInfo currAttrCatHeader;
        currAttrBuff.getHeader(&currAttrCatHeader);

        for(int j=0; j<currAttrCatHeader.numEntries; j++){ //looping thru current attribute catalog block

          Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
          currAttrBuff.getRecord(attrCatRecord, j);

          if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Student")==0 &&
              strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Class")==0) {
            strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, "Batch");
            currAttrBuff.setRecord(attrCatRecord, j);
          }
        }

      currAttrBlock=currAttrCatHeader.rblock; //move to next block in linked list
    }

    // --- verification phase: reprint Student's full schema ---
    printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    currAttrBlock=ATTRCAT_BLOCK; // reuse, outer while has fully exited by now
    while(currAttrBlock!=-1) {

        RecBuffer currAttrBuff(currAttrBlock);
        HeadInfo currAttrCatHeader;
        currAttrBuff.getHeader(&currAttrCatHeader);

        for(int j=0; j<currAttrCatHeader.numEntries; j++){

          Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
          currAttrBuff.getRecord(attrCatRecord, j);

          if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, "Student")==0) {
            const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
            printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
          }
        }

      currAttrBlock=currAttrCatHeader.rblock;
    }

    break;
  }

  return 0;
}
*/ 