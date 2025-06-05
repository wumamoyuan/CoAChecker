#ifndef _HASHMAP_H
#define _HASHMAP_H

typedef struct _HashNode {
    unsigned int hash;      // 哈希值
    void *key;              // 键
    void *value;            // 值
    struct _HashNode *next; // 下一个节点
} HashNode;

typedef unsigned int (*Hashcode)(void *key);     // 哈希函数类型
typedef int (*KeyEqual)(void *key1, void *key2); // 判等函数类型
typedef char *(*KeyToString)(void *key);
typedef char *(*ValueToString)(void *value);
typedef void (*DestructFn)(void *ptr);

typedef struct _HashMap {
    HashNode **table; // 哈希表
    int tableLen;
    int size;
    int keySize;
    int valueSize;
    int threshold;
    int loadFactor;
    Hashcode hashcode;
    KeyEqual equal;
    KeyToString keyToString;
    ValueToString valueToString;
    DestructFn destructKey;
    DestructFn destructValue;
} HashMap;

typedef struct _HashNodeIterator {
    int (*HasNext)(struct _HashNodeIterator *it);   // 是否还有下一个元素函数
    void *(*GetNext)(struct _HashNodeIterator *it); // 获取下一个元素函数
    void (*Remove)(struct _HashNodeIterator *it);   // 删除当前元素函数
    HashMap *hashMap;
    HashNode *current;
    HashNode *next;
    int index; // 当前索引
} HashNodeIterator;

typedef struct _HashMapInterface {
    HashMap *(*Create)(int keySize, int valueSize, Hashcode hashCode, KeyEqual keyEqual); // 创建哈希表函数
    int (*Put)(HashMap *hashMap, void *key, void *value);                                    // 添加键值对函数
    int (*ContainsKey)(HashMap *hashMap, void *key);                                         // 判断键是否存在函数
    void *(*Get)(HashMap *hashMap, void *key);                                               // 获取键值对函数
    void *(*GetOrDefault)(HashMap *hashMap, void *key, void *defaultValue);               // 获取键值对函数，若不存在则返回默认值
    int (*Remove)(HashMap *hashMap, void *key);                                         // 删除键值对函数
    void (*Clear)(HashMap *hashMap);                                                      // 清空哈希表函数
    int (*Size)(HashMap *hashMap);                                                        // 获取哈希表大小函数
    void (*Finalize)(HashMap *hashMap);                                                   // 释放哈希表函数
    KeyToString (*SetKeyToString)(HashMap *hashMap, KeyToString keyToString);             // 设置键转换为字符串函数
    ValueToString (*SetValueToString)(HashMap *hashMap, ValueToString valueToString);     // 设置值转换为字符串函数
    DestructFn (*SetDestructKey)(HashMap *hashMap, DestructFn destructKey);             // 设置键析构函数
    DestructFn (*SetDestructValue)(HashMap *hashMap, DestructFn destructValue);     // 设置值析构函数
    void (*DestructPointer)(void *pHashMap);
    HashNodeIterator *(*NewIterator)(HashMap *hashMap);                                   // 创建迭代器函数
    HashNodeIterator *(*DeleteIterator)(HashNodeIterator *it);                            // 删除迭代器函数
    int (*HashCode)(HashMap *hashMap);                                                    // 获取哈希值函数
    int (*Equal)(HashMap *hashMap, HashMap *hashMap2);                                    // 判断两个哈希表是否相等函数
} HashMapInterface;

extern HashMapInterface iHashMap; // 哈希表接口

#endif // _HASHMAP_H