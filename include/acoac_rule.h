#ifndef _ACoACRule_H
#define _ACoACRule_H

#include "ccl/containers.h"
#include "hashset.h"

/* The comparison operator. {=, !=, <, >, <=, >=} */
typedef enum {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    GREATER_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN_OR_EQUAL
} comparisonOperator;

/* The type of an attribute. {boolean, string, integer} */
typedef enum {
    BOOLEAN,
    STRING,
    INTEGER,
} AttrType;

/* An atomic condition, consisting of an attribute, a value, and a comparison operator. */
typedef struct {
    int attribute;
    int value;
    comparisonOperator op;
} AtomCondition;

/* A rule, consisting of an administrator condition, a user condition, a target attribute, and a target value. */
typedef struct _Rule {
    HashSet *adminCond;
    HashSet *userCond;
    int targetAttrIdx;
    int targetValueIdx;

    // The map from an attribute to the set of its values that can satisfy the user condition
    HashMap *pmapUserCondValue;
} Rule;

typedef struct _AtomConditionInterface {
    int (*Evaluate)(AtomCondition *atomCond, int valIdx);
    void (*FindEffectiveValues)(AtomCondition *atomdCond, HashSet *domain, HashSet *effectiveValues);
    unsigned int (*HashCode)(void *pAtomCond);
    int (*Equal)(void *pAtomCond1, void *pAtomCond2);
} AtomConditionInterface;

extern AtomConditionInterface iAtomCondition;

typedef struct _RuleInterface {
    Rule *(*Create)(HashSet *adminCond, HashSet *userCond, int targetAttrIdx, int targetValueIdx);
    int (*DiscreteCond)(Rule *r, HashMap *reachableAVs);
    int (*CanBeManaged)(Rule *r, HashMap *userState);
    int (*IsEffective)(Rule *r, HashMap *reachableAVs);
    unsigned int (*HashCode)(void *pRule);
    int (*Equal)(void *pRule1, void *pRule2);
} RuleInterface;

extern RuleInterface iRule;

#endif // _ACoACRule_H