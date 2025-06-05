#include "acoac_absref.h"
#include "acoac_utils.h"

#include <time.h>

AbsRef *createAbsRef(ACoACInstance *pOriInst) {
    AbsRef *pAbsRef = (AbsRef *)malloc(sizeof(AbsRef));
    pAbsRef->pOriInst = pOriInst;
    pAbsRef->pOriVecRules = pVecRules;
    pAbsRef->round = 0;

    pAbsRef->pSetF = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
    pAbsRef->pSetB = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
    pAbsRef->pMapReachableAVs = NULL;
    pAbsRef->pMapReachableAVsInc = NULL;
    pAbsRef->pMapUsefulAVs = NULL;
    pAbsRef->pMapUsefulAVsInc = NULL;
    return pAbsRef;
}

/**
 * Forward rule-selection strategy.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 * @return 0 if no new rules are selected, 1 otherwise
 */
static int forwardSearch(AbsRef *pAbsRef) {
    int ret = 0;

    if (pAbsRef->pMapReachableAVsInc == NULL) {
        pAbsRef->pMapReachableAVs = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pAbsRef->pMapReachableAVs, iHashSet.DestructPointer);
        pAbsRef->pMapReachableAVsInc = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pAbsRef->pMapReachableAVsInc, iHashSet.DestructPointer);

        HashNodeIterator *itMap = iHashMap.NewIterator(
            *(HashMap **)iHashMap.Get(pAbsRef->pOriInst->pTableInitState->pRowMap, &pAbsRef->pOriInst->queryUserIdx));
        HashNode *node;
        HashSet *pSet;
        while (itMap->HasNext(itMap)) {
            node = itMap->GetNext(itMap);
            pSet = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            iHashSet.Add(pSet, node->value);
            iHashMap.Put(pAbsRef->pMapReachableAVs, node->key, &pSet);

            pSet = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            iHashSet.Add(pSet, node->value);
            iHashMap.Put(pAbsRef->pMapReachableAVsInc, node->key, &pSet);
        }
        iHashMap.DeleteIterator(itMap);
    }

    HashBasedTable *pTablePrecond2Rule = pAbsRef->pOriInst->pTablePrecond2Rule;
    HashMap *pMapNewReachableAVsInc = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pMapNewReachableAVsInc, iHashSet.DestructPointer);

    /* Traverse the newly reachable attribute values, find the rules that may become reachable
     * because of the newly reachable attribute values. If these rules are not in pf, and the 
     * current reachable attribute key-value pair can satisfy the userCond of the rule, then
     * add the rule to pf, and update the reachable attribute key-value pair */
    HashNodeIterator *itMap = iHashMap.NewIterator(pAbsRef->pMapReachableAVsInc);
    HashNode *node;
    HashSet **ppSetRuleIdxes, *pSetValIdxes, **ppSetValIdxes;
    HashSetIterator *itSetValIdx, *itSetRuleIdx;
    int *pAttrIdx, *pValueIdx, *pRuleIdx, targetAttrIdx, targetValueIdx;
    Rule *pRule;
    while (itMap->HasNext(itMap)) {
        node = itMap->GetNext(itMap);
        pAttrIdx = (int *)node->key;
        itSetValIdx = iHashSet.NewIterator(*(HashSet **)node->value);
        while (itSetValIdx->HasNext(itSetValIdx)) {
            pValueIdx = (int *)itSetValIdx->GetNext(itSetValIdx);
            ppSetRuleIdxes = iHashBasedTable.Get(pTablePrecond2Rule, pAttrIdx, pValueIdx);
            if (ppSetRuleIdxes == NULL) {
                continue;
            }
            itSetRuleIdx = iHashSet.NewIterator(*ppSetRuleIdxes);
            while (itSetRuleIdx->HasNext(itSetRuleIdx)) {
                pRuleIdx = (int *)itSetRuleIdx->GetNext(itSetRuleIdx);
                pRule = iVector.GetElement(pAbsRef->pOriVecRules, *pRuleIdx);
                if (iHashSet.Contains(pAbsRef->pSetF, pRuleIdx)) {
                    continue;
                }
                if (!iRule.IsEffective(pRule, pAbsRef->pMapReachableAVs)) {
                    continue;
                }
                ret = 1;
                iHashSet.Add(pAbsRef->pSetF, pRuleIdx);
                targetAttrIdx = pRule->targetAttrIdx;
                targetValueIdx = pRule->targetValueIdx;
                ppSetValIdxes = iHashMap.Get(pAbsRef->pMapReachableAVs, &targetAttrIdx);
                if (ppSetValIdxes == NULL || !iHashSet.Contains(*ppSetValIdxes, &targetValueIdx)) {
                    ppSetValIdxes = iHashMap.Get(pMapNewReachableAVsInc, &targetAttrIdx);
                    if (ppSetValIdxes == NULL) {
                        pSetValIdxes = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                        ppSetValIdxes = &pSetValIdxes;
                        iHashMap.Put(pMapNewReachableAVsInc, &targetAttrIdx, ppSetValIdxes);
                    }
                    iHashSet.Add(*ppSetValIdxes, &targetValueIdx);
                }
            }
            iHashSet.DeleteIterator(itSetRuleIdx);
        }
        iHashSet.DeleteIterator(itSetValIdx);
    }
    iHashMap.DeleteIterator(itMap);

    // Add the new reachable attribute-value pairs in pMapNewReachableAVsInc to pMapReachableAVs
    itMap = iHashMap.NewIterator(pMapNewReachableAVsInc);
    while (itMap->HasNext(itMap)) {
        node = itMap->GetNext(itMap);
        pAttrIdx = (int *)node->key;
        ppSetValIdxes = iHashMap.Get(pAbsRef->pMapReachableAVs, pAttrIdx);
        if (ppSetValIdxes == NULL) {
            pSetValIdxes = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            ppSetValIdxes = &pSetValIdxes;
            iHashMap.Put(pAbsRef->pMapReachableAVs, pAttrIdx, ppSetValIdxes);
        }
        itSetValIdx = iHashSet.NewIterator(*(HashSet **)node->value);
        while (itSetValIdx->HasNext(itSetValIdx)) {
            iHashSet.Add(*ppSetValIdxes, itSetValIdx->GetNext(itSetValIdx));
        }
        iHashSet.DeleteIterator(itSetValIdx);
    }
    iHashMap.DeleteIterator(itMap);

    iHashMap.Finalize(pAbsRef->pMapReachableAVsInc);
    pAbsRef->pMapReachableAVsInc = pMapNewReachableAVsInc;
    return ret;
}

/**
 * Backward rule-selection strategy.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 * @return 0 if no new rules are selected, 1 otherwise
 */
static int backwardSearch(AbsRef *pAbsRef) {
    int ret = 0;
    if (pAbsRef->pMapUsefulAVsInc == NULL) {
        // Initialize pMapUsefulAVs with pMapQueryAVs
        pAbsRef->pMapUsefulAVs = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pAbsRef->pMapUsefulAVs, iHashSet.DestructPointer);
        pAbsRef->pMapUsefulAVsInc = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pAbsRef->pMapUsefulAVsInc, iHashSet.DestructPointer);

        HashNodeIterator *itMap = iHashMap.NewIterator(pAbsRef->pOriInst->pmapQueryAVs);
        HashNode *node;
        HashSet *pSet;
        while (itMap->HasNext(itMap)) {
            node = itMap->GetNext(itMap);
            pSet = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            iHashSet.Add(pSet, node->value);
            iHashMap.Put(pAbsRef->pMapUsefulAVs, node->key, &pSet);

            pSet = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            iHashSet.Add(pSet, node->value);
            iHashMap.Put(pAbsRef->pMapUsefulAVsInc, node->key, &pSet);
        }
        iHashMap.DeleteIterator(itMap);
    }

    if (iHashMap.Size(pAbsRef->pMapUsefulAVsInc) == 0) {
        return 0;
    }

    HashMap *pMapNewUsefulAVsInc = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pMapNewUsefulAVsInc, iHashSet.DestructPointer);

    HashNodeIterator *itMap = iHashMap.NewIterator(pAbsRef->pMapUsefulAVsInc), *itMap2;
    HashNode *node, *node2;
    HashSet **ppSetRuleIdxes, *pSetValIdxes, **ppSetValIdxes;
    HashSetIterator *itSetValIdx, *itSetValIdx2, *itSetRuleIdx;
    int *pAttrIdx, *pAttrIdx2, *pValueIdx, *pValueIdx2, *pRuleIdx;
    Rule *pRule;

    while (itMap->HasNext(itMap)) {
        node = itMap->GetNext(itMap);
        pAttrIdx = (int *)node->key;
        itSetValIdx = iHashSet.NewIterator(*(HashSet **)node->value);
        while (itSetValIdx->HasNext(itSetValIdx)) {
            pValueIdx = (int *)itSetValIdx->GetNext(itSetValIdx);
            ppSetRuleIdxes = iHashBasedTable.Get(pAbsRef->pOriInst->pTableTargetAV2Rule, pAttrIdx, pValueIdx);
            if (ppSetRuleIdxes == NULL) {
                continue;
            }
            itSetRuleIdx = iHashSet.NewIterator(*ppSetRuleIdxes);
            while (itSetRuleIdx->HasNext(itSetRuleIdx)) {
                pRuleIdx = (int *)itSetRuleIdx->GetNext(itSetRuleIdx);
                if (!iHashSet.Add(pAbsRef->pSetB, pRuleIdx)) {
                    continue;
                }
                ret = 1;
                pRule = iVector.GetElement(pAbsRef->pOriVecRules, *pRuleIdx);
                itMap2 = iHashMap.NewIterator(pRule->pmapUserCondValue);
                while (itMap2->HasNext(itMap2)) {
                    node2 = itMap2->GetNext(itMap2);
                    pAttrIdx2 = (int *)node2->key;
                    itSetValIdx2 = iHashSet.NewIterator(*(HashSet **)node2->value);
                    while (itSetValIdx2->HasNext(itSetValIdx2)) {
                        pValueIdx2 = (int *)itSetValIdx2->GetNext(itSetValIdx2);
                        ppSetValIdxes = iHashMap.Get(pAbsRef->pMapUsefulAVs, pAttrIdx2);
                        if (ppSetValIdxes == NULL) {
                            pSetValIdxes = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                            ppSetValIdxes = &pSetValIdxes;
                            iHashMap.Put(pAbsRef->pMapUsefulAVs, pAttrIdx2, ppSetValIdxes);
                        }
                        if (!iHashSet.Add(*ppSetValIdxes, pValueIdx2)) {
                            continue;
                        }

                        ppSetValIdxes = iHashMap.Get(pMapNewUsefulAVsInc, pAttrIdx2);
                        if (ppSetValIdxes == NULL) {
                            pSetValIdxes = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                            ppSetValIdxes = &pSetValIdxes;
                            iHashMap.Put(pMapNewUsefulAVsInc, pAttrIdx2, ppSetValIdxes);
                        }
                        iHashSet.Add(*ppSetValIdxes, pValueIdx2);
                    }
                    iHashSet.DeleteIterator(itSetValIdx2);
                }
                iHashMap.DeleteIterator(itMap2);
            }
            iHashSet.DeleteIterator(itSetRuleIdx);
        }
        iHashSet.DeleteIterator(itSetValIdx);
    }
    iHashMap.DeleteIterator(itMap);

    iHashMap.Finalize(pAbsRef->pMapUsefulAVsInc);
    pAbsRef->pMapUsefulAVsInc = pMapNewUsefulAVsInc;
    return ret;
}

/**
 * Clone the original global rule list.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 */
static void cloneRules(AbsRef *pAbsRef) {
    int nRules = iVector.Size(pAbsRef->pOriVecRules);

    Vector *pVecNewRules = iVector.Create(sizeof(Rule), nRules);
    Rule *pRule, *pNewRule;
    HashNode *node;
    HashSet *pSet;
    HashSetIterator *itSet;
    int i;
    for (i = 0; i < nRules; i++) {
        pRule = iVector.GetElement(pAbsRef->pOriVecRules, i);
        iVector.Add(pVecNewRules, pRule);
        pNewRule = iVector.GetElement(pVecNewRules, i);

        if (pRule->pmapUserCondValue != NULL) {
            pNewRule->pmapUserCondValue = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
            HashNodeIterator *itMap = iHashMap.NewIterator(pRule->pmapUserCondValue);
            while (itMap->HasNext(itMap)) {
                node = itMap->GetNext(itMap);

                pSet = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                itSet = iHashSet.NewIterator(*(HashSet **)node->value);
                while (itSet->HasNext(itSet)) {
                    iHashSet.Add(pSet, itSet->GetNext(itSet));
                }
                iHashSet.DeleteIterator(itSet);
                iHashMap.Put(pNewRule->pmapUserCondValue, node->key, &pSet);
            }
            iHashMap.DeleteIterator(itMap);
        }
    }
    pVecRules = pVecNewRules;
}

/**
 * Use the rules selected by the forward rule-selection strategy and backward rule-selection strategy
 * to generate an ACoAC instance. If the rule-selection strategies cannot select any new rules, return NULL.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 * @return An ACoAC instance containing the rules selected by the forward-selection strategy
 * and backward-selection strategy
 */
static ACoACInstance *getInstance(AbsRef *pAbsRef) {
    int modified = 0;
    modified |= forwardSearch(pAbsRef);
    modified |= backwardSearch(pAbsRef);
    if (!modified) {
        logACoAC(__func__, __LINE__, 0, INFO, "Failed, the policy cannot be refined anymore...\n");
        return NULL;
    }

    cloneRules(pAbsRef);

    ACoACInstance *pNewInstance = createACoACInstance();
    HashSetIterator *itSet = iHashSet.NewIterator(pAbsRef->pSetF);
    int ruleIdx;
    while (itSet->HasNext(itSet)) {
        ruleIdx = *(int *)itSet->GetNext(itSet);
        addRule(pNewInstance, ruleIdx);
    }
    iHashSet.DeleteIterator(itSet);

    itSet = iHashSet.NewIterator(pAbsRef->pSetB);
    while (itSet->HasNext(itSet)) {
        ruleIdx = *(int *)itSet->GetNext(itSet);
        addRule(pNewInstance, ruleIdx);
    }
    iHashSet.DeleteIterator(itSet);

    int queryUserIdx = pAbsRef->pOriInst->queryUserIdx;
    pNewInstance->pVecUserIndices = pAbsRef->pOriInst->pVecUserIndices;

    HashNodeIterator *itMap = iHashMap.NewIterator(*(HashMap **)iHashMap.Get(pAbsRef->pOriInst->pTableInitState->pRowMap, &queryUserIdx));
    HashNode *node;
    while (itMap->HasNext(itMap)) {
        node = itMap->GetNext(itMap);
        addUAVByIdx(pNewInstance, queryUserIdx, *(int *)node->key, *(int *)node->value);
    }
    iHashMap.DeleteIterator(itMap);

    pNewInstance->queryUserIdx = queryUserIdx;

    itMap = iHashMap.NewIterator(pAbsRef->pOriInst->pmapQueryAVs);
    while (itMap->HasNext(itMap)) {
        node = itMap->GetNext(itMap);
        iHashMap.Put(pNewInstance->pmapQueryAVs, node->key, node->value);
    }
    iHashMap.DeleteIterator(itMap);

    return pNewInstance;
}

ACoACInstance *abstract(AbsRef *pAbsRef) {
    logACoAC(__func__, __LINE__, 0, INFO, "[Start] abstracting policy\n");
    pAbsRef->round++;
    clock_t startAbstracting = clock();

    ACoACInstance *newInstance = getInstance(pAbsRef);

    double timeSpent = (double)(clock() - startAbstracting) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] abstracting policy, cost => %.2fms\n", timeSpent);
    logACoAC(__func__, __LINE__, 0, INFO, "rules => %d/%d\n", iHashSet.Size(newInstance->pSetRuleIdxes), iHashSet.Size(pAbsRef->pOriInst->pSetRuleIdxes));

    return newInstance;
}

ACoACInstance *refine(AbsRef *pAbsRef) {
    logACoAC(__func__, __LINE__, 0, INFO, "[Start] %d-th policy refinement\n", pAbsRef->round);
    clock_t startRefining = clock();

    ACoACInstance *newInstance = getInstance(pAbsRef);

    if (newInstance != NULL) {
        double timeSpent = (double)(clock() - startRefining) / CLOCKS_PER_SEC * 1000;
        logACoAC(__func__, __LINE__, 0, INFO, "[end] %d-th policy refinement, cost => %.2fms\n", pAbsRef->round, timeSpent);
        logACoAC(__func__, __LINE__, 0, INFO, "rules => %d/%d\n", iHashSet.Size(newInstance->pSetRuleIdxes), iHashSet.Size(pAbsRef->pOriInst->pSetRuleIdxes));
    }
    pAbsRef->round++;
    return newInstance;
}
