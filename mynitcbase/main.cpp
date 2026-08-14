#include "Buffer/StaticBuffer.h"
#include "Buffer/BlockBuffer.h"
#include "Cache/RelCacheTable.h"
#include "Cache/AttrCacheTable.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"

#include <cstdio>

#define STUDENTS_RELID 2

int main(int argc, char *argv[]) {
  Disk disk_run;
  StaticBuffer buffer;
  OpenRelTable cache;

  for (int relId = RELCAT_RELID; relId <= STUDENTS_RELID; relId++) {
    RelCatEntry relCatEntry;
    
    int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    if (ret != SUCCESS) {
      continue;  // relation not cached (e.g. Students not created yet)
    }

    printf("Relation: %s\n", relCatEntry.relName);

    for (int j = 0; j < relCatEntry.numAttrs; j++) {
      AttrCatEntry attrCatEntry;
      ret = AttrCacheTable::getAttrCatEntry(relId, j, &attrCatEntry);
      if (ret != SUCCESS) {
        continue;
      }

      const char *attrType = (attrCatEntry.attrType == NUMBER) ? "NUM" : "STR";
      printf("  %s: %s\n", attrCatEntry.attrName, attrType);
    }
    printf("\n");
  }

  return 0;
}