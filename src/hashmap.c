#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMUM_CAPACITY 0x40000000
#define DEFAULT_LOAD_FACTOR 0.75
#define DEFAULT_INITIAL_CAPACITY 16
#define TREEIFY_THRESHOLD 8
#define MAX_INTEGER 0x7FFFFFFF

// static int numberOfLeadingZeros(int i) {
//     // HD, Count leading 0's
//     if (i <= 0)
//         return i == 0 ? 32 : 0;
//     int n = 31;
//     if (i >= 1 << 16) {
//         n -= 16;
//         i = (unsigned int)i >> 16;
//     }
//     if (i >= 1 << 8) {
//         n -= 8;
//         i = (unsigned int)i >> 8;
//     }
//     if (i >= 1 << 4) {
//         n -= 4;
//         i = (unsigned int)i >> 4;
//     }
//     if (i >= 1 << 2) {
//         n -= 2;
//         i = (unsigned int)i >> 2;
//     }
//     return n - ((unsigned int)i >> 1);
// }

// /**
//  * Returns a power of two size for the given target capacity.
//  */
// static int tableSizeFor(int cap) {
//     int n = -1 >> numberOfLeadingZeros(cap - 1);
//     return (n < 0) ? 1 : (n >= MAXIMUM_CAPACITY) ? MAXIMUM_CAPACITY
//                                                  : n + 1;
// }

/**
 * Default hash function. Calculates the hash code of the adress of the given key.
 * @param key The key to hash.
 * @return The hash code of the key.
 */
static unsigned int DefualtHashCode(void *key) {
    int pointerSize = sizeof(void *);
    char *keyPtr = (char *)&key;
    unsigned int hash = 0;
    int i;
    for (i = 0; i < pointerSize; i++) {
        hash = (hash << 5) - hash + keyPtr[i]; // hash * 31 + key[i]
    }
    return hash;
}

/**
 * Default equality function. Compares the two keys for equality.
 * @param key1 The first key to compare.
 * @param key2 The second key to compare.
 * @return 1 if the keys are equal, 0 otherwise.
 */
static int DefaultEqual(void *key1, void *key2) {
    return key1 == key2;
}

static char *DefaultElementToString(void *keyOrValue) {
    int pointerSize = sizeof(void *);
    char *keyPtr = (char *)&keyOrValue;
    unsigned int hash = 0;
    int i;
    for (i = 0; i < pointerSize; i++) {
        hash = (hash << 5) - hash + keyPtr[i]; // hash * 31 + key[i]
    }
    char *hashStr = (char *)malloc(16);
    strcpy(hashStr, "hash@");
    snprintf(hashStr + 5, sizeof(hashStr), "%u", hash);
    return hashStr;
}

/**
 * Returns the size of the hash map.
 *
 * @param hashMap The hash map to get the size of.
 * @return The size of the hash map.
 */
int Size(HashMap *hashMap) {
    return hashMap->size;
}

static HashNode *newNode(unsigned int hash, void *key, int keySize, void *value, int valueSize, HashNode *next) {
    HashNode *node = (HashNode *)malloc(sizeof(HashNode));
    node->hash = hash;
    char *keyCopy = (char *)malloc(keySize);
    memcpy(keyCopy, key, keySize);
    node->key = keyCopy;

    char *valueCopy = (char *)malloc(valueSize);
    memcpy(valueCopy, value, valueSize);
    node->value = valueCopy;

    node->next = next;
    return node;
}

/**
 * Initializes or doubles table size.  If null, allocates in
 * accord with initial capacity target held in field threshold.
 * Otherwise, because we are using power-of-two expansion, the
 * elements from each bin must either stay at same index, or move
 * with a power of two offset in the new table.
 *
 * @return The new table.
 */
static HashNode **resize(HashMap *hashMap) {
    HashNode **oldTab = hashMap->table;
    int oldCap = (oldTab == NULL) ? 0 : hashMap->tableLen;
    int oldThr = hashMap->threshold;
    int newCap, newThr = 0;
    if (oldCap > 0) {
        if (oldCap >= MAXIMUM_CAPACITY) {
            hashMap->threshold = MAX_INTEGER;
            return oldTab;
        } else if ((newCap = oldCap << 1) < MAXIMUM_CAPACITY &&
                   oldCap >= DEFAULT_INITIAL_CAPACITY) {
            // double threshold
            newThr = oldThr << 1;
        }
    } else if (oldThr > 0) {
        // initial capacity was placed in threshold
        newCap = oldThr;
    } else {
        // zero initial threshold signifies using defaults
        newCap = DEFAULT_INITIAL_CAPACITY;
        newThr = (int)(DEFAULT_LOAD_FACTOR * DEFAULT_INITIAL_CAPACITY);
    }
    if (newThr == 0) {
        float ft = (float)newCap * hashMap->loadFactor;
        newThr = (newCap < MAXIMUM_CAPACITY && ft < (float)MAXIMUM_CAPACITY ? (int)ft : MAX_INTEGER);
    }
    hashMap->threshold = newThr;
    HashNode **newTab = (HashNode **)malloc(newCap * sizeof(HashNode *));
    memset(newTab, 0, newCap * sizeof(HashNode *));
    hashMap->table = newTab;
    hashMap->tableLen = newCap;
    if (oldTab != NULL) {
        int j;
        for (j = 0; j < oldCap; ++j) {
            HashNode *e;
            if ((e = oldTab[j]) != NULL) {
                oldTab[j] = NULL;
                if (e->next == NULL)
                    newTab[e->hash & (newCap - 1)] = e;
                else { // preserve order
                    HashNode *loHead = NULL, *loTail = NULL;
                    HashNode *hiHead = NULL, *hiTail = NULL;
                    HashNode *next;
                    do {
                        next = e->next;
                        if ((e->hash & oldCap) == 0) {
                            if (loTail == NULL)
                                loHead = e;
                            else
                                loTail->next = e;
                            loTail = e;
                        } else {
                            if (hiTail == NULL)
                                hiHead = e;
                            else
                                hiTail->next = e;
                            hiTail = e;
                        }
                    } while ((e = next) != NULL);
                    if (loTail != NULL) {
                        loTail->next = NULL;
                        newTab[j] = loHead;
                    }
                    if (hiTail != NULL) {
                        hiTail->next = NULL;
                        newTab[j + oldCap] = hiHead;
                    }
                }
            }
        }
        free(oldTab);
    }
    return newTab;
}

static int putVal(HashMap *hashMap, unsigned int hash, void *key, void *value, int onlyIfAbsent, int evict) {
    HashNode **tab;
    HashNode *p;
    int n, i;
    if ((tab = hashMap->table) == NULL || (n = hashMap->tableLen) == 0) {
        tab = resize(hashMap);
        n = hashMap->tableLen;
    }
    if ((p = tab[i = (n - 1) & hash]) == NULL)
        tab[i] = newNode(hash, key, hashMap->keySize, value, hashMap->valueSize, NULL);
    else {
        HashNode *e;
        void *k;
        if (p->hash == hash &&
            ((k = p->key) == key || (key != NULL && hashMap->equal(key, k))))
            e = p;
        else {
            int binCount;
            for (binCount = 0;; ++binCount) {
                if ((e = p->next) == NULL) {
                    p->next = newNode(hash, key, hashMap->keySize, value, hashMap->valueSize, NULL);
                    /*if (binCount >= TREEIFY_THRESHOLD - 1)
                        //change the link list to a red-black tree
                        treeifyBin(tab, hash);*/
                    break;
                }
                if (e->hash == hash &&
                    ((k = e->key) == key || (key != NULL && hashMap->equal(key, k))))
                    break;
                p = e;
            }
        }
        if (e != NULL) {
            // existing mapping for key
            void *oldValue = e->value;
            if (!onlyIfAbsent || oldValue == NULL) {
                memcpy(e->value, value, hashMap->valueSize);
                return 1;
            }
            // afterNodeAccess(e);
            return 0;
        }
    }
    //++modCount;
    if (++hashMap->size > hashMap->threshold)
        resize(hashMap);
    // afterNodeInsertion(evict);
    return 0;
}

static HashNode *getNode(HashMap *hashMap, void *key) {
    HashNode **tab;
    HashNode *first, *e;
    int n, hash;
    void *k;
    if ((tab = hashMap->table) != NULL && (n = hashMap->tableLen) > 0 &&
        (first = tab[(n - 1) & (hash = hashMap->hashcode(key))]) != NULL) {
        if (first->hash == hash &&
            ((k = first->key) == key || (key != NULL && hashMap->equal(key, k))))
            return first;
        if ((e = first->next) != NULL) {
            do {
                if (e->hash == hash &&
                    ((k = e->key) == key || (key != NULL && hashMap->equal(key, k))))
                    return e;
            } while ((e = e->next) != NULL);
        }
    }
    return NULL;
}

/****************************************************************************************************
 * 功能：删除节点
 * 参数：
 *      @hashMap[in]: 哈希表
 *      @hash[in]: 哈希值
 *      @key[in]: 键
 *      @value[in]: 值
 *      @matchValue[in]: 是否匹配值
 *      @movable[in]: 是否可移动
 * 返回值：
 *      1表示删除成功，0表示删除失败
 ***************************************************************************************************/
static int removeNode(HashMap *hashMap, unsigned int hash, void *key, void *value, int matchValue, int movable) {
    HashNode **tab;
    HashNode *p;
    int n, index;
    if ((tab = hashMap->table) != NULL && (n = hashMap->tableLen) > 0 &&
        (p = tab[index = (n - 1) & hash]) != NULL) {
        HashNode *node = NULL, *e;
        void *k, *v;
        if (p->hash == hash &&
            ((k = p->key) == key || (key != NULL && hashMap->equal(key, k))))
            node = p;
        else if ((e = p->next) != NULL) {
            do {
                if (e->hash == hash &&
                    ((k = e->key) == key ||
                     (key != NULL && hashMap->equal(key, k)))) {
                    node = e;
                    break;
                }
                p = e;
            } while ((e = e->next) != NULL);
        }
        // 如果key与value的类型不一样，判断是否相等的方式不一样，那么可能存在bug，因为hashMap->equal函数是用来判断key是否相等的
        if (node != NULL && (!matchValue || (v = node->value) == value ||
                             (value != NULL && hashMap->equal(value, v)))) {
            if (node == p)
                tab[index] = node->next;
            else
                p->next = node->next;
            //++modCount;
            --hashMap->size;
            // afterNodeRemoval(node);
            if (hashMap->destructKey != NULL) {
                hashMap->destructKey(node->key);
            }
            free(node->key);
            if (hashMap->destructValue != NULL) {
                hashMap->destructValue(node->value);
            }
            free(node->value);
            free(node);
            return 1;
        }
    }
    return 0;
}

static HashMap *Create(int keySize, int valueSize, Hashcode hashcode, KeyEqual keyEqual) {
    HashMap *hashMap = (HashMap *)malloc(sizeof(HashMap));
    hashMap->size = 0;
    hashMap->keySize = keySize;
    hashMap->valueSize = valueSize;
    hashMap->threshold = 0;
    hashMap->loadFactor = DEFAULT_LOAD_FACTOR;
    hashMap->table = NULL;
    hashMap->hashcode = hashcode ? hashcode : DefualtHashCode;
    hashMap->equal = keyEqual ? keyEqual : DefaultEqual;
    hashMap->keyToString = DefaultElementToString;
    hashMap->valueToString = DefaultElementToString;
    hashMap->destructKey = NULL;
    hashMap->destructValue = NULL;
    return hashMap;
}

static int Put(HashMap *hashMap, void *key, void *value) {
    return putVal(hashMap, hashMap->hashcode(key), key, value, 0, 0);
}

static int ContainsKey(HashMap *hashMap, void *key) {
    return getNode(hashMap, key) != NULL;
}

static void *Get(HashMap *hashMap, void *key) {
    HashNode *e;
    return (e = getNode(hashMap, key)) == NULL ? NULL : e->value;
}

static void *GetOrDefault(HashMap *hashMap, void *key, void *defaultValue) {
    HashNode *e;
    return (e = getNode(hashMap, key)) == NULL ? defaultValue : e->value;
}

static int RemoveKey(HashMap *hashMap, void *key) {
    return removeNode(hashMap, hashMap->hashcode(key), key, NULL, 0, 1);
}

static void Clear(HashMap *hashMap) {
    int i = 0;
    HashNode **tab = hashMap->table, *e, *next;
    if (tab != NULL && hashMap->size > 0) {
        hashMap->size = 0;
        for (i = 0; i < hashMap->tableLen; ++i) {
            e = tab[i];
            if (e != NULL) {
                do {
                    next = e->next;
                    if (hashMap->destructKey != NULL) {
                        hashMap->destructKey(e->key);
                    }
                    free(e->key);
                    if (hashMap->destructValue != NULL) {
                        hashMap->destructValue(e->value);
                    }
                    free(e->value);
                    free(e);
                    e = next;
                } while (e != NULL);
                tab[i] = NULL;
            }
        }
    }
}

static void Finalize(HashMap *hashMap) {
    if (hashMap == NULL) {
        return;
    }
    Clear(hashMap);
    free(hashMap->table);
    hashMap->table = NULL;
    free(hashMap);
}

static int HasNext(HashNodeIterator *it) {
    return it->next != NULL;
}

static void *GetNext(HashNodeIterator *it) {
    HashNode **t;
    HashNode *e = it->next;
    if (e == NULL) {
        printf("No next element\n");
        abort();
        return NULL;
    }
    int tableLen = it->hashMap->tableLen;
    if ((it->next = (it->current = e)->next) == NULL && (t = it->hashMap->table) != NULL) {
        do {
        } while (it->index < tableLen && (it->next = t[it->index++]) == NULL);
    }
    return e;
}

static void RemoveCurrent(HashNodeIterator *it) {
    HashNode *p = it->current;
    if (p == NULL) {
        fprintf(stderr, "Illegal state exception\n");
        abort();
    }
    it->current = NULL;
    removeNode(it->hashMap, p->hash, p->key, NULL, 0, 0);
}

static HashNodeIterator *NewIterator(HashMap *hashMap) {
    HashNode **t = hashMap->table;
    HashNodeIterator *it = (HashNodeIterator *)malloc(sizeof(HashNodeIterator));
    it->hashMap = hashMap;
    it->current = NULL;
    it->next = NULL;
    it->index = 0;
    if (t != NULL && hashMap->size > 0) {
        do {
        } while (it->index < hashMap->tableLen && (it->next = t[it->index++]) == NULL);
    }
    it->HasNext = HasNext;
    it->GetNext = GetNext;
    it->Remove = RemoveCurrent;
    return it;
}

static HashNodeIterator *DeleteIterator(HashNodeIterator *it) {
    if (it != NULL) {
        free(it);
        it = NULL;
    }
    return it;
}

static KeyToString SetKeyToString(HashMap *hashMap, KeyToString keyToString) {
    KeyToString oldKeyToString = hashMap->keyToString;
    hashMap->keyToString = keyToString ? keyToString : DefaultElementToString;
    return oldKeyToString;
}

static ValueToString SetValueToString(HashMap *hashMap, ValueToString valueToString) {
    ValueToString oldValueToString = hashMap->valueToString;
    hashMap->valueToString = valueToString ? valueToString : DefaultElementToString;
    return oldValueToString;
}

static DestructFn SetDestructKey(HashMap *hashMap, DestructFn destructKey) {
    DestructFn oldDestructKey = hashMap->destructKey;
    hashMap->destructKey = destructKey;
    return oldDestructKey;
}

static DestructFn SetDestructValue(HashMap *hashMap, DestructFn destructValue) {
    DestructFn oldDestructValue = hashMap->destructValue;
    hashMap->destructValue = destructValue;
    return oldDestructValue;
}

static void DestructHashMapPointer(void *pHashMap) {
    Finalize(*(HashMap **)pHashMap);
}

static int HashMapHashCode(HashMap *hashMap) {
    // int hash = 1;
    // HashNodeIterator *it = NewIterator(hashMap);
    // HashNode *node;
    // while (it->HasNext(it))
    // {
    //     node = (HashNode *)it->GetNext(it);
    //     hash = 31 * hash + node->hash;
    // }
    // DeleteIterator(it);
    // return hash;
    printf("HashMapHashCode has not been implemented\n");
    abort();
}

static int HashMapEqual(HashMap *hashMap, HashMap *hashMap2) {
    // return hashMap->equal(hashMap, hashMap2);
    printf("HashMapEqual has not been implemented\n");
    abort();
}

HashMapInterface iHashMap = {
    .Create = Create,
    .Put = Put,
    .ContainsKey = ContainsKey,
    .Get = Get,
    .GetOrDefault = GetOrDefault,
    .Remove = RemoveKey,
    .Clear = Clear,
    .Size = Size,
    .Finalize = Finalize,
    .HashCode = HashMapHashCode,
    .Equal = HashMapEqual,
    .SetKeyToString = SetKeyToString,
    .SetValueToString = SetValueToString,
    .SetDestructKey = SetDestructKey,
    .SetDestructValue = SetDestructValue,
    .DestructPointer = DestructHashMapPointer,
    .NewIterator = NewIterator,
    .DeleteIterator = DeleteIterator};