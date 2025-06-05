#include "acoac_inst.h"
#include "acoac_utils.h"
#include "ccl/ccl_internal.h"

#include <time.h>

strCollection *pscUsers = NULL;

Dictionary *pdictUser2Index = NULL;

strCollection *pscAttrs = NULL;

Dictionary *pdictAttr2Index = NULL;

strCollection *pscValues = NULL;

Dictionary *pdictValue2Index = NULL;

HashMap *pmapAttr2Type = NULL;

HashMap *pmapAttr2DefVal = NULL;

Vector *pVecRules = NULL;

/**
 * Hash code for a rule index.
 * If two rule indices point to two rules with the same content, they should have the same hash code.
 * 
 * @param pRuleIdx[in] A pointer to the rule index
 * @return The hash code of the rule index
 */
static unsigned int RuleIdxHashCode(void *pRuleIdx) {
    int ruleIdx = *(int *)pRuleIdx;
    if (ruleIdx < 0 || ruleIdx >= iVector.Size(pVecRules)) {
        logACoAC(__func__, __LINE__, 0, ERROR, "rule index out of range: %d (size: %d)\n", ruleIdx, iVector.Size(pVecRules));
        exit(-1);
    }
    Rule *pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
    return iRule.HashCode(pRule);
}

/**
 * Check if two rule indices point to two rules with the same content.
 * 
 * @param pRuleIdx1[in] A pointer to the first rule index
 * @param pRuleIdx2[in] A pointer to the second rule index
 * @return 1 if the two rule indices point to two rules with the same content, 0 otherwise
 */
static int RuleIdxEqual(void *pRuleIdx1, void *pRuleIdx2) {
    int ruleIdx1 = *(int *)pRuleIdx1;
    int ruleIdx2 = *(int *)pRuleIdx2;
    if (ruleIdx1 < 0 || ruleIdx1 >= iVector.Size(pVecRules) || ruleIdx2 < 0 || ruleIdx2 >= iVector.Size(pVecRules)) {
        logACoAC(__func__, __LINE__, 0, ERROR, "rule index out of range: %d, %d (size: %d)\n", ruleIdx1, ruleIdx2, iVector.Size(pVecRules));
        exit(-1);
    }
    Rule *pRule1 = (Rule *)iVector.GetElement(pVecRules, ruleIdx1);
    Rule *pRule2 = (Rule *)iVector.GetElement(pVecRules, ruleIdx2);
    return iRule.Equal(pRule1, pRule2);
}

void initGlobalVars() {
    pscUsers = istrCollection.Create(2);
    pdictUser2Index = iDictionary.Create(sizeof(int), 2);
    pscAttrs = istrCollection.Create(0);
    pdictAttr2Index = iDictionary.Create(sizeof(int), 0);
    pscValues = istrCollection.Create(0);
    pdictValue2Index = iDictionary.Create(sizeof(int), 0);
    pmapAttr2Type = iHashMap.Create(sizeof(int), sizeof(int), IntHashCode, IntEqual);
    pmapAttr2DefVal = iHashMap.Create(sizeof(int), sizeof(int), IntHashCode, IntEqual);
    pVecRules = iVector.Create(sizeof(Rule), 0);
}

void finalizeGlobalVars() {
    istrCollection.Finalize(pscUsers);
    iDictionary.Finalize(pdictUser2Index);
    istrCollection.Finalize(pscAttrs);
    iDictionary.Finalize(pdictAttr2Index);
    istrCollection.Finalize(pscValues);
    iDictionary.Finalize(pdictValue2Index);
    iHashMap.Finalize(pmapAttr2Type);
    iHashMap.Finalize(pmapAttr2DefVal);
    for (int i = 0; i < iVector.Size(pVecRules); i++) {
        Rule *pRule = (Rule *)iVector.GetElement(pVecRules, i);
        iHashSet.Finalize(pRule->adminCond);
        iHashSet.Finalize(pRule->userCond);
        iHashMap.Finalize(pRule->pmapUserCondValue);
    }
    iVector.Finalize(pVecRules);
}

ACoACInstance *createACoACInstance() {
    ACoACInstance *pInst = (ACoACInstance *)malloc(sizeof(ACoACInstance));
    if (pInst == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to allocate memory for ACoACInstance\n");
        return NULL;
    }

    pInst->pVecUserIndices = iVector.Create(sizeof(int), 2);

    pInst->pMapAttr2Dom = iHashMap.Create(sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual);
    iHashMap.SetDestructValue(pInst->pMapAttr2Dom, iHashSet.DestructPointer); //0xc72fd0
    pInst->pTableInitState = iHashBasedTable.Create(sizeof(int), sizeof(int), sizeof(int), IntHashCode, IntEqual, IntHashCode, IntEqual);

    pInst->pSetRuleIdxes = iHashSet.Create(sizeof(int), RuleIdxHashCode, RuleIdxEqual);

    pInst->pTableTargetAV2Rule = iHashBasedTable.Create(sizeof(int), sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual, IntHashCode, IntEqual);
    iHashBasedTable.SetDestructValue(pInst->pTableTargetAV2Rule, iHashSet.DestructPointer);
    pInst->pTablePrecond2Rule = iHashBasedTable.Create(sizeof(int), sizeof(int), sizeof(HashSet *), IntHashCode, IntEqual, IntHashCode, IntEqual);
    iHashBasedTable.SetDestructValue(pInst->pTablePrecond2Rule, iHashSet.DestructPointer);

    pInst->queryUserIdx = -1;
    pInst->pmapQueryAVs = iHashMap.Create(sizeof(int), sizeof(int), IntHashCode, IntEqual);
    return pInst;
}

void finalizeACoACInstance(ACoACInstance *pInst) {
    iVector.Finalize(pInst->pVecUserIndices);
    iHashMap.Finalize(pInst->pMapAttr2Dom);
    iHashBasedTable.Finalize(pInst->pTableInitState);
    iHashSet.Finalize(pInst->pSetRuleIdxes);
    iHashBasedTable.Finalize(pInst->pTableTargetAV2Rule);
    iHashBasedTable.Finalize(pInst->pTablePrecond2Rule);
    iHashMap.Finalize(pInst->pmapQueryAVs);
    free(pInst);
}

/**
 * Get the attribute name from the attribute index.
 * 
 * @param pAttrIdx[in] A pointer to the attribute index
 * @return A free-able attribute name
 */
static char *attrIdxToString(void *pAttrIdx) {
    char *attr = istrCollection.GetElement(pscAttrs, *(int *)pAttrIdx);
    return strdup(attr);
}

/**
 * Convert an integer value index to a string.
 * 
 * @param pIntValIdx[in] A pointer to the integer value index
 * @return A free-able integer value string
 */
static char *intValueIdxToString(void *pIntValIdx) {
    char *buffer = (char *)malloc(32);
    sprintf(buffer, "%d", *(int *)pIntValIdx);
    return buffer;
}

/**
 * Convert a boolean value index to a string.
 * 
 * @param pBoolValIdx[in] A pointer to the boolean value index
 * @return A free-able boolean value string
 */
static char *boolValueIdxToString(void *pBoolValIdx) {
    return *(int *)pBoolValIdx == 0 ? strdup("false") : strdup("true");
}

/**
 * Get the string value from the string value index.
 * 
 * @param pStrValIdx[in] A pointer to the string value index
 * @return A free-able string value string
 */
static char *stringValueIdxToString(void *pStrValIdx) {
    return strdup(istrCollection.GetElement(pscValues, *(int *)pStrValIdx));
}

/**
 * Update the attribute domain @{pMapAttr2Dom} of the ACoAC instance.
 * 
 * @param pInst[in] The ACoAC instance
 * @param attrType[in] The type of the attribute
 * @param attrIdx[in] The index of the attribute
 * @param valueIdx[in] The index of the value
 */
static void addAV(ACoACInstance *pInst, AttrType attrType, int attrIdx, int valueIdx) {
    HashSet *pDom, **ppDom = iHashMap.Get(pInst->pMapAttr2Dom, &attrIdx);
    if (ppDom == NULL) {
        pDom = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
        ppDom = &pDom;
        iHashSet.SetElementToString(*ppDom, attrType == BOOLEAN ? boolValueIdxToString : attrType == INTEGER ? intValueIdxToString
                                                                                                             : stringValueIdxToString);
        iHashMap.Put(pInst->pMapAttr2Dom, &attrIdx, ppDom);
    }
    iHashSet.Add(*ppDom, &valueIdx);
}

int addUAV(ACoACInstance *pInst, char *user, char *attr, char *value) {
    int userIdx = getUserIndex(user);
    int attrIdx = getAttrIndex(attr);
    AttrType attrType = getAttrTypeByIdx(attrIdx);
    int valueIdx;
    int ret = getValueIndex(attrType, value, &valueIdx);
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to add user attribute value pair: %s, %s, %s\n", user, attr, value);
        return ret;
    }
    addAV(pInst, attrType, attrIdx, valueIdx);
    iHashBasedTable.Put(pInst->pTableInitState, &userIdx, &attrIdx, &valueIdx);
    return 0;
}

int addUAVByIdx(ACoACInstance *pInst, int userIdx, int attrIdx, int valueIdx) {
    AttrType attrType = getAttrTypeByIdx(attrIdx);
    addAV(pInst, attrType, attrIdx, valueIdx);
    iHashBasedTable.Put(pInst->pTableInitState, &userIdx, &attrIdx, &valueIdx);
    return 0;
}

int addRule(ACoACInstance *pInst, int ruleIdx) {
    if (iHashSet.Contains(pInst->pSetRuleIdxes, &ruleIdx)) {
        logACoAC(__func__, __LINE__, 0, DEBUG, "Rule exists\n");
        return 0;
    }
    iHashSet.Add(pInst->pSetRuleIdxes, &ruleIdx);
    Rule *r = (Rule *)iVector.GetElement(pVecRules, ruleIdx);

    // 建立从TargetAttr与TargetValue到Rule的映射
    HashSet *psetRuleIdxes, **ppsetRuleIdxes = iHashBasedTable.Get(pInst->pTableTargetAV2Rule, &r->targetAttrIdx, &r->targetValueIdx);
    if (ppsetRuleIdxes == NULL) {
        psetRuleIdxes = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
        ppsetRuleIdxes = &psetRuleIdxes;
        iHashBasedTable.Put(pInst->pTableTargetAV2Rule, &r->targetAttrIdx, &r->targetValueIdx, ppsetRuleIdxes);
    }
    iHashSet.Add(*ppsetRuleIdxes, &ruleIdx);

    // 建立从用户条件属性到Rule的映射
    if (r->pmapUserCondValue != NULL) {
        int *pAttrIdx, *pValueIdx;
        HashNode *nodeUserCondValue;
        HashSet *psetVals;

        HashSet *psetRulesOfAV, **ppsetRulesOfAV;
        HashNodeIterator *itUserCondValue = iHashMap.NewIterator(r->pmapUserCondValue);
        HashSetIterator *itVals;
        while (itUserCondValue->HasNext(itUserCondValue)) {
            nodeUserCondValue = (HashNode *)itUserCondValue->GetNext(itUserCondValue);
            pAttrIdx = (int *)nodeUserCondValue->key;
            psetVals = *(HashSet **)nodeUserCondValue->value;

            itVals = iHashSet.NewIterator(psetVals);
            while (itVals->HasNext(itVals)) {
                pValueIdx = (int *)itVals->GetNext(itVals);
                ppsetRulesOfAV = iHashBasedTable.Get(pInst->pTablePrecond2Rule, pAttrIdx, pValueIdx);
                if (ppsetRulesOfAV == NULL) {
                    psetRulesOfAV = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                    ppsetRulesOfAV = &psetRulesOfAV;
                    iHashBasedTable.Put(pInst->pTablePrecond2Rule, pAttrIdx, pValueIdx, ppsetRulesOfAV);
                }
                iHashSet.Add(*ppsetRulesOfAV, &ruleIdx);
            }
            iHashSet.DeleteIterator(itVals);
        }
        iHashMap.DeleteIterator(itUserCondValue);
    }
    AttrType attrType = getAttrTypeByIdx(r->targetAttrIdx);
    addAV(pInst, attrType, r->targetAttrIdx, r->targetValueIdx);
    return 1;
}

int getUserIndex(char *user) {
    int *i = (int *)iDictionary.GetElement(pdictUser2Index, user);
    if (i == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "cannot find user %s, please check the policy again\n", user);
        return -1;
    }
    return *i;
}

int getAttrIndex(char *attr) {
    int *i = (int *)iDictionary.GetElement(pdictAttr2Index, attr);
    if (i == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "cannot find attribute %s, please check the policy again\n", attr);
        return -1;
    }
    return *i;
}

int getValueIndex(AttrType attrType, char *value, int *pValueIdx) {
    int valueIdx, *pValueIdxLocal;
    char *pEnd;
    switch (attrType) {
    case BOOLEAN:
        *pValueIdx = (strcmp(value, "false") == 0) ? 0 : 1;
        return 0;
    case STRING:
        pValueIdxLocal = iDictionary.GetElement(pdictValue2Index, value);
        if (pValueIdxLocal != NULL) {
            *pValueIdx = *pValueIdxLocal;
            return 0;
        }
        valueIdx = istrCollection.Size(pscValues);
        iDictionary.Insert(pdictValue2Index, value, &valueIdx);
        istrCollection.Add(pscValues, value);
        *pValueIdx = valueIdx;
        return 0;
    case INTEGER:
        valueIdx = (int)strtod(value, &pEnd);
        if (*pEnd != '\0') {
            logACoAC(__func__, __LINE__, 0, ERROR, "The value %s should be an integer\n", value);
            return -1;
        }
        *pValueIdx = valueIdx;
        return 0;
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "The attribute datatype should be %d(boolean), %d(string), or %d(integer)\n", BOOLEAN, STRING, INTEGER);
        return -1;
    }
}

AttrType getAttrType(char *attr) {
    return getAttrTypeByIdx(getAttrIndex(attr));
}

AttrType getAttrTypeByIdx(int attrIdx) {
    int *pAttrTypeIdx = (int *)iHashMap.Get(pmapAttr2Type, &attrIdx);
    if (pAttrTypeIdx == NULL) {
        fprintf(stderr, "error: cannot find attribute index %d, please check the policy again\n", attrIdx);
        exit(-1);
    }
    return *pAttrTypeIdx;
}

char *getValueByIndex(AttrType attrType, int valueIdx) {
    switch (attrType) {
    case BOOLEAN:
        return valueIdx == 0 ? strdup("false") : strdup("true");
    case STRING:
        return strdup(istrCollection.GetElement(pscValues, valueIdx));
    case INTEGER:
        return intValueIdxToString(&valueIdx);
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "The attribute datatype should be %d(boolean), %d(string), or %d(integer)\n", BOOLEAN, STRING, INTEGER);
        exit(-1);
    }
}

int getInitValue(ACoACInstance *pInst, int userIdx, int attrIdx) {
    int *pValueIdx = (int *)iHashBasedTable.Get(pInst->pTableInitState, &userIdx, &attrIdx);
    if (pValueIdx != NULL) {
        return *pValueIdx;
    }
    int *pDefValIdx = (int *)iHashMap.Get(pmapAttr2DefVal, &attrIdx);
    if (pDefValIdx == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "cannot find attribute %s in the default value map\n", istrCollection.GetElement(pscAttrs, attrIdx));
        exit(-1);
    }
    return *pDefValIdx;
}

/**
 * Get a string representation of the attribute-value pairs needed to satisfy the user condition of a rule.
 * 
 * @param r[in] The rule
 * @return A free-able string representation of the attribute-value pairs
 */
static char *solution(Rule *r) {
    if (r->pmapUserCondValue == NULL) {
        return strdup("not defined");
    }
    if (iHashMap.Size(r->pmapUserCondValue) == 0) {
        return strdup("none");
    }
    HashNodeIterator *it = iHashMap.NewIterator(r->pmapUserCondValue);
    HashNode *node;
    int *pAttrIdx;
    AttrType attrType;
    HashSet *psetVals;
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        pAttrIdx = (int *)node->key;
        psetVals = *(HashSet **)node->value;
        attrType = getAttrTypeByIdx(*pAttrIdx);
        switch (attrType) {
        case BOOLEAN:
            iHashSet.SetElementToString(psetVals, boolValueIdxToString);
            break;
        case INTEGER:
            iHashSet.SetElementToString(psetVals, intValueIdxToString);
            break;
        case STRING:
            iHashSet.SetElementToString(psetVals, stringValueIdxToString);
            break;
        default:
            logACoAC(__func__, __LINE__, 0, ERROR, "The attribute datatype should be %d(boolean), %d(string), or %d(integer)\n", BOOLEAN, STRING, INTEGER);
            exit(-1);
        }
    }
    iHashMap.DeleteIterator(it);
    return mapToString(r->pmapUserCondValue, attrIdxToString, iHashSet.ToString);
}

/**
 * Get a string representation of the comparison operator.
 * 
 * @param op[in] The comparison operator
 * @return A free-able string representation of the comparison operator
 */
static char *getOpStr(comparisonOperator op) {
    switch (op) {
    case EQUAL:
        return "=";
    case NOT_EQUAL:
        return "!=";
    case GREATER_THAN:
        return ">";
    case LESS_THAN:
        return "<";
    case GREATER_THAN_OR_EQUAL:
        return ">=";
    case LESS_THAN_OR_EQUAL:
        return "<=";
    default:
        return "unknown";
    }
}

/**
 * Get a string representation of a administrative condition or a user condition.
 * 
 * @param condition[in] A condition
 * @return A free-able string representation of the condition
 */
static char *conditionToString(HashSet *condition) {
    char *atomCondStr = NULL;
    if (iHashSet.Size(condition) == 0) {
        atomCondStr = strdup("TRUE");
    } else {
        HashSetIterator *it = iHashSet.NewIterator(condition);
        char *opStr, *attr, *value;
        int first = 1, cur = 0;
        while (it->HasNext(it)) {
            AtomCondition *atomCond = it->GetNext(it);
            attr = istrCollection.GetElement(pscAttrs, atomCond->attribute);
            value = getValueByIndex(getAttrTypeByIdx(atomCond->attribute), atomCond->value);
            opStr = getOpStr(atomCond->op);
            if (first) {
                first = 0;
                atomCondStr = (char *)malloc(strlen(attr) + strlen(opStr) + strlen(value) + 1);
            } else {
                atomCondStr = (char *)realloc(atomCondStr, cur + strlen(attr) + strlen(opStr) + strlen(value) + 4);
                atomCondStr[cur++] = ' ';
                atomCondStr[cur++] = '&';
                atomCondStr[cur++] = ' ';
            }
            memcpy(atomCondStr + cur, attr, strlen(attr));
            cur += strlen(attr);
            memcpy(atomCondStr + cur, opStr, strlen(opStr));
            cur += strlen(opStr);
            memcpy(atomCondStr + cur, value, strlen(value));
            cur += strlen(value);
            free(value);
        }
        atomCondStr[cur++] = '\0';
        iHashSet.DeleteIterator(it);
    }
    return atomCondStr;
}

/**
 * Get a string representation of a rule.
 * 
 * @param ppRule[in] A pointer to the rule
 * @return A free-able string representation of the rule
 */
char *RuleToString(void *ppRule) {
    Rule *r = *(Rule **)ppRule;

    char *adminCondStr = conditionToString(r->adminCond);
    char *userCondStr = conditionToString(r->userCond);
    char *targetAttr = istrCollection.GetElement(pscAttrs, r->targetAttrIdx);
    AttrType attrType = getAttrTypeByIdx(r->targetAttrIdx);
    char *targetValue = getValueByIndex(attrType, r->targetValueIdx);
    char *solutionStr = solution(r);

    int adminCondStrLen = strlen(adminCondStr);
    int userCondStrLen = strlen(userCondStr);
    int targetAttrLen = strlen(targetAttr);
    int targetValueLen = strlen(targetValue);
    int solutionStrLen = strlen(solutionStr);

    char *ret = (char *)malloc(
        adminCondStrLen + userCondStrLen + targetAttrLen + targetValueLen + solutionStrLen + 14);
    *ret = '\0';
    int cur = 0;
    ret[cur++] = '(';
    memcpy(ret + cur, adminCondStr, adminCondStrLen);
    cur += adminCondStrLen;
    ret[cur++] = ',';
    ret[cur++] = ' ';
    memcpy(ret + cur, userCondStr, userCondStrLen);
    cur += userCondStrLen;
    ret[cur++] = ',';
    ret[cur++] = ' ';
    memcpy(ret + cur, targetAttr, targetAttrLen);
    cur += targetAttrLen;
    ret[cur++] = ',';
    ret[cur++] = ' ';
    memcpy(ret + cur, targetValue, targetValueLen);
    cur += targetValueLen;
    ret[cur++] = ')';
    ret[cur++] = '\t';
    ret[cur++] = '/';
    ret[cur++] = '*';
    memcpy(ret + cur, solutionStr, solutionStrLen);
    cur += solutionStrLen;
    ret[cur++] = '*';
    ret[cur++] = '/';
    ret[cur++] = '\0';

    free(adminCondStr);
    free(userCondStr);
    free(solutionStr);
    free(targetValue);
    return ret;
}

void init(ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, INFO, "[Start] rules initialization\n");
    clock_t initStartTime = clock();

    /*计算值域*/
    HashMap *map = iHashMap.Create(sizeof(int), sizeof(int), IntHashCode, IntEqual);
    int userNum = iVector.Size(pInst->pVecUserIndices);
    int *pAttrIdx;
    int flag, *pFlag;
    if (iHashMap.Size(pInst->pTableInitState->pRowMap) == userNum) {
        int first = 1;
        HashNodeIterator *itInitstate = iHashMap.NewIterator(pInst->pTableInitState->pRowMap), *itAVs;
        HashNode *nodeInitState, *nodeAVs;
        HashMap *pmapAVs;
        while (itInitstate->HasNext(itInitstate)) {
            nodeInitState = (HashNode *)itInitstate->GetNext(itInitstate);
            pmapAVs = *(HashMap **)nodeInitState->value;

            itAVs = iHashMap.NewIterator(pmapAVs);
            while (itAVs->HasNext(itAVs)) {
                nodeAVs = (HashNode *)itAVs->GetNext(itAVs);
                pAttrIdx = (int *)nodeAVs->key;
                if (first) {
                    flag = 1;
                    iHashMap.Put(map, pAttrIdx, &flag);
                } else {
                    pFlag = iHashMap.Get(map, pAttrIdx);
                    if (pFlag != NULL) {
                        *pFlag = *pFlag + 1;
                    }
                }
            }
            if (first) {
                first = 0;
            }
            iHashMap.DeleteIterator(itAVs);
        }
        iHashMap.DeleteIterator(itInitstate);
    }

    HashNodeIterator *itAttr2DefVal = iHashMap.NewIterator(pmapAttr2DefVal);
    HashNode *nodeAttr2DefVal;
    int *pDefValIdx;
    HashSet *pDom, **ppDom;
    AttrType attrType;
    while (itAttr2DefVal->HasNext(itAttr2DefVal)) {
        nodeAttr2DefVal = (HashNode *)itAttr2DefVal->GetNext(itAttr2DefVal);
        pAttrIdx = (int *)nodeAttr2DefVal->key;
        attrType = getAttrTypeByIdx(*pAttrIdx);
        pFlag = iHashMap.Get(map, pAttrIdx);
        if (pFlag == NULL || *pFlag != userNum) {
            pDefValIdx = (int *)nodeAttr2DefVal->value;
            ppDom = iHashMap.Get(pInst->pMapAttr2Dom, pAttrIdx);
            if (ppDom == NULL) {
                pDom = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
                ppDom = &pDom;
                iHashMap.Put(pInst->pMapAttr2Dom, pAttrIdx, ppDom);
                iHashSet.SetElementToString(*ppDom, attrType == BOOLEAN ? boolValueIdxToString : attrType == INTEGER ? intValueIdxToString
                                                                                                                     : stringValueIdxToString);
            }
            iHashSet.Add(*ppDom, pDefValIdx);
        }
    }
    iHashMap.DeleteIterator(itAttr2DefVal);
    iHashMap.Finalize(map);

    HashSet *pSetNewRules = iHashSet.Create(sizeof(int), RuleIdxHashCode, RuleIdxEqual);
    int ruleIdx;
    Rule *r;
    char *rStr;
    HashSetIterator *itRules = iHashSet.NewIterator(pInst->pSetRuleIdxes);
    while (itRules->HasNext(itRules)) {
        ruleIdx = *(int *)itRules->GetNext(itRules);
        r = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        if (iRule.DiscreteCond(r, pInst->pMapAttr2Dom) != -1) {
            iHashSet.Add(pSetNewRules, &ruleIdx);
        } else {
            rStr = RuleToString(&r);
            logACoAC(__func__, __LINE__, 0, DEBUG, "The user condition cannot be satisfied: %s\n", rStr);
            free(rStr);
        }
    }
    iHashSet.DeleteIterator(itRules);

    int nOldRules = iHashSet.Size(pInst->pSetRuleIdxes);

    iHashSet.Clear(pInst->pSetRuleIdxes);
    iHashBasedTable.Clear(pInst->pTableTargetAV2Rule);
    iHashBasedTable.Clear(pInst->pTablePrecond2Rule);

    itRules = iHashSet.NewIterator(pSetNewRules);
    while (itRules->HasNext(itRules)) {
        ruleIdx = *(int *)itRules->GetNext(itRules);
        addRule(pInst, ruleIdx);
    }
    iHashSet.DeleteIterator(itRules);
    iHashSet.Finalize(pSetNewRules);

    int nNewRules = iHashSet.Size(pInst->pSetRuleIdxes);

    clock_t initEndTime = clock();
    double time_spent = (double)(initEndTime - initStartTime) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[End] rules initialization, cost => %.2fms\n", time_spent);
    logACoAC(__func__, __LINE__, 0, INFO, "rules: %d==>%d, difference: %d\n", nOldRules, nNewRules, nOldRules - nNewRules);
}
