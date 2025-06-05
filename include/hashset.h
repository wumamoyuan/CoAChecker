#ifndef _HASHSET_H
#define _HASHSET_H

#include "hashmap.h"

typedef HashMap HashSet;

typedef HashNodeIterator HashSetIterator;

typedef struct _HashSetInterface {
    HashSet *(*Create)(int valueSize, Hashcode hashCode, KeyEqual keyEqual); // 创建HashSet
    int (*Add)(HashSet *hashSet, void *newval);
    int (*Contains)(HashSet *hashSet, void *element);
    int (*containsAll)(HashSet *hashSet1, HashSet *hashSet2);
    unsigned int (*HashCode)(void *hashSet);
    int (*Equal)(void *hashSet1, void *hashSet2);
    int (*Size)(HashSet *hashSet);
    int (*RetainAll)(HashSet *hashSet1, HashSet *hashSet2); // 交集
    int (*Remove)(HashSet *hashSet, void *pVal);            // 删除元素函数
    void (*Clear)(HashSet *hashSet);
    void (*Finalize)(HashSet *hashSet);
    char *(*ToString)(void *pHashSet); // 转换为字符串函数
    // size_t (*GetElementSize)(const HashSet *hashSet);
    KeyToString (*SetElementToString)(HashSet *hashSet, KeyToString elementToString); // 设置键转换为字符串函数
    DestructFn (*SetDestructElement)(HashSet *hashSet, DestructFn destructElement); // 设置键析构函数
    void (*DestructPointer)(void *pHashSet);
    HashSetIterator *(*NewIterator)(HashSet *hashSet);
    HashSetIterator *(*DeleteIterator)(HashSetIterator *it);
} HashSetInterface;

extern HashSetInterface iHashSet;

#endif // _HASHSET_H