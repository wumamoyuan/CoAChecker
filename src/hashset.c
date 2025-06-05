#include "hashset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *defaultValue = "0";

static HashSet *Create(int valueSize, Hashcode hashCode, KeyEqual keyEqual) {
    return iHashMap.Create(valueSize, sizeof(char), hashCode, keyEqual);
}

static int Add(HashSet *hashSet, void *newval) {
    return !iHashMap.Put(hashSet, newval, defaultValue);
}

static int Size(HashSet *hashSet) {
    return iHashMap.Size(hashSet);
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
    return e->key;
}

static HashSetIterator *NewIterator(HashSet *hashSet) {
    HashSetIterator *it = iHashMap.NewIterator(hashSet);
    it->GetNext = GetNext;
    return it;
}

static HashSetIterator *DeleteIterator(HashSetIterator *it) {
    return iHashMap.DeleteIterator(it);
}

static int Contains(HashSet *hashSet, void *element) {
    return iHashMap.ContainsKey(hashSet, element);
}

/**
 * This implementation iterates over the fisrt collection, checking each
 * element returned by the iterator in turn to see if it's contained
 * in the second collection.  If it's not so contained, it's removed
 * from the first collection with the iterator's {@code remove} method.
 *
 * @param hashSet1 the collection to be retained
 * @param hashSet2 the collection to be retained in the first
 *
 * @return 1 if the collection was modified, 0 otherwise
 */
static int RetainAll(HashSet *hashSet1, HashSet *hashSet2) {
    int modified = 0;
    HashSetIterator *it = NewIterator(hashSet1);
    while (it->HasNext(it)) {
        if (!Contains(hashSet2, it->GetNext(it))) {
            it->Remove(it);
            modified = 1;
        }
    }
    DeleteIterator(it);
    return modified;
}

static int Remove(HashSet *hashSet, void *pval) {
    return iHashMap.Remove(hashSet, pval);
}

static void Clear(HashSet *hashSet) {
    return iHashMap.Clear(hashSet);
}

static int containsAll(HashSet *hashSet1, HashSet *hashSet2) {
    int ret = 1;
    HashSetIterator *it = NewIterator(hashSet2);
    while (it->HasNext(it)) {
        if (!Contains(hashSet1, it->GetNext(it))) {
            ret = 0;
            break;
        }
    }
    iHashMap.DeleteIterator(it);
    return ret;
}

static unsigned int HashSetHashCode(void *hashSet) {
    HashSet *hs = *(HashSet **)hashSet;
    int hash = 1;
    HashSetIterator *it = NewIterator(hs);
    while (it->HasNext(it)) {
        hash = 31 * hash + hs->hashcode(it->GetNext(it));
    }
    iHashMap.DeleteIterator(it);
    return hash;
}

static int HashSetEqual(void *ppSet1, void *ppSet2) {
    if (ppSet1 == ppSet2) {
        return 1;
    }
    HashSet *hs1 = *(HashSet **)ppSet1;
    HashSet *hs2 = *(HashSet **)ppSet2;
    if (iHashSet.Size(hs1) != iHashSet.Size(hs2)) {
        return 0;
    }
    return containsAll(hs1, hs2);
}

/*static size_t GetElementSize(HashSet *hashSet)
{
    return S->keySize;
}*/

static void Finalize(HashSet *hashSet) {
    iHashMap.Finalize(hashSet);
}

static char *ToString(void *pHashSet) {
    HashSet *hashSet = *(HashSet **)pHashSet;
    HashSetIterator *it = iHashSet.NewIterator(hashSet);
    char *e;
    char *ret = (char *)malloc(3);
    int cur = 0;
    ret[cur++] = '[';
    int first = 1;
    int elementLen;
    while (it->HasNext(it)) {
        e = hashSet->keyToString(it->GetNext(it));
        elementLen = strlen(e);
        ret = (char *)realloc(ret, cur + elementLen + 4);
        if (first) {
            first = 0;
        } else {
            ret[cur++] = ',';
            ret[cur++] = ' ';
        }
        memcpy(ret + cur, e, elementLen);
        free(e);
        cur += elementLen;
    }
    ret[cur++] = ']';
    ret[cur++] = '\0';
    iHashSet.DeleteIterator(it);
    return ret;
}

static KeyToString SetElementToString(HashSet *hashSet, KeyToString elementToString) {
    return iHashMap.SetKeyToString(hashSet, elementToString);
}

static DestructFn SetDestructElement(HashSet *hashSet, DestructFn destructElement) {
    return iHashMap.SetDestructKey(hashSet, destructElement);
}

static void DestructHashSetPointer(void *pHashSet) {
    Finalize(*(HashSet **)pHashSet);
}

HashSetInterface iHashSet = {
    .Create = Create,
    .Add = Add,
    .Contains = Contains,
    .containsAll = containsAll,
    .HashCode = HashSetHashCode,
    .Equal = HashSetEqual,
    .Size = Size,
    .RetainAll = RetainAll,
    .Remove = Remove,
    .Clear = Clear,
    //.GetElementSize = GetElementSize,
    .Finalize = Finalize,
    .ToString = ToString,
    .SetElementToString = SetElementToString,
    .SetDestructElement = SetDestructElement,
    .DestructPointer = DestructHashSetPointer,
    .NewIterator = NewIterator,
    .DeleteIterator = DeleteIterator,
};