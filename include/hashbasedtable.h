#ifndef _HASHBASEDTABLE_H
#define _HASHBASEDTABLE_H

#include "hashmap.h"

typedef struct _HashBasedTable {
    HashMap *pRowMap;
    int colKeySize;
    int valueSize;
    Hashcode colHashCode;
    KeyEqual colKeyEqual;
    DestructFn destructCol;
    DestructFn destructValue;
} HashBasedTable;

typedef struct _HashBasedTableInterface {
    HashBasedTable *(*Create)(int rowKeySize, int colKeySize, int valueSize, Hashcode rowHashCode, KeyEqual rowKeyEqual, Hashcode colHashCode, KeyEqual colKeyEqual);
    int (*Put)(HashBasedTable *hashTable, void *rowKey, void *colKey, void *value);
    void *(*Get)(HashBasedTable *hashTable, void *rowKey, void *colKey);
    HashMap *(*GetRow)(HashBasedTable *hashTable, void *rowKey);
    DestructFn (*SetDestructRow)(HashBasedTable *hashTable, DestructFn destructRow);
    DestructFn (*SetDestructCol)(HashBasedTable *hashTable, DestructFn destructCol);
    DestructFn (*SetDestructValue)(HashBasedTable *hashTable, DestructFn destructValue);
    void (*Clear)(HashBasedTable *hashTable);
    void (*Finalize)(HashBasedTable *hashTable);
} HashBasedTableInterface;

extern HashBasedTableInterface iHashBasedTable;

#endif //HASHBASEDTABLE_H