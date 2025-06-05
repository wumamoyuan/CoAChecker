#include <string.h>

#include "acoac_rule.h"
#include "acoac_utils.h"
#include "hashset.h"

static int Evaluate(AtomCondition *atomCond, int valIdx) {
    int valIdxInCond = atomCond->value;
    comparisonOperator op = atomCond->op;
    if (op == EQUAL) {
        return valIdx == valIdxInCond;
    }
    if (op == NOT_EQUAL) {
        return valIdx != valIdxInCond;
    }
    // if (strcmp(valIdx, "UNKNOWN") == 0)
    // {
    //     return 0;
    // }
    switch (op) {
    case LESS_THAN:
        return valIdx < valIdxInCond;
    case GREATER_THAN:
        return valIdx > valIdxInCond;
    case LESS_THAN_OR_EQUAL:
        return valIdx <= valIdxInCond;
    case GREATER_THAN_OR_EQUAL:
        return valIdx >= valIdxInCond;
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "Invalid comparison operator\n");
        exit(-1);
    }
}

/**
 * 在值域domain中寻找所有满足该条件的值effectiveValues
 * @param domain 查找范围
 * @return domain中满足该条件的值构成的集合
 */
static void FindEffectiveValues(AtomCondition *atomdCond, HashSet *domain, HashSet *effectiveValues) {
    HashSetIterator *it = iHashSet.NewIterator(domain);
    int valIdx;
    while (it->HasNext(it)) {
        valIdx = *(int *)it->GetNext(it);
        if (Evaluate(atomdCond, valIdx)) {
            iHashSet.Add(effectiveValues, &valIdx);
        }
    }
    iHashSet.DeleteIterator(it);
}

static void RetainEffectiveValues(AtomCondition *atomdCond, HashSet *effectiveValues) {
    HashSetIterator *it = iHashSet.NewIterator(effectiveValues);
    int valIdx;
    while (it->HasNext(it)) {
        valIdx = *(int *)it->GetNext(it);
        if (!Evaluate(atomdCond, valIdx)) {
            it->Remove(it);
        }
    }
    iHashSet.DeleteIterator(it);
}

static unsigned int AtomCondHashCode(void *obj) {
    AtomCondition *atomCond = (AtomCondition *)obj; //*(atomCondition **)pAtomCond;
    unsigned int hash = 1;
    hash = 31 * hash + atomCond->attribute;
    hash = 31 * hash + atomCond->op;
    hash = 31 * hash + atomCond->value;
    return hash;
}

static int AtomCondEqual(void *obj1, void *obj2) {
    AtomCondition *atomCond1 = (AtomCondition *)obj1; //*(atomCondition **)pAtomCond1;
    AtomCondition *atomCond2 = (AtomCondition *)obj2; //*(atomCondition **)pAtomCond2;
    if (atomCond1->op != atomCond2->op) {
        return 0;
    }
    if (atomCond1->attribute != atomCond2->attribute) {
        return 0;
    }
    if (atomCond1->value != atomCond2->value) {
        return 0;
    }
    return 1;
}

static unsigned int RuleHashCode(void *pRule) {
    if (pRule == NULL) {
        printf("[Error] RuleHashCode: rule is NULL\n");
        abort();
    }
    int hash = 1;
    Rule *r = (Rule *)pRule;

    hash = 31 * hash + iHashSet.HashCode(&r->adminCond);

    if (r->pmapUserCondValue == NULL) {
        hash = 31 * hash + iHashSet.HashCode(&r->userCond);
    } else {
        int hash2 = 1;
        HashNodeIterator *it = iHashMap.NewIterator(r->pmapUserCondValue);
        HashNode *node;
        while (it->HasNext(it)) {
            node = (HashNode *)it->GetNext(it);
            hash2 = 31 * hash2 + IntHashCode(node->key);
            hash2 = 31 * hash2 + iHashSet.HashCode(node->value);
        }
        iHashMap.DeleteIterator(it);
        hash = 31 * hash + hash2;
    }

    hash = 31 * hash + r->targetAttrIdx;
    hash = 31 * hash + r->targetValueIdx;
    return hash;
}

static int RuleEqual(void *pRule1, void *pRule2) {
    if (pRule1 == NULL || pRule2 == NULL) {
        printf("[Error] RuleEqual: rule is NULL\n");
        abort();
    }
    Rule *r1 = (Rule *)pRule1;
    Rule *r2 = (Rule *)pRule2;
    if (r1->targetAttrIdx != r2->targetAttrIdx) {
        return 0;
    }
    if (r1->targetValueIdx != r2->targetValueIdx) {
        return 0;
    }
    if (iHashSet.Equal(&r1->adminCond, &r2->adminCond) == 0) {
        return 0;
    }
    if (r1->pmapUserCondValue == NULL || r2->pmapUserCondValue == NULL) {
        return iHashSet.Equal(&r1->userCond, &r2->userCond);
    } else {
        int ret = 1;
        HashNodeIterator *it = iHashMap.NewIterator(r1->pmapUserCondValue);
        HashNode *node;
        HashSet **psetCondValues1, **psetCondValues2;
        while (it->HasNext(it)) {
            node = (HashNode *)it->GetNext(it);
            psetCondValues1 = (HashSet **)node->value;
            psetCondValues2 = iHashMap.Get(r2->pmapUserCondValue, node->key);
            if (psetCondValues2 == NULL || iHashSet.Equal(psetCondValues1, psetCondValues2) == 0) {
                ret = 0;
                break;
            }
        }
        iHashMap.DeleteIterator(it);
        return ret;
    }
}

static Rule *Create(HashSet *adminCond, HashSet *userCond, int targetAttrIdx, int targetValueIdx) {
    Rule *r = (Rule *)malloc(sizeof(Rule));
    if (r == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to allocate memory for rule\n");
        return NULL;
    }
    r->adminCond = adminCond;
    r->userCond = userCond;
    r->targetAttrIdx = targetAttrIdx;
    r->targetValueIdx = targetValueIdx;
    r->pmapUserCondValue = NULL;
    return r;
}

/****************************************************************************************************
 * 功能：根据值域更新规则中的用户条件
 *       例：原条件为level:{3,6,9}，值域为level:{2,3,6}，那么更新后的条件为{3,6}
 *       注意：当某个属性的取值集合与值域相同时，说明该属性不会再影响此规则的生效条件，可以从条件中删除
 * 参数：
 *       reachableAVs[in] 各属性的值域
 * 返回值：
 *       1：条件被更新了
 *       0：条件未更新
 *       -1：规则不可被满足
 ***************************************************************************************************/
static int DiscreteCond(Rule *r, HashMap *reachableAVs) {
    int ret = 0;
    int *pAttrIdx;
    HashSet *effectiveVals, **pEffectiveVals, *reachableValues, **pReachableVals;
    HashNode *node;
    HashMap *pmapUserCondValue = r->pmapUserCondValue;
    if (pmapUserCondValue == NULL) {
        pmapUserCondValue = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pmapUserCondValue, iHashSet.DestructPointer);
        
        r->pmapUserCondValue = pmapUserCondValue;
        HashSetIterator *it = iHashSet.NewIterator(r->userCond);
        AtomCondition *atomCond;
        int attrIdx;
        while (it->HasNext(it)) {
            atomCond = it->GetNext(it); //*(atomCondition **)it->GetNext(it);
            attrIdx = atomCond->attribute;

            pEffectiveVals = iHashMap.Get(pmapUserCondValue, &attrIdx);
            if (pEffectiveVals == NULL) {
                effectiveVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                pReachableVals = iHashMap.Get(reachableAVs, &attrIdx);
                if (pReachableVals != NULL) {
                    FindEffectiveValues(atomCond, *pReachableVals, effectiveVals);
                }
                iHashMap.Put(pmapUserCondValue, &attrIdx, &effectiveVals);
            } else {
                RetainEffectiveValues(atomCond, *pEffectiveVals);
            }
        }
        iHashSet.DeleteIterator(it);
        ret = 1;
    } else {
        int before;
        HashNodeIterator *it = iHashMap.NewIterator(pmapUserCondValue);
        while (it->HasNext(it)) {
            node = it->GetNext(it);
            pAttrIdx = (int *)node->key;
            reachableValues = *(HashSet **)node->value;
            before = iHashSet.Size(reachableValues);
            pReachableVals = iHashMap.Get(reachableAVs, pAttrIdx);
            if (pReachableVals == NULL) {
                iHashSet.Clear(reachableValues);
            } else {
                iHashSet.RetainAll(reachableValues, *pReachableVals);
            }
            ret = (iHashSet.Size(reachableValues) != before);
        }
        iHashMap.DeleteIterator(it);
    }

    HashSet **pSet, **pTargetAttrDom = iHashMap.Get(pmapUserCondValue, &r->targetAttrIdx);
    if (pTargetAttrDom != NULL) {
        iHashSet.Add(*pTargetAttrDom, &r->targetValueIdx);
        pSet = iHashMap.Get(reachableAVs, &r->targetAttrIdx);
        if (pSet != NULL && iHashSet.Equal(pTargetAttrDom, pSet)) {
            iHashMap.Remove(pmapUserCondValue, &r->targetAttrIdx);
        } else {
            iHashSet.Remove(*pTargetAttrDom, &r->targetValueIdx);
        }
    }

    HashNodeIterator *it = iHashMap.NewIterator(pmapUserCondValue);
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        pAttrIdx = (int *)node->key;
        reachableValues = *(HashSet **)node->value;
        if (iHashSet.Size(reachableValues) == 0) {
            ret = -1;
            break;
        }
        pSet = iHashMap.Get(reachableAVs, pAttrIdx);
        if (pSet != NULL && iHashSet.Equal(&reachableValues, pSet)) {
            it->Remove(it);
            // free(node->key);
            // iHashSet.Finalize(reachableValues);
            // free(node->value);
            // free(node);
            // iHashMap.Remove(pmapUserCondValue, pAttrIdx);
        }
    }
    iHashMap.DeleteIterator(it);
    return ret;
}

static int CanBeManaged(Rule *r, HashMap *userState) {
    int ret = 1;
    HashNodeIterator *it = iHashMap.NewIterator(r->pmapUserCondValue);
    int *pAttrIdx, *pUserAttrVal;
    HashNode *node;
    HashSet *condValues;
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        pAttrIdx = (int *)node->key;
        condValues = *(HashSet **)node->value;
        pUserAttrVal = iHashMap.Get(userState, pAttrIdx);
        if (pUserAttrVal == NULL || !iHashSet.Contains(condValues, pUserAttrVal)) {
            ret = 0;
            break;
        }
    }
    iHashMap.DeleteIterator(it);
    return ret;
}

static int IsEffective(Rule *r, HashMap *reachableAVs) {
    int ret = 1;
    HashNodeIterator *it = iHashMap.NewIterator(r->pmapUserCondValue);
    int flag, *pAttrIdx, *pVal;
    HashNode *node;
    HashSet *condValues, **pReachableValues;
    HashSetIterator *itCondValues;
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        pAttrIdx = (int *)node->key;
        condValues = *(HashSet **)node->value;
        flag = 0;
        itCondValues = iHashSet.NewIterator(condValues);
        while (itCondValues->HasNext(itCondValues)) {
            pVal = (int *)itCondValues->GetNext(itCondValues);
            pReachableValues = iHashMap.Get(reachableAVs, pAttrIdx);
            if (pReachableValues != NULL && iHashSet.Contains(*pReachableValues, pVal)) {
                flag = 1;
                break;
            }
        }
        iHashSet.DeleteIterator(itCondValues);
        if (!flag) {
            ret = 0;
            break;
        }
    }
    iHashMap.DeleteIterator(it);
    return ret;
}

AtomConditionInterface iAtomCondition = {
    .Evaluate = Evaluate,
    .FindEffectiveValues = FindEffectiveValues,
    .HashCode = AtomCondHashCode,
    .Equal = AtomCondEqual,
};

RuleInterface iRule = {
    .Create = Create,
    .DiscreteCond = DiscreteCond,
    .CanBeManaged = CanBeManaged,
    .IsEffective = IsEffective,
    .HashCode = RuleHashCode,
    .Equal = RuleEqual,
};