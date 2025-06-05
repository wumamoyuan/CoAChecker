#include "acoac_pruning.h"
#include "acoac_utils.h"
#include "analysis_result.h"
#include <time.h>

/****************************************************************************************************
 * 功能：给定一个ACoAC-safety analysis实例与一个属性合取条件，判断是否存在一个用户，其初始状态下的属性能够
 *      满足给定的条件。例如，假设给定的条件为 n0>1 & s1=s1_2，在实例的初始状态，某个用户的属性n0为2，属性
 *      s1为默认值s1_2，那么该条件可以被该用户满足，该函数应该返回1。
 * 参数：
 *      @pInst[in]: ACoAC实例
 *      @pCond[in]: 属性合取条件
 * 返回值：
 *      1：存在满足条件的用户，0：不存在满足条件的用户
 ***************************************************************************************************/
static int isEffective(ACoACInstance *pInst, HashSet *pCond) {
    int attrIdx, *pValueIdx;
    HashMap *pmapAVs;
    HashSetIterator *itCond;
    AtomCondition *pAtomCond;
    int effective = 0;

    // 遍历所有显式指定了部分属性初始值的用户
    HashNodeIterator *it = iHashMap.NewIterator(pInst->pTableInitState->pRowMap);
    while (it->HasNext(it)) {
        // 获得用户初始状态下的属性键值对
        pmapAVs = *(HashMap **)((HashNode *)it->GetNext(it))->value;

        effective = 1;
        // 遍历pCond中的每个原子条件
        itCond = iHashSet.NewIterator(pCond);
        while (itCond->HasNext(itCond)) {
            pAtomCond = (AtomCondition *)itCond->GetNext(itCond);
            attrIdx = pAtomCond->attribute;
            // 获取用户初始状态下的属性值
            pValueIdx = (int *)iHashMap.Get(pmapAVs, &attrIdx);
            if (pValueIdx == NULL) {
                // 没有显式给出属性初始值，使用默认值
                pValueIdx = (int *)iHashMap.Get(pmapAttr2DefVal, &attrIdx);
            }
            // 判断原子条件是否成立
            if (!iAtomCondition.Evaluate(pAtomCond, *pValueIdx)) {
                // 该用户的初始状态下的属性值无法满足该原子条件，终止当前循环并继续判断下一个用户
                effective = 0;
                break;
            }
        }
        iHashSet.DeleteIterator(itCond);

        if (effective) {
            // 找到了一个满足条件的用户，返回1
            iHashMap.DeleteIterator(it);
            return 1;
        }
    }
    iHashMap.DeleteIterator(it);

    // 所有显式指定了部分属性初始值的用户都无法满足条件
    // 继续判断未显式指定初始值的用户，这些用户在初始状态下的的所有属性值都为默认值
    effective = 1;
    // 遍历pCond中的每个原子条件
    itCond = iHashSet.NewIterator(pCond);
    while (itCond->HasNext(itCond)) {
        pAtomCond = (AtomCondition *)itCond->GetNext(itCond);
        attrIdx = pAtomCond->attribute;
        // 获取属性的属性值
        pValueIdx = (int *)iHashMap.Get(pmapAttr2DefVal, &attrIdx);
        // 判断原子条件是否成立
        if (!iAtomCondition.Evaluate(pAtomCond, *pValueIdx)) {
            // 存在无法被满足的原子条件，终止循环
            effective = 0;
            break;
        }
    }
    iHashSet.DeleteIterator(itCond);
    return effective;
}

/****************************************************************************************************
 * 功能：清理用户与初始属性状态，只保留目标用户和目标用户的初始属性状态
 * 参数：
 *      @pInst[in]: ACoAC实例
 * 返回值：
 *      清理后的ACoAC实例
 ***************************************************************************************************/
ACoACInstance *userCleaning(ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, INFO, "[start] user cleaning\n");
    clock_t startUserCleaning = clock();

    ACoACInstance *pNewInst = createACoACInstance();

    // 1.删除所有管理条件无法满足的规则，并将剩余规则的管理条件修改为true
    // 为了提高效率，用一个Map存储已经判断过的AdminCond，避免重复判断
    HashMap *pmapAdminCond2Bool = iHashMap.Create(sizeof(HashSet *), sizeof(int), iHashSet.HashCode, iHashSet.Equal);
    iHashMap.SetDestructKey(pmapAdminCond2Bool, iHashSet.DestructPointer);
    HashSetIterator *itRuleIdxes = iHashSet.NewIterator(pInst->pSetRuleIdxes);
    int effective, *pEffective, ruleIdx;
    HashSet *psetAdminCond;
    while (itRuleIdxes->HasNext(itRuleIdxes)) {
        ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
        Rule *pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        psetAdminCond = pRule->adminCond;
        pEffective = iHashMap.Get(pmapAdminCond2Bool, &psetAdminCond);
        if (pEffective != NULL && *pEffective) {
            // pRule->adminCond = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
            iHashSet.Clear(pRule->adminCond);
            addRule(pNewInst, ruleIdx);
        } else {
            effective = isEffective(pInst, psetAdminCond);
            iHashMap.Put(pmapAdminCond2Bool, &psetAdminCond, &effective);
            if (effective) {
                pRule->adminCond = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
                addRule(pNewInst, ruleIdx);
            }
        }
    }
    iHashSet.DeleteIterator(itRuleIdxes);
    iHashMap.Finalize(pmapAdminCond2Bool);

    iHashMap.Finalize(pInst->pMapAttr2Dom);
    iHashSet.Finalize(pInst->pSetRuleIdxes);
    iHashBasedTable.Finalize(pInst->pTableTargetAV2Rule);
    iHashBasedTable.Finalize(pInst->pTablePrecond2Rule);

    // 2.只保留目标用户
    int queryUserIdx = pInst->queryUserIdx;
    iVector.Add(pNewInst->pVecUserIndices, &queryUserIdx);
    iVector.Finalize(pInst->pVecUserIndices);

    // 3. 只保留目标用户的初始状态，并将状态中未列举的属性值用默认值替代
    HashMap **ppmapInitStateOfQueryUser = iHashMap.Get(pInst->pTableInitState->pRowMap, &queryUserIdx);
    HashNodeIterator *itAttr2DefVal = iHashMap.NewIterator(pmapAttr2DefVal);
    HashNode *node;
    int *pAttrIdx, *pDefValIdx;
    while (itAttr2DefVal->HasNext(itAttr2DefVal)) {
        node = itAttr2DefVal->GetNext(itAttr2DefVal);
        pAttrIdx = (int *)node->key;
        pDefValIdx = (int *)node->value;
        if (ppmapInitStateOfQueryUser == NULL || (pDefValIdx = (int *)iHashMap.Get(*ppmapInitStateOfQueryUser, pAttrIdx)) == NULL) {
            // 该属性在目标用户的初始状态中没有列举，使用默认值
            pDefValIdx = (int *)node->value;
        }
        addUAVByIdx(pNewInst, queryUserIdx, *pAttrIdx, *pDefValIdx);
    }
    iHashMap.DeleteIterator(itAttr2DefVal);
    iHashBasedTable.Finalize(pInst->pTableInitState);

    // 4. 查询不变
    pNewInst->queryUserIdx = queryUserIdx;

    iHashMap.Finalize(pNewInst->pmapQueryAVs);
    pNewInst->pmapQueryAVs = pInst->pmapQueryAVs;

    // 释放pInst
    free(pInst);

    double timeSpent = (double)(clock() - startUserCleaning) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] user cleaning, cost => %.2fms\n", timeSpent);

    return pNewInst;
}

/****************************************************************************************************
 * 功能：对规则进行简单清理
 *      1.根据属性值域清理规则条件，当某个原子条件不可被满足时，删除该策略
 *      2.删除重复规则
 *      3.如果某个属性的值域大小变为1，那么该属性是多余的，需要被删除
 * 参数：
 *      @pInst[in]: 待处理ACoAC实例
 *      @ruleCleaningCnt[in]: 规则清理次数
 *      @pModification[out]: 实例是否发生修改
 *      @result[out]: 清理过程中对策略安全性的验证结果
 * 返回值：
 *      处理后的ACoAC实例
 ***************************************************************************************************/
ACoACInstance *ruleCleaning(ACoACInstance *pInst, int *ruleCleaningCnt, int *pModification, ACoACResult *result) {
    logACoAC(__func__, __LINE__, 0, INFO, "[Start] rule cleaning %d\n", *ruleCleaningCnt);
    clock_t startRuleCleaning = clock();
    ACoACInstance *pNewInst = createACoACInstance();
    *pModification = 0;
    HashMap *pmapAttrDom = pInst->pMapAttr2Dom;

    /*1.根据值域清理查询内容*/
    // 查询用户不变
    int queryUserIdx = pInst->queryUserIdx;
    pNewInst->queryUserIdx = queryUserIdx;
    HashNodeIterator *itQueryAVs = iHashMap.NewIterator(pInst->pmapQueryAVs);
    HashNode *node;
    int *pQueryAttrIdx, *pQueryValueIdx;
    while (itQueryAVs->HasNext(itQueryAVs)) {
        node = itQueryAVs->GetNext(itQueryAVs);
        pQueryAttrIdx = (int *)node->key;
        pQueryValueIdx = (int *)node->value;
        HashSet **ppSetDom = iHashMap.Get(pmapAttrDom, pQueryAttrIdx);

        // 查询属性的值域为空，或者值域中不包含被查询值时，查询不可能成立
        if (ppSetDom == NULL || !iHashSet.Contains(*ppSetDom, pQueryValueIdx)) {
            char *queryAttr = istrCollection.GetElement(pscAttrs, *pQueryAttrIdx);
            char *queryVal = getValueByIndex(getAttrTypeByIdx(*pQueryAttrIdx), *pQueryValueIdx);
            logACoAC(__func__, __LINE__, 0, INFO, "unreachable, because: the query value %s is not in the domain of the query attribute %s\n", queryAttr, queryAttr);
            free(queryVal);
            iHashMap.DeleteIterator(itQueryAVs);
            result->code = ACoAC_RESULT_UNREACHABLE;
            return pNewInst;
        }

        // 当查询属性的值域包含被查询值且大小为1时，该属性项将永远成立，不用参与最终查询
        if (iHashSet.Size(*ppSetDom) == 1) {
            *pModification = 1;
        } else {
            iHashMap.Put(pNewInst->pmapQueryAVs, pQueryAttrIdx, pQueryValueIdx);
        }
    }
    iHashMap.DeleteIterator(itQueryAVs);
    iHashMap.Finalize(pInst->pmapQueryAVs);
    // 如果查询属性值域为空，表示查询条件永远成立
    if (iHashMap.Size(pNewInst->pmapQueryAVs) == 0) {
        logACoAC(__func__, __LINE__, 0, INFO, "reachable, because: the query is always satisfied\n");
        result->code = ACoAC_RESULT_REACHABLE;
        result->pVecActions = iVector.Create(sizeof(AdminstrativeAction), 0);
        return pNewInst;
    }

    HashSet *pSetToBeRemoved = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
    HashNodeIterator *itAttrDom = iHashMap.NewIterator(pmapAttrDom);
    HashSet *pSetDom;
    int *pAttrIdx;
    while (itAttrDom->HasNext(itAttrDom)) {
        node = itAttrDom->GetNext(itAttrDom);
        pAttrIdx = (int *)node->key;
        pSetDom = *(HashSet **)node->value;
        if (iHashSet.Size(pSetDom) == 1) {
            iHashSet.Add(pSetToBeRemoved, pAttrIdx);
        }
    }
    iHashMap.DeleteIterator(itAttrDom);

    /*2.根据值域清洗规则，同时根据Set的无重复性删除重复规则*/
    int discreteResult, nOldRules = iHashSet.Size(pInst->pSetRuleIdxes);
    Rule *pRule;
    HashSetIterator *itRuleIdxes = iHashSet.NewIterator(pInst->pSetRuleIdxes);
    int ruleIdx;
    char *ruleStr;
    while (itRuleIdxes->HasNext(itRuleIdxes)) {
        ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
        pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        // 目标属性值域大小为1时，表示它永远不会变化，此时规则没有意义，不应当保留
        if (iHashSet.Contains(pSetToBeRemoved, &pRule->targetAttrIdx)) {
            *pModification = 1;
            ruleStr = RuleToString(&pRule);
            logACoAC(__func__, __LINE__, 0, DEBUG, "because the size of domain of the target attribute is 1, the rule can be removed: %s\n", ruleStr);
            free(ruleStr);
            continue;
        }
        discreteResult = iRule.DiscreteCond(pRule, pmapAttrDom);
        if (discreteResult == 1) {
            *pModification = 1;
        } else if (discreteResult == -1) {
            // 属性值域无法满足规则条件时，不保留该规则
            ruleStr = RuleToString(&pRule);
            logACoAC(__func__, __LINE__, 0, DEBUG, "the user condition can not be satisfied: %s\n", ruleStr);
            free(ruleStr);
            *pModification = 1;
            continue;
        }
        addRule(pNewInst, ruleIdx);
    }
    iHashSet.DeleteIterator(itRuleIdxes);

    /*3.用户不需要清理*/
    iVector.Finalize(pNewInst->pVecUserIndices);
    pNewInst->pVecUserIndices = pInst->pVecUserIndices;

    /*4.清理初始属性值*/
    HashNodeIterator *itInitState = iHashMap.NewIterator(iHashBasedTable.GetRow(pInst->pTableInitState, &queryUserIdx));
    while (itInitState->HasNext(itInitState)) {
        node = itInitState->GetNext(itInitState);
        pAttrIdx = (int *)node->key;
        if (iHashSet.Contains(pSetToBeRemoved, pAttrIdx)) {
            *pModification = 1;
            continue;
        }
        addUAVByIdx(pNewInst, queryUserIdx, *pAttrIdx, *(int *)node->value);
    }
    iHashSet.Finalize(pSetToBeRemoved);

    iHashMap.DeleteIterator(itInitState);
    iHashBasedTable.Finalize(pInst->pTableInitState);

    iHashMap.Finalize(pInst->pMapAttr2Dom);
    iHashSet.Finalize(pInst->pSetRuleIdxes);
    iHashBasedTable.Finalize(pInst->pTableTargetAV2Rule);
    iHashBasedTable.Finalize(pInst->pTablePrecond2Rule);
    free(pInst);

    int nNewRules = iHashSet.Size(pNewInst->pSetRuleIdxes);
    double timeSpent = (double)(clock() - startRuleCleaning) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] rule cleaning %d, cost => %.2fms\n", (*ruleCleaningCnt)++, timeSpent);
    logACoAC(__func__, __LINE__, 0, INFO, "modification ==> %d, rules: %d==>%d, difference: %d\n", *pModification, nOldRules, nNewRules, nOldRules - nNewRules);
    return pNewInst;
}

/****************************************************************************************************
 * 功能：建立一个映射Map，该Map可以将user映射到user的可达属性值
 *      映射的初始化：即user在初始状态下的各属性值
 *      映射的更新：
 *      循环遍历CS规则，设当前规则为(ca,cu,attr,value)
 *      对于每一组满足a与u的可达属性值分别满足ca与cu的a和u，将(attr,value)加入u的可达属性值，将当前规则加入规则集
 *      上述步骤详细为：
 *      遍历所有用户，设当前用户为u
 *      如果u的可达属性值能够满足ca，则u可充当管理员，将u记录到集合S中，并设置标志flag1，表示已找到管理员
 *      如果u的可达属性值能够满足cu，则u可充当被管理者，将u记录到集合T中
 *      如果flag1为真且T不为空集，则将(attr,value)加入每个T中用户的可达属性值，将规则加入remain中，并设置标志flag2，表示映射已更新
 *      如果映射未更新，则停止循环遍历cs规则
 *      admin被设置为S，user被设置为T
 * 参数：
 *      @pInst[in]: 待处理ACoAC实例
 *      @forwardSlicingCnt[in]: 前向剪枝次数
 *      @pModification[out]: 实例是否发生修改
 *      @result[out]: 剪枝过程中对策略安全性的验证结果
 * 返回值：
 *      处理后的ACoAC实例
 ***************************************************************************************************/
static ACoACInstance *forwardSlice(ACoACInstance *pInst, int *forwardSlicingCnt, int *pModification) {
    logACoAC(__func__, __LINE__, 0, INFO, "[start] forward slicing %d\n", *forwardSlicingCnt);
    clock_t startForwardSlicing = clock();
    *pModification = 0;
    int nOldRules = iHashSet.Size(pInst->pSetRuleIdxes);

    // 创建新实例
    ACoACInstance *pNewInst = createACoACInstance();

    // 1.根据queryUser的初始属性值， 初始化可达属性值
    int queryUserIdx = pInst->queryUserIdx;
    HashMap *pmapReachableAVs = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pmapReachableAVs, iHashSet.DestructPointer);

    HashNodeIterator *itInitState = iHashMap.NewIterator(iHashBasedTable.GetRow(pInst->pTableInitState, &queryUserIdx));
    HashNode *node;
    int *pAttrIdx, *pValIdx;
    HashSet *pSetVals;
    while (itInitState->HasNext(itInitState)) {
        node = itInitState->GetNext(itInitState);
        pAttrIdx = (int *)node->key;
        pValIdx = (int *)node->value;
        pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
        iHashSet.Add(pSetVals, pValIdx);
        iHashMap.Put(pmapReachableAVs, pAttrIdx, &pSetVals);
    }
    iHashMap.DeleteIterator(itInitState);

    // 用于存储增量可达属性值
    HashMap *pmapReachableAVsIncs = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pmapReachableAVsIncs, iHashSet.DestructPointer);

    // 第一轮检查规则是否可达
    HashSetIterator *itRuleIdxes = iHashSet.NewIterator(pInst->pSetRuleIdxes);
    Rule *pRule;
    HashSet **ppSetVals;
    int targetAttrIdx, targetValIdx, ruleIdx;
    while (itRuleIdxes->HasNext(itRuleIdxes)) {
        ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
        pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        if (iRule.IsEffective(pRule, pmapReachableAVs)) {
            targetAttrIdx = pRule->targetAttrIdx;
            targetValIdx = pRule->targetValueIdx;
            addRule(pNewInst, ruleIdx);

            // 更新可达属性值
            ppSetVals = (HashSet **)iHashMap.Get(pmapReachableAVs, &targetAttrIdx);
            if (ppSetVals == NULL) {
                pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                ppSetVals = &pSetVals;
                iHashMap.Put(pmapReachableAVs, &targetAttrIdx, ppSetVals);
            }
            iHashSet.Add(*ppSetVals, &targetValIdx);

            // 更新增量可达属性值
            ppSetVals = (HashSet **)iHashMap.Get(pmapReachableAVsIncs, &targetAttrIdx);
            if (ppSetVals == NULL) {
                pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                ppSetVals = &pSetVals;
                iHashMap.Put(pmapReachableAVsIncs, &targetAttrIdx, ppSetVals);
            }
            iHashSet.Add(*ppSetVals, &targetValIdx);

            // 从剩余规则集中移除
            itRuleIdxes->Remove(itRuleIdxes);
        }
    }
    iHashSet.DeleteIterator(itRuleIdxes);

    // 迭代处理增量可达属性值
    HashMap *pmapNewIncrement;
    HashNodeIterator *itReachableAVsIncs;
    HashSetIterator *itVals;
    HashSet **ppSetRuleIdxes;
    int *pRuleIdx;
    while (iHashMap.Size(pmapReachableAVsIncs) > 0) {
        pmapNewIncrement = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
        iHashMap.SetDestructValue(pmapNewIncrement, iHashSet.DestructPointer);

        itReachableAVsIncs = iHashMap.NewIterator(pmapReachableAVsIncs);
        while (itReachableAVsIncs->HasNext(itReachableAVsIncs)) {
            node = itReachableAVsIncs->GetNext(itReachableAVsIncs);
            pAttrIdx = (int *)node->key;
            itVals = iHashSet.NewIterator(*(HashSet **)node->value);
            while (itVals->HasNext(itVals)) {
                pValIdx = (int *)itVals->GetNext(itVals);
                // 找出所有可能因增量可达属性值而可达的规则，即以该属性值为前置条件的规则
                ppSetRuleIdxes = (HashSet **)iHashBasedTable.Get(pInst->pTablePrecond2Rule, pAttrIdx, pValIdx);
                if (ppSetRuleIdxes == NULL) {
                    continue;
                }
                itRuleIdxes = iHashSet.NewIterator(*ppSetRuleIdxes);
                while (itRuleIdxes->HasNext(itRuleIdxes)) {
                    pRuleIdx = (int *)itRuleIdxes->GetNext(itRuleIdxes);
                    pRule = (Rule *)iVector.GetElement(pVecRules, *pRuleIdx);
                    // 检查规则是否在剩余规则集中且可达
                    if (!iHashSet.Contains(pInst->pSetRuleIdxes, pRuleIdx) || !iRule.IsEffective(pRule, pmapReachableAVs)) {
                        continue;
                    }
                    // 将该规则添加到新实例中
                    addRule(pNewInst, *pRuleIdx);
                    targetAttrIdx = pRule->targetAttrIdx;
                    targetValIdx = pRule->targetValueIdx;
                    ppSetVals = (HashSet **)iHashMap.Get(pmapReachableAVs, &targetAttrIdx);
                    if (ppSetVals != NULL && iHashSet.Contains(*ppSetVals, &targetValIdx)) {
                        continue;
                    }
                    // 更新可达属性值
                    if (ppSetVals == NULL) {
                        pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                        ppSetVals = &pSetVals;
                        iHashMap.Put(pmapReachableAVs, &targetAttrIdx, ppSetVals);
                    }
                    iHashSet.Add(*ppSetVals, &targetValIdx);
                    // 更新新增量
                    ppSetVals = (HashSet **)iHashMap.Get(pmapNewIncrement, &targetAttrIdx);
                    if (ppSetVals == NULL) {
                        pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                        ppSetVals = &pSetVals;
                        iHashMap.Put(pmapNewIncrement, &targetAttrIdx, ppSetVals);
                    }
                    iHashSet.Add(*ppSetVals, &targetValIdx);
                    // 从剩余规则集中移除
                    iHashSet.Remove(pInst->pSetRuleIdxes, pRuleIdx);
                }
                iHashSet.DeleteIterator(itRuleIdxes);
            }
            iHashSet.DeleteIterator(itVals);
        }
        iHashMap.DeleteIterator(itReachableAVsIncs);
        // 清理旧的增量集
        iHashMap.Finalize(pmapReachableAVsIncs);
        pmapReachableAVsIncs = pmapNewIncrement;
    }
    iHashMap.Finalize(pmapReachableAVsIncs);

    // 检查是否发生修改
    if (iHashSet.Size(pNewInst->pSetRuleIdxes) != nOldRules) {
        *pModification = 1;
        itRuleIdxes = iHashSet.NewIterator(pInst->pSetRuleIdxes);
        while (itRuleIdxes->HasNext(itRuleIdxes)) {
            ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
            pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
            logACoAC(__func__, __LINE__, 0, DEBUG, "user condition can never be satisfied: %s", RuleToString(&pRule));
        }
        iHashMap.DeleteIterator(itRuleIdxes);
    }

    // 设置新实例的其他属性
    iHashMap.Finalize(pNewInst->pMapAttr2Dom);
    pNewInst->pMapAttr2Dom = pmapReachableAVs;

    iVector.Finalize(pNewInst->pVecUserIndices);
    pNewInst->pVecUserIndices = pInst->pVecUserIndices;

    iHashBasedTable.Finalize(pNewInst->pTableInitState);
    pNewInst->pTableInitState = pInst->pTableInitState;

    pNewInst->queryUserIdx = pInst->queryUserIdx;

    iHashMap.Finalize(pNewInst->pmapQueryAVs);
    pNewInst->pmapQueryAVs = pInst->pmapQueryAVs;

    iHashMap.Finalize(pInst->pMapAttr2Dom);
    iHashSet.Finalize(pInst->pSetRuleIdxes);
    iHashBasedTable.Finalize(pInst->pTablePrecond2Rule);
    iHashBasedTable.Finalize(pInst->pTableTargetAV2Rule);
    free(pInst);

    int nNewRules = iHashSet.Size(pNewInst->pSetRuleIdxes);

    double timeSpent = (double)(clock() - startForwardSlicing) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] forward slicing %d, cost => %.2fms\n", (*forwardSlicingCnt)++, timeSpent);
    logACoAC(__func__, __LINE__, 0, INFO, "modification ==> %d, rules: %d==>%d, difference: %d\n", *pModification, nOldRules, nNewRules, nOldRules - nNewRules);

    return pNewInst;
}

typedef struct _AVP {
    int attrIdx;
    int valIdx;
} AVP;

static unsigned int AVPHashCode(void *pKey) {
    AVP *pAVP = (AVP *)pKey;
    unsigned int hash = 1;
    hash = hash * 31 + pAVP->attrIdx;
    hash = hash * 31 + pAVP->valIdx;
    return hash;
}

static int AVPEqual(void *pKey1, void *pKey2) {
    AVP *pAVP1 = (AVP *)pKey1;
    AVP *pAVP2 = (AVP *)pKey2;
    return pAVP1->attrIdx == pAVP2->attrIdx && pAVP1->valIdx == pAVP2->valIdx;
}

ACoACInstance *backwardSlice(ACoACInstance *pInst, int *backwardSlicingCnt, int *pModification) {
    logACoAC(__func__, __LINE__, 0, INFO, "[Start] backward slicing %d\n", *backwardSlicingCnt);
    clock_t startBackwardSlicing = clock();
    *pModification = 0;
    ACoACInstance *pNewInst = createACoACInstance();

    List *pListStack = iList.Create(sizeof(AVP));
    HashSet *pSetVisited = iHashSet.Create(sizeof(AVP), AVPHashCode, AVPEqual);
    HashNodeIterator *itQueryAVs = iHashMap.NewIterator(pInst->pmapQueryAVs);
    HashNode *node;
    int queryAttrIdx, queryValueIdx;
    while (itQueryAVs->HasNext(itQueryAVs)) {
        node = itQueryAVs->GetNext(itQueryAVs);
        queryAttrIdx = *(int *)node->key;
        queryValueIdx = *(int *)node->value;
        AVP avp = {.attrIdx = queryAttrIdx, .valIdx = queryValueIdx};
        iList.PushFront(pListStack, &avp);
        iHashSet.Add(pSetVisited, &avp);
    }
    iHashMap.DeleteIterator(itQueryAVs);

    HashMap *pmapReachableAVs = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pmapReachableAVs, iHashSet.DestructPointer);

    AVP avp, avp2;
    Rule *pRule;
    int ruleIdx;
    char *ruleStr, *val;
    HashNodeIterator *itUserCondValue;
    HashSet **ppSetRuleIdxes, *pSetVals, **ppSetVals;
    HashSetIterator *itRuleIdxes, *itVals;
    while (iList.Size(pListStack) > 0) {
        iList.PopFront(pListStack, &avp);
        ppSetRuleIdxes = iHashBasedTable.Get(pInst->pTableTargetAV2Rule, &avp.attrIdx, &avp.valIdx);
        if (ppSetRuleIdxes == NULL) {
            continue;
        }

        ppSetVals = (HashSet **)iHashMap.Get(pmapReachableAVs, &avp.attrIdx);
        if (ppSetVals == NULL) {
            pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            ppSetVals = &pSetVals;
            iHashMap.Put(pmapReachableAVs, &avp.attrIdx, ppSetVals);
        }
        iHashSet.Add(*ppSetVals, &avp.valIdx);

        itRuleIdxes = iHashSet.NewIterator(*ppSetRuleIdxes);
        while (itRuleIdxes->HasNext(itRuleIdxes)) {
            ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
            pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
            addRule(pNewInst, ruleIdx);
            ruleStr = RuleToString(&pRule);
            val = getValueByIndex(getAttrTypeByIdx(avp.attrIdx), avp.valIdx);
            logACoAC(__func__, __LINE__, 0, DEBUG, "rule %s is associated with (%s, %s)", ruleStr, istrCollection.GetElement(pscAttrs, avp.attrIdx), val);
            free(ruleStr);
            free(val);
            itUserCondValue = iHashMap.NewIterator(pRule->pmapUserCondValue);
            while (itUserCondValue->HasNext(itUserCondValue)) {
                node = itUserCondValue->GetNext(itUserCondValue);
                itVals = iHashSet.NewIterator(*(HashSet **)node->value);
                while (itVals->HasNext(itVals)) {
                    avp2 = (AVP){.attrIdx = *(int *)node->key, .valIdx = *(int *)itVals->GetNext(itVals)};
                    if (iHashSet.Add(pSetVisited, &avp2)) {
                        iList.PushFront(pListStack, &avp2);
                    }
                }
                iHashSet.DeleteIterator(itVals);
            }
            iHashMap.DeleteIterator(itUserCondValue);
        }
        iHashSet.DeleteIterator(itRuleIdxes);
    }
    iList.Finalize(pListStack);
    iHashSet.Finalize(pSetVisited);

    if (iHashSet.Size(pNewInst->pSetRuleIdxes) != iHashSet.Size(pInst->pSetRuleIdxes)) {
        *pModification = 1;
    }

    int *pAttrIdx;
    HashNodeIterator *itInitState = iHashMap.NewIterator(iHashBasedTable.GetRow(pInst->pTableInitState, &pInst->queryUserIdx));
    while (itInitState->HasNext(itInitState)) {
        node = itInitState->GetNext(itInitState);
        pAttrIdx = (int *)node->key;
        ppSetVals = (HashSet **)iHashMap.Get(pmapReachableAVs, pAttrIdx);
        if (ppSetVals == NULL) {
            pSetVals = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
            ppSetVals = &pSetVals;
            iHashMap.Put(pmapReachableAVs, pAttrIdx, ppSetVals);
        }
        iHashSet.Add(*ppSetVals, node->value);
    }
    iHashMap.DeleteIterator(itInitState);

    int nOldRules = iHashSet.Size(pInst->pSetRuleIdxes);

    iHashMap.Finalize(pNewInst->pMapAttr2Dom);
    pNewInst->pMapAttr2Dom = pmapReachableAVs;

    iVector.Finalize(pNewInst->pVecUserIndices);
    pNewInst->pVecUserIndices = pInst->pVecUserIndices;

    iHashBasedTable.Finalize(pNewInst->pTableInitState);
    pNewInst->pTableInitState = pInst->pTableInitState;

    pNewInst->queryUserIdx = pInst->queryUserIdx;

    iHashMap.Finalize(pNewInst->pmapQueryAVs);
    pNewInst->pmapQueryAVs = pInst->pmapQueryAVs;

    iHashMap.Finalize(pInst->pMapAttr2Dom);
    iHashSet.Finalize(pInst->pSetRuleIdxes);
    iHashBasedTable.Finalize(pInst->pTablePrecond2Rule);
    iHashBasedTable.Finalize(pInst->pTableTargetAV2Rule);
    free(pInst);

    int nNewRules = iHashSet.Size(pNewInst->pSetRuleIdxes);

    double timeSpent = (double)(clock() - startBackwardSlicing) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] backward slicing %d, cost => %.2fms\n", (*backwardSlicingCnt)++, timeSpent);
    logACoAC(__func__, __LINE__, 0, INFO, "modification ==> %d, rules: %d==>%d, difference: %d\n", *pModification, nOldRules, nNewRules, nOldRules - nNewRules);

    return pNewInst;
}

ACoACInstance *slice(ACoACInstance *pInst, ACoACResult *pResult) {
    logACoAC(__func__, __LINE__, 0, INFO, "[start] slicing instance\n");
    clock_t startSlicing = clock();
    int nOldRules = iHashSet.Size(pInst->pSetRuleIdxes);

    pResult->code = ACoAC_RESULT_UNKNOWN;
    int rcMod, rcCnt = 1;
    int fsMod = 1, fsCnt = 1;
    int bsMod = 1, bsCnt = 1;
    while (1) {
        pInst = ruleCleaning(pInst, &rcCnt, &rcMod, pResult);
        if (pResult->code != ACoAC_RESULT_UNKNOWN) {
            return pInst;
        }
        if (!rcMod && !fsMod && !bsMod) {
            // 实例不再变化时，停止剪枝
            break;
        }
        pInst = forwardSlice(pInst, &fsCnt, &fsMod);
        if (!rcMod && !fsMod && !bsMod) {
            // 实例不再变化时，停止剪枝
            break;
        }
        pInst = backwardSlice(pInst, &bsCnt, &bsMod);
        if (!rcMod && !fsMod && !bsMod) {
            // 实例不再变化时，停止剪枝
            break;
        }
    }
    int nNewRules = iHashSet.Size(pInst->pSetRuleIdxes);
    double timeSpent = (double)(clock() - startSlicing) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] slicing instance, cost => %.2fms\n", timeSpent);
    logACoAC(__func__, __LINE__, 0, INFO, "rules: %d==>%d, difference: %d\n", nOldRules, nNewRules, nOldRules - nNewRules);
    return pInst;
}
