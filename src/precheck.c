#include "precheck.h"

static ACoACResult getReachableResultFromRules(ACoACInstance *pInst, Vector *ruleIdxes) {
    Vector *actions = iVector.Create(sizeof(AdminstrativeAction), 0);
    int ruleIdx;
    Rule *pRule;
    AdminstrativeAction action;
    AttrType attrType;
    int i;
    for (i = 0; i < iVector.Size(ruleIdxes); i++) {
        ruleIdx = *(int *)iVector.GetElement(ruleIdxes, i);
        pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        
        action.adminIdx = pInst->queryUserIdx;
        action.userIdx = pInst->queryUserIdx;
        action.attr = istrCollection.GetElement(pscAttrs, pRule->targetAttrIdx);
        attrType = *(AttrType *)iHashMap.Get(pmapAttr2Type, &pRule->targetAttrIdx);
        action.val = getValueByIndex(attrType, pRule->targetValueIdx);

        iVector.Add(actions, &action);
    }
    return (ACoACResult){
        .code = ACoAC_RESULT_REACHABLE,
        .pVecActions = actions,
        .pVecRules = ruleIdxes};
}

ACoACResult preCheck(ACoACInstance *pInst) {
    HashMap *pmapQueryAVs = pInst->pmapQueryAVs;
    HashBasedTable *pTableTargetAV2Rule = pInst->pTableTargetAV2Rule;
    Vector *pVecSelectedRuleIdxes = iVector.Create(sizeof(int), 0);

    // 第一阶段，对于a1=v1,...,an=vn形式的查询，依次检查是否存在目标为ai=vi且条件为true的规则，如果都存在，则说明查询属性组是可达的
    int success = 1, found;
    HashNodeIterator *itQueryAVs = iHashMap.NewIterator(pmapQueryAVs);
    HashNode *node;
    int queryAttrIdx, queryValueIdx, ruleIdx;
    HashSet **ppSetRuleIdxes;
    HashSetIterator *itRuleIdxes;
    Rule *pRule;
    while (itQueryAVs->HasNext(itQueryAVs)) {
        node = (HashNode *)itQueryAVs->GetNext(itQueryAVs);
        queryAttrIdx = *(int *)node->key;
        queryValueIdx = *(int *)node->value;

        if (queryValueIdx == getInitValue(pInst, pInst->queryUserIdx, queryAttrIdx)) {
            continue;
        }
        ppSetRuleIdxes = (HashSet **)iHashBasedTable.Get(pTableTargetAV2Rule, &queryAttrIdx, &queryValueIdx);
        if (ppSetRuleIdxes == NULL) {
            // 一定不可达
            iHashMap.DeleteIterator(itQueryAVs);
            return (ACoACResult){.code = ACoAC_RESULT_UNREACHABLE};
        }
        found = 0;
        itRuleIdxes = iHashSet.NewIterator(*ppSetRuleIdxes);
        while (itRuleIdxes->HasNext(itRuleIdxes)) {
            ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
            pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
            if (iHashMap.Size(pRule->pmapUserCondValue) == 0) {
                found = 1;
                iVector.Add(pVecSelectedRuleIdxes, &ruleIdx);
                break;
            }
        }
        iHashSet.DeleteIterator(itRuleIdxes);
        if (!found) {
            success = 0;
            break;
        }
    }
    iHashMap.DeleteIterator(itQueryAVs);
    if (success) {
        return getReachableResultFromRules(pInst, pVecSelectedRuleIdxes);
    }

    iVector.Clear(pVecSelectedRuleIdxes);
    // 如果目标属性值只有一对，遍历以其为目标的规则，判断这些规则的条件是否均可无条件满足
    if (iHashMap.Size(pmapQueryAVs) == 1) {
        itQueryAVs = iHashMap.NewIterator(pmapQueryAVs);
        node = (HashNode *)itQueryAVs->GetNext(itQueryAVs);
        queryAttrIdx = *(int *)node->key;
        queryValueIdx = *(int *)node->value;
        ppSetRuleIdxes = (HashSet **)iHashBasedTable.Get(pTableTargetAV2Rule, &queryAttrIdx, &queryValueIdx);
        itRuleIdxes = iHashSet.NewIterator(*ppSetRuleIdxes);

        HashNodeIterator *itUserCondValue;
        int condAttrIdx, condValueIdx;
        HashSet *psetCondValueIdxes;
        HashSetIterator *itCondValueIdxes;
        HashSet **ppSetRuleIdxes2;
        HashSetIterator *itRuleIdxes2;
        int ruleIdx2;
        Rule *pRule2;
        while (itRuleIdxes->HasNext(itRuleIdxes)) {
            ruleIdx = *(int *)itRuleIdxes->GetNext(itRuleIdxes);
            iVector.Add(pVecSelectedRuleIdxes, &ruleIdx);
            success = 1;
            pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
            itUserCondValue = iHashMap.NewIterator(pRule->pmapUserCondValue);
            while (itUserCondValue->HasNext(itUserCondValue)) {
                node = (HashNode *)itUserCondValue->GetNext(itUserCondValue);
                condAttrIdx = *(int *)node->key;
                psetCondValueIdxes = *(HashSet **)node->value;
                condValueIdx = getInitValue(pInst, pInst->queryUserIdx, condAttrIdx);
                if (!iHashSet.Contains(psetCondValueIdxes, &condValueIdx)) {
                    found = 0;
                    itCondValueIdxes = iHashSet.NewIterator(psetCondValueIdxes);
                    while (itCondValueIdxes->HasNext(itCondValueIdxes)) {
                        condValueIdx = *(int *)itCondValueIdxes->GetNext(itCondValueIdxes);
                        ppSetRuleIdxes2 = (HashSet **)iHashBasedTable.Get(pTableTargetAV2Rule, &condAttrIdx, &condValueIdx);
                        if (ppSetRuleIdxes2 == NULL) {
                            continue;
                        }
                        itRuleIdxes2 = iHashSet.NewIterator(*ppSetRuleIdxes2);
                        while (itRuleIdxes2->HasNext(itRuleIdxes2)) {
                            ruleIdx2 = *(int *)itRuleIdxes2->GetNext(itRuleIdxes2);
                            pRule2 = (Rule *)iVector.GetElement(pVecRules, ruleIdx2);
                            if (iHashMap.Size(pRule2->pmapUserCondValue) == 0) {
                                iVector.Insert(pVecSelectedRuleIdxes, &ruleIdx2);
                                found = 1;
                                break;
                            }
                        }
                        iHashSet.DeleteIterator(itRuleIdxes2);
                        if (found) {
                            break;
                        }
                    }
                    iHashSet.DeleteIterator(itCondValueIdxes);
                    if (!found) {
                        success = 0;
                        break;
                    }
                }
            }
            iHashMap.DeleteIterator(itUserCondValue);
            if (success) {
                break;
            }
            iVector.Clear(pVecSelectedRuleIdxes);
        }
    }
    if (iVector.Size(pVecSelectedRuleIdxes) == 0) {
        return (ACoACResult){.code = ACoAC_RESULT_UNKNOWN};
    }
    return getReachableResultFromRules(pInst, pVecSelectedRuleIdxes);
}