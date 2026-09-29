#include "Schema.h"

#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]){
    int ret = OpenRelTable::openRel(relName);

    // OpenRelTable::openRel() will send rel-id [0,12].
    //If relation is not open, any error code will return negative integer.
    if(ret >= 0){
        return SUCCESS;
    }

    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]){
    // if relation in relation catalog or attribute catalog, the not permited to close
    if( strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0 ){
        return E_NOTPERMITTED;
    }

    int relId = OpenRelTable::getRelId(relName);
    if( relId == E_RELNOTOPEN){
        return E_RELNOTOPEN;
    }

    return OpenRelTable::closeRel(relId);
}

// renameRel(): method to change the relation name of specified relation to a new specified name.
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]){
    // oldRelName and newRelName should not be same as relation catalog or attribute catalog name.
    if(strcmp(oldRelName,RELCAT_RELNAME) == 0 || strcmp(newRelName,RELCAT_RELNAME) == 0 || strcmp(oldRelName,ATTRCAT_RELNAME) == 0 || strcmp(newRelName,ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // check if relation is closed or not
    // NOTE: Relation must be close to perform this function
    int relId = OpenRelTable:: getRelId(oldRelName);
    if(relId >= 0){
        return E_RELOPEN;
    }

    int retVal = BlockAccess:: renameRelation(oldRelName, newRelName);
    return retVal;
}

// renameAttr(): method used to change attrname of a given relation
// NOTE: Relation must be close to perfrom this operation.
int Schema::renameAttr(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]){
    // check if relName is not same as RELCAT_NAME and ATTRCAT_NAME
    if(strcmp(relName,RELCAT_RELNAME) == 0 || strcmp(relName,ATTRCAT_RELNAME) == 0){
        return E_NOTPERMITTED;
    }

    // check if relation is closed or not
    int relId = OpenRelTable::getRelId(relName);
    if(relId >=0){
        return E_RELOPEN;
    }

    int retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
    return retVal;
}