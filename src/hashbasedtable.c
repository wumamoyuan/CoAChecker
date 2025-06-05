#include <stdio.h>
#include <stdlib.h>

#include "hashbasedtable.h"

static HashBasedTable *Create(int rowKeySize, int colKeySize, int valueSize, Hashcode rowHashCode, KeyEqual rowKeyEqual, Hashcode colHashCode, KeyEqual colKeyEqual) {
    HashBasedTable *hashTable = (HashBasedTable *)malloc(sizeof(HashBasedTable));
    hashTable->pRowMap = iHashMap.Create(rowKeySize, sizeof(HashMap *), rowHashCode, rowKeyEqual);
    iHashMap.SetDestructValue(hashTable->pRowMap, iHashMap.DestructPointer);

    hashTable->colKeySize = colKeySize;
    hashTable->valueSize = valueSize;
    hashTable->colHashCode = colHashCode;
    hashTable->colKeyEqual = colKeyEqual;
    hashTable->destructCol = NULL;
    hashTable->destructValue = NULL;
    return hashTable;
}

static int Put(HashBasedTable *hashTable, void *rowKey, void *colKey, void *value) {
    HashMap *pColMap, **ppColMap = iHashMap.Get(hashTable->pRowMap, rowKey);
    if (ppColMap == NULL) {
        pColMap = iHashMap.Create(hashTable->colKeySize, hashTable->valueSize, hashTable->colHashCode, hashTable->colKeyEqual);
        if (hashTable->destructValue != NULL) {
            iHashMap.SetDestructValue(pColMap, hashTable->destructValue);
        }
        ppColMap = &pColMap;
        iHashMap.Put(hashTable->pRowMap, rowKey, ppColMap);
    }
    return iHashMap.Put(*ppColMap, colKey, value);
}

static void *Get(HashBasedTable *hashTable, void *rowKey, void *colKey) {
    HashMap **ppColMap = iHashMap.Get(hashTable->pRowMap, rowKey);
    if (ppColMap == NULL) {
        return NULL;
    }
    return iHashMap.Get(*ppColMap, colKey);
}

static HashMap *GetRow(HashBasedTable *hashTable, void *rowKey) {
    HashMap **ppColMap = (HashMap **)iHashMap.Get(hashTable->pRowMap, rowKey);
    return ppColMap == NULL ? NULL : *ppColMap;
}

static DestructFn SetDestructRow(HashBasedTable *hashTable, DestructFn destructRow) {
    DestructFn oldDestructRow = hashTable->pRowMap->destructKey;
    hashTable->pRowMap->destructKey = destructRow;
    return oldDestructRow;
}
static DestructFn SetDestructCol(HashBasedTable *hashTable, DestructFn destructCol) {
    DestructFn oldDestructCol = hashTable->destructCol;
    hashTable->destructCol = destructCol;
    return oldDestructCol;
}

static DestructFn SetDestructValue(HashBasedTable *hashTable, DestructFn destructValue) {
    DestructFn oldDestructValue = hashTable->destructValue;
    hashTable->destructValue = destructValue;
    return oldDestructValue;
}

static void Clear(HashBasedTable *hashTable) {
    iHashMap.Clear(hashTable->pRowMap);
}

static void Finalize(HashBasedTable *hashTable) {
    iHashMap.Finalize(hashTable->pRowMap);
    free(hashTable);
}

HashBasedTableInterface iHashBasedTable = {
    .Create = Create,
    .Put = Put,
    .Get = Get,
    .GetRow = GetRow,
    .SetDestructRow = SetDestructRow,
    .SetDestructCol = SetDestructCol,
    .SetDestructValue = SetDestructValue,
    .Clear = Clear,
    .Finalize = Finalize};