#include "acoac_io.h"
#include "acoac_utils.h"

#define USERS "Users"
#define ATTRIBUTES "Attributes"
#define DEFAULT_VALUE "Default Value"
#define UAV "UAV"
#define RULES "Rules"
#define SPEC "Spec"

/**
 * Write the users of an ACoAC instance to a file.
 * The format is:
 *      Users
 *      u u ... u;
 * where Users is the keyword, u is a user in the user set, and each user is on a separate line, with the last user ending with a semicolon.
 * 
 * @param fp[in]: The file pointer
 * @param pInst[in]: The ACoAC instance
 * @return 0 if write successfully, -1 if failed
 */
static int writeUsers(FILE *fp, ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, DEBUG, "[Writing] Writing users\n");
    if (pInst->pVecUserIndices == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Users is a NULL pointer\n");
        return -1;
    }

    fprintf(fp, USERS);
    int i = 0, userNum = iVector.Size(pInst->pVecUserIndices);
    if (userNum == 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The set of users is empty\n");
        return -1;
    }
    fprintf(fp, "\n%s", istrCollection.GetElement(pscUsers, *(int *)iVector.GetElement(pInst->pVecUserIndices, i++)));
    while (i < userNum) {
        fprintf(fp, " %s", istrCollection.GetElement(pscUsers, *(int *)iVector.GetElement(pInst->pVecUserIndices, i++)));
    }
    fprintf(fp, ";\n\n");
    return 0;
}

/**
 * Write the attributes and default values of an ACoAC instance to a file.
 * The format of the attributes is:
 *       Attributes
 *       boolean: a a ... a
 *       string: a a ... a
 *       int: a a ... a;
 * where Attributes is the keyword, boolean, string, and int are the data types of the attributes, and a is an attribute in the attribute set.
 * 
 * The format of the default values of the attributes is:
 *       Default Value
 *       a: dv
 *       a: dv
 *       ...
 *       a: dv;
 * where Default Value is the keyword, a is an attribute in the attribute set, and dv is the default value of the attribute a.
 * Each default value is on a separate line, with the last default value ending with a semicolon.
 * Note: If the attribute is of type boolean and the default value is false, or the attribute is of type string and the default value is an empty string, or the attribute is of type int and the default value is 0, then the default value is not written.
 * 
 * @param fp[in]: The file pointer
 * @param pInst[in]: The ACoAC instance
 * @return 0 if write successfully, -1 if failed
 */
static int writeAttrsAndDefaultValues(FILE *fp, ACoACInstance *pInst) {
    if (iHashMap.Size(pInst->pMapAttr2Dom) == 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The set of attributes is empty\n");
        return -1;
    }

    HashSet *attrs[3];
    int i = 0;
    for (i = 0; i < 3; i++) {
        attrs[i] = iHashSet.Create(sizeof(int), IntHashCode, IntEqual);
    }

    HashNodeIterator *itmapAttr2Dom = iHashMap.NewIterator(pInst->pMapAttr2Dom);
    HashNode *node;
    int *pAttrIdx;
    AttrType attrType;
    while (itmapAttr2Dom->HasNext(itmapAttr2Dom)) {
        node = itmapAttr2Dom->GetNext(itmapAttr2Dom);
        pAttrIdx = (int *)node->key;
        attrType = getAttrTypeByIdx(*pAttrIdx);
        iHashSet.Add(attrs[attrType], pAttrIdx);
    }
    iHashMap.DeleteIterator(itmapAttr2Dom);

    // Write the attributes and their data types
    fprintf(fp, ATTRIBUTES);
    HashSetIterator *itHashSet;
    char *typename;
    for (i = 0; i < 3; i++) {
        typename = (i == 0) ? "boolean" : (i == 1) ? "string" : "int";
        if (!iHashSet.Size(attrs[i])) {
            continue;
        }
        fprintf(fp, "\n%s:", typename);
        itHashSet = iHashSet.NewIterator(attrs[i]);
        while (itHashSet->HasNext(itHashSet)) {
            fprintf(fp, " %s", istrCollection.GetElement(pscAttrs, *(int *)itHashSet->GetNext(itHashSet)));
        }
        iHashSet.DeleteIterator(itHashSet);
    }
    fprintf(fp, ";\n\n");

    // Write the default values of the attributes
    fprintf(fp, DEFAULT_VALUE);
    int defaultValueIdx;
    int f = 0;
    for (i = 0; i < 3; i++) {
        if (!iHashSet.Size(attrs[i])) {
            continue;
        }
        itHashSet = iHashSet.NewIterator(attrs[i]);
        while (itHashSet->HasNext(itHashSet)) {
            pAttrIdx = (int *)itHashSet->GetNext(itHashSet);
            defaultValueIdx = *(int *)iHashMap.Get(pmapAttr2DefVal, pAttrIdx);
            if (defaultValueIdx) {
                fprintf(fp, "\n%s: %s", istrCollection.GetElement(pscAttrs, *pAttrIdx), getValueByIndex(getAttrTypeByIdx(*pAttrIdx), defaultValueIdx));
                f = 1;
            }
        }
        iHashSet.DeleteIterator(itHashSet);
    }
    if (f) {
        fprintf(fp, ";\n\n");
    } else {
        fprintf(fp, "\n;\n\n");
    }
    for (i = 0; i < 3; i++) {
        iHashSet.Finalize(attrs[i]);
    }
    return 0;
}

/**
 * Write the user attribute values of the initial state of an ACoAC instance to a file.
 * The format is:
 *      UAV
 *      (u, a, v)
 *      (u, a, v)
 *      ...
 *      (u, a, v);
 * where UAV is the keyword, u is a user, a is an attribute, and v is a value of the attribute a.
 * Each group of user attribute values is on a separate line, with the last group ending with a semicolon.
 * 
 * @param fp[in]: The file pointer
 * @param pInst[in]: The ACoAC instance
 * @return 0 if write successfully, -1 if failed
 */
static int writeUAVs(FILE *fp, ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, DEBUG, "[Writing] Writing UAV\n");
    if (pInst->pTableInitState == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The initial state is a NULL pointer\n");
        return -1;
    }

    fprintf(fp, UAV);
    if (iHashMap.Size(pInst->pTableInitState->pRowMap) == 0) {
        fprintf(fp, "\n;\n\n");
        return 0;
    }
    int f = 0;
    int attrIdx, valueIdx;
    char *user, *attr, *value;
    HashNodeIterator *itUsers = iHashMap.NewIterator(pInst->pTableInitState->pRowMap), *itAttrs;
    HashNode *nodeUsers, *nodeAttrs;
    HashMap *pmapAttrValues;
    while (itUsers->HasNext(itUsers)) {
        nodeUsers = itUsers->GetNext(itUsers);
        user = istrCollection.GetElement(pscUsers, *(int *)nodeUsers->key);

        pmapAttrValues = *(HashMap **)nodeUsers->value;
        itAttrs = iHashMap.NewIterator(pmapAttrValues);
        while (itAttrs->HasNext(itAttrs)) {
            nodeAttrs = itAttrs->GetNext(itAttrs);
            valueIdx = *(int *)nodeAttrs->value;
            if (valueIdx) {
                attrIdx = *(int *)nodeAttrs->key;
                attr = istrCollection.GetElement(pscAttrs, attrIdx);
                value = getValueByIndex(getAttrTypeByIdx(attrIdx), valueIdx);
                fprintf(fp, "\n(%s, %s, %s)", user, attr, value);
                f = 1;
            }
        }
        iHashMap.DeleteIterator(itAttrs);
    }
    iHashMap.DeleteIterator(itUsers);
    if (f) {
        fprintf(fp, ";\n\n");
    } else {
        fprintf(fp, "\n;\n\n");
    }
    return 0;
}

/**
 * Write the rules of an ACoAC instance to a file.
 * The format is:
 *      Rules
 *      r
 *      r
 *      ...
 *      r;
 * where Rules is the keyword, r is a rule in the rule set, and each rule is on a separate line, with the last rule ending with a semicolon.
 * 
 * @param fp[in]: The file pointer
 * @param pInst[in]: The ACoAC instance
 * @return 0 if write successfully, -1 if failed
 */
static int writeRules(FILE *fp, ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, DEBUG, "[Writing] Writing rules\n");
    if (pVecRules == NULL || pInst->pSetRuleIdxes == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The set of rules is a NULL pointer\n");
        return -1;
    }

    fprintf(fp, RULES);;
    if (iHashSet.Size(pInst->pSetRuleIdxes) == 0) {
        fprintf(fp, "\n;\n\n");
        return 0;
    }

    int ruleIdx;
    char *rStr;
    Rule *pRule;
    HashSetIterator *itRules = iHashSet.NewIterator(pInst->pSetRuleIdxes);
    while (itRules->HasNext(itRules)) {
        ruleIdx = *(int *)itRules->GetNext(itRules);
        pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
        rStr = RuleToString(&pRule);
        fprintf(fp, "\n%s", rStr);
        free(rStr);
    }
    iHashSet.DeleteIterator(itRules);
    fprintf(fp, ";\n\n");
    return 0;
}

/**
 * Write the safety query of an ACoAC instance to a file.
 * The format is:
 *      Spec
 *      (u, a = v, a = v, ..., a = v);
 * where Spec is the keyword, u is the target user, a is an attribute, and v is a value of the attribute a.
 * 
 * 
 * @param fp[in]: The file pointer
 * @param pInst[in]: The ACoAC instance
 * @return 0 if write successfully, -1 if failed
 */
static int writeSpec(FILE *fp, ACoACInstance *pInst) {
    logACoAC(__func__, __LINE__, 0, DEBUG, "[Writing] Writing spec\n");
    if (pInst->pmapQueryAVs == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The query is a NULL pointer\n");
        return -1;
    }
    if (iHashMap.Size(pInst->pmapQueryAVs) == 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] The set of query attribute-value pairs is empty\n");
        return -1;
    }

    fprintf(fp, SPEC);
    fprintf(fp, "\n(%s", istrCollection.GetElement(pscUsers, pInst->queryUserIdx));

    // 遍历查询属性值集合，写入文件
    HashNodeIterator *itAttrs = iHashMap.NewIterator(pInst->pmapQueryAVs);
    int attrIdx;
    char *attr, *value;
    HashNode *node;
    while (itAttrs->HasNext(itAttrs)) {
        node = itAttrs->GetNext(itAttrs);
        attrIdx = *(int *)node->key;
        attr = istrCollection.GetElement(pscAttrs, attrIdx);
        value = getValueByIndex(getAttrTypeByIdx(attrIdx), *(int *)node->value);
        fprintf(fp, ", %s = %s", attr, value);
    }
    iHashMap.DeleteIterator(itAttrs);
    fprintf(fp, ");\n");
    return 0;
}

int writeACoACInstance(ACoACInstance *pInst, char *filename) {
    logACoAC(__func__, __LINE__, 0, DEBUG, "[Writing] writing ACoAC instance into file %s\n", filename);
    if (pInst == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Writing failed, the given ACoAC instance is a NULL pointer\n");
        return -1;
    }
    if (filename == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Writing failed, the given filename is a NULL pointer\n");
        return -1;
    }
    // 检查文件是否存在，如果存在则删除
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        fclose(fp);
        if (remove(filename) != 0) {
            logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] File %s exists but failed to delete\n", filename);
            return -1;
        }
    }

    // 创建文件，如果文件所在目录不存在，则创建目录
    fp = fopen(filename, "w");
    if (fp == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to create file %s\n", filename);
        return -1;
    }

    int ret;
    if ((ret = writeUsers(fp, pInst)) != 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to write users\n");
        fclose(fp);
        return ret;
    }
    if ((ret = writeAttrsAndDefaultValues(fp, pInst)) != 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to write attributes and default values\n");
        fclose(fp);
        return ret;
    }
    if ((ret = writeUAVs(fp, pInst)) != 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to write user attribute values\n");
        fclose(fp);
        return ret;
    }
    if ((ret = writeRules(fp, pInst)) != 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to write rules\n");
        fclose(fp);
        return ret;
    }
    if ((ret = writeSpec(fp, pInst)) != 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "[Writing] Failed to write special attributes\n");
        fclose(fp);
        return ret;
    }
    fclose(fp);
    logACoAC(__func__, __LINE__, 0, INFO, "[Writing] Successfully wrote ACoAC instance into file %s\n", filename);
    return 0;
}
