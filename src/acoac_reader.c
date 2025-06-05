#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acoac_io.h"
#include "acoac_utils.h"

#define USERS "Users"
#define ATTRIBUTES "Attributes"
#define DEFAULT_VALUE "Default Value"
#define UAV "UAV"
#define RULES "Rules"
#define SPEC "Spec"

/****************************************************************************************************
 * 功能：处理用户列表，添加用户到实例中。
 * 参数：
 *      @line[in]: 用空格隔开的若干个用户名，格式为"User1 User2 User3 ...
 *UserN;" 返回值： 0表示成功，-1表示失败
 ****************************************************************************************************/
static int handleUsers(ACoACInstance *pInst, char *line) {
    char *user;
    int userLen = 0;

    char *token = strtok(line, " ");
    char *nextToken = NULL;
    int lastUser, userNum = 0;

    while (token != NULL) {
        nextToken = strtok(NULL, " ");
        lastUser = (nextToken == NULL);
        user = strtrim(token);
        userLen = strlen(user);
        if (lastUser) {
            if (userLen > 0 && user[userLen - 1] == ';') {
                // Remove trailing semicolon
                user[userLen - 1] = '\0';
                userLen--;
            }
        }

        if (userLen > 0) {
            // Add user to the instance
            if (iDictionary.Insert(pdictUser2Index, user, &userNum)) {
                istrCollection.Add(pscUsers, user);
                iVector.Add(pInst->pVecUserIndices, &userNum);
                userNum++;
                logACoAC(__func__, __LINE__, 0, DEBUG, "Add %d user: %s\n", userNum, user);
            }
        }
        token = nextToken;
    }
}

static int handleAttributes(char *line) {
    if (strcmp(";", line) == 0) {
        return 0;
    }

    char *token = NULL, *type = NULL;
    int start = 0, cur = 0;
    int attrStrTrimmed = 0;
    AttrType attrType;
    int defValIdx = 0, attrNum;

    while (1) {
        if (line[cur] == ':') {
            if (token != NULL) {
                logACoAC(__func__, __LINE__, 0, ERROR, "type already set to %s, line: %s\n", token, line);
                return -1;
            }
            token = (char *)malloc((cur - start + 1) * sizeof(char));
            strncpy(token, line, cur - start);
            token[cur - start] = '\0';
            cur++;
            type = strtrim(token);
            if (strcmp(type, "boolean") == 0) {
                attrType = BOOLEAN;
            } else if (strcmp(type, "string") == 0) {
                attrType = STRING;
                iDictionary.Insert(pdictValue2Index, "", &defValIdx);
                istrCollection.Add(pscValues, "");
            } else if (strcmp(type, "int") == 0) {
                attrType = INTEGER;
            }
            continue;
        }
        if (type != NULL && !attrStrTrimmed) {
            if (isspace(line[cur])) {
                cur++;
                continue;
            }
            attrStrTrimmed = 1;
            start = cur;
        }
        int lastIteration = 0;
        if (line[cur] == '\0') {
            lastIteration = 1;
        }
        if ((line[cur] == ' ' || line[cur] == '\0') && type != NULL) {
            if (line[cur] == '\0' && line[cur - 1] == ';') {
                line[cur - 1] = '\0';
            } else {
                line[cur] = '\0';
            }

            if (!iDictionary.Contains(pdictAttr2Index, line + start)) {
                attrNum = istrCollection.Size(pscAttrs);
                iDictionary.Insert(pdictAttr2Index, line + start, &attrNum);
                istrCollection.Add(pscAttrs, line + start);
                // iVector.Add(pInst->pVecAttrIdxes, &attrNum);
                iHashMap.Put(pmapAttr2Type, &attrNum, &attrType);
                iHashMap.Put(pmapAttr2DefVal, &attrNum, &defValIdx);
                attrNum++;
            }
            start = cur + 1;
        }
        if (lastIteration) {
            break;
        }
        cur++;
    }
    if (token != NULL) {
        free(token);
    }
    return 0;
}

static int handleDefaultValues(char *line) {
    if (strcmp(";", line) == 0) {
        return 0;
    }

    char *attr = strtok(line, ":");
    char *value = strtok(NULL, ":");
    if (attr == NULL || value == NULL || strtok(NULL, ":") != NULL) {
        fprintf(stderr, "Error: default value should be in the form of [attr] : [value]\n");
        exit(-1);
    }
    attr = strtrim(attr);
    value = strtrim(value);
    int valLen = strlen(value);
    if (valLen > 0 && value[valLen - 1] == ';') {
        value[--valLen] = '\0';
    }
    AttrType attrType = getAttrType(attr);
    int attrIdx = getAttrIndex(attr);
    int valueIdx;
    int ret = getValueIndex(attrType, value, &valueIdx);
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to add default value: %s, %s\n", attr, value);
        exit(ret);
    }
    iHashMap.Put(pmapAttr2DefVal, &attrIdx, &valueIdx);
    return 0;
}

static int handleUAV(ACoACInstance *pInst, char *line) {
    if (strcmp(";", line) == 0) {
        return 0;
    }

    char *user = NULL, *attr = NULL, *value = NULL;

    char *p = line;
    user = p;
    int partCount = 0;
    while (1) {
        if (*p == '\0') {
            logACoAC(__func__, __LINE__, 0, ERROR, "UAV should be in the form of ([user], [attr], [value]), missing right bracket\n");
            abort();
            return -1;
        } else if (*p == ',') {
            *p = '\0';
            if (partCount == 0) {
                attr = p + 1;
            } else if (partCount == 1) {
                value = p + 1;
            } else {
                logACoAC(__func__, __LINE__, 0, ERROR, "UAV should be in the form of ([user], [attr], [value]), there should be extact two commas\n");
                abort();
                return -1;
            }
            partCount++;
        } else if (*p == ')') {
            *p = '\0';
            break;
        }
        p++;
    }

    if (user == NULL || attr == NULL || value == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "UAV should be in the form of ([user], [attr], [value]), there should be extact two commas\n");
        abort();
        return -1;
    }

    if (*user != '(') {
        logACoAC(__func__, __LINE__, 0, ERROR, "UAV should be in the form of ([user], [attr], [value]), missing left bracket\n");
        abort();
        return -1;
    }

    int ret = addUAV(pInst, strtrim(user + 1), strtrim(attr), strtrim(value));
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle UAV: %s, %s, %s)\n", user, attr, value);
        exit(ret);
    }
    return 0;
}

static AtomCondition *handleAtomCondition(char *atomCondStr) {
    char *attr = NULL, *value = NULL;
    comparisonOperator op;
    char *p = atomCondStr;
    attr = p;
    int opNum = 0;
    while (1) {
        if (*p == '!' && *(p + 1) == '=') {
            op = NOT_EQUAL;
            *p = '\0';
            p += 2;
            value = p;
            opNum++;
            if (opNum > 1) {
                break;
            }
        } else if (*p == '<') {
            *p = '\0';
            if (*(p + 1) == '=') {
                op = LESS_THAN_OR_EQUAL;
                p += 2;
            } else {
                op = LESS_THAN;
                p++;
            }
            value = p;
            opNum++;
            if (opNum > 1) {
                break;
            }
        } else if (*p == '>') {
            *p = '\0';
            if (*(p + 1) == '=') {
                op = GREATER_THAN_OR_EQUAL;
                p += 2;
            } else {
                op = GREATER_THAN;
                *p = '\0';
                p++;
            }
            value = p;
            opNum++;
            if (opNum > 1) {
                break;
            }
        } else if (*p == '=') {
            op = EQUAL;
            *p = '\0';
            p++;
            value = p;
            opNum++;
            if (opNum > 1) {
                break;
            }
        } else if (*p == '\0') {
            break;
        } else {
            p++;
        }
    }

    if (opNum == 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "atom cond should be form of attr op value, but missing operation in %s\n", atomCondStr);
        exit(-1);
    } else if (opNum > 1) {
        logACoAC(__func__, __LINE__, 0, ERROR, "found multiple operation in %s\n", atomCondStr);
        exit(-1);
    }

    attr = strtrim(attr);
    value = strtrim(value);

    AtomCondition *atomCond = (AtomCondition *)malloc(sizeof(AtomCondition));
    atomCond->attribute = getAttrIndex(attr);
    int ret = getValueIndex(getAttrType(attr), value, &atomCond->value);
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle atom condition: %s\n", atomCondStr);
        exit(ret);
    }
    atomCond->op = op;
    return atomCond;
}

static HashSet *handleCondition(char *condStr) {
    HashSet *condition = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
    if (strcmp("TRUE", condStr) == 0) {
        return condition;
    }

    char *atomCondStr = strtok(condStr, "&");
    AtomCondition *pAtomCond;
    while (atomCondStr != NULL) {
        atomCondStr = strtrim(atomCondStr);
        pAtomCond = handleAtomCondition(atomCondStr);
        iHashSet.Add(condition, pAtomCond);
        free(pAtomCond);
        atomCondStr = strtok(NULL, "&");
    }
    return condition;
}

static int handleRule(ACoACInstance *pInst, char *line) {
    if (strcmp(";", line) == 0) {
        return 0;
    }

    char *adminCondStr = NULL, *userCondStr = NULL, *attr = NULL, *value = NULL;
    char *p = line;
    adminCondStr = p;
    int partCount = 0;
    while (1) {
        if (*p == '\0') {
            logACoAC(__func__, __LINE__, 0, ERROR, "a rule should be form of (admincond, usercond, attr, val), missing right bracket\n");
            abort();
            return -1;
        } else if (*p == ',') {
            *p = '\0';
            if (partCount == 0) {
                userCondStr = p + 1;
            } else if (partCount == 1) {
                attr = p + 1;
            } else if (partCount == 2) {
                value = p + 1;
            } else {
                logACoAC(__func__, __LINE__, 0, ERROR, "a rule should be form of (admincond, usercond, attr, val), there should be extact three commas\n");
                abort();
                return -1;
            }
            partCount++;
        } else if (*p == ')') {
            *p = '\0';
            break;
        }
        p++;
    }
    if (adminCondStr == NULL || userCondStr == NULL || attr == NULL ||
        value == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "a rule should be form of (admincond, usercond, attr, val), there should be extact three commas\n");
        abort();
        return -1;
    }

    if (*adminCondStr != '(') {
        logACoAC(__func__, __LINE__, 0, ERROR, "a rule should be form of (admincond, usercond, attr, val), missing left bracket\n");
        abort();
        return -1;
    }

    adminCondStr = strtrim(adminCondStr + 1);
    userCondStr = strtrim(userCondStr);
    attr = strtrim(attr);
    value = strtrim(value);

    int attrIdx = getAttrIndex(attr);
    int valueIdx;
    int ret = getValueIndex(getAttrType(attr), value, &valueIdx);
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle rule: (%s, %s, %s, %s)\n", adminCondStr, userCondStr, attr, value);
        exit(ret);
    }
    HashSet *adminCond = handleCondition(adminCondStr);
    HashSet *userCond = handleCondition(userCondStr);
    Rule *r = iRule.Create(adminCond, userCond, attrIdx, valueIdx);
    iVector.Add(pVecRules, r);
    free(r);
    int ruleIdx = iVector.Size(pVecRules) - 1;
    addRule(pInst, ruleIdx);
    return 0;
}

static int handleSpec(ACoACInstance *pInst, char *line) {
    if (*line != '(') {
        logACoAC(__func__, __LINE__, 0, ERROR, "spec should starts with (, but it is %s\n", line);
        abort();
        return -1;
    }

    char *queryAV = NULL, *attr = NULL, *value = NULL;
    int attrIdx, valueIdx;
    char *p = line + 1;
    char *queryUser = p;
    int partCount = 0, toBreak = 0;
    while (1) {
        if (*p == '\0') {
            logACoAC(__func__, __LINE__, 0, ERROR, "spec should be form of (user, attr=value, attr=value, ...);, missing right bracket\n");
            abort();
            return -1;
        } else if (*p == ',' || *p == ')') {
            if (*p == ')') {
                toBreak = 1;
            }
            *p = '\0';
            if (partCount != 0 || *p == ')') {
                queryAV = strtrim(queryAV);
                attr = strtok(queryAV, "=");
                value = strtok(NULL, "=");
                if (attr == NULL || value == NULL ||
                    strtok(NULL, "=") != NULL) {
                    logACoAC(__func__, __LINE__, 0, ERROR, "query av should be form of <attr> = <value>, but it is %s\n", queryAV);
                    abort();
                    return -1;
                }
                attr = strtrim(attr);
                value = strdup(strtrim(value));
                attrIdx = getAttrIndex(attr);
                int ret = getValueIndex(getAttrType(attr), value, &valueIdx);
                if (ret) {
                    logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle query av: %s, %s\n", attr, value);
                    exit(ret);
                }
                iHashMap.Put(pInst->pmapQueryAVs, &attrIdx, &valueIdx);
                logACoAC(__func__, __LINE__, 0, INFO, "add query attribute: %s, value: %s\n", attr, value);
                free(value);
            }
            if (toBreak) {
                break;
            }
            queryAV = p + 1;
            partCount++;
        }
        p++;
    }

    queryUser = strtrim(queryUser);
    pInst->queryUserIdx = getUserIndex(queryUser);
    logACoAC(__func__, __LINE__, 0, INFO, "query user: %s\n", queryUser);
    return 0;
}

/****************************************************************************************************
 * 功能：处理一行数据。根据stage的值，调用不同的处理方法。
 *      stage=1时，表示当前行为用户列表，调用handleUsers方法处理；
 *      stage=2时，表示当前行为属性列表，调用handleAttributes方法处理；
 *      stage=3时，表示当前行为属性默认值列表，调用handleDefaultValues方法处理；
 *      stage=4时，表示当前行为用户初始属性值列表，调用handleUAV方法处理；
 *      stage=5时，表示当前行为ACoAC规则，调用handleRules方法处理；
 *      stage=6时，表示当前行为查询语句，调用handleSpec方法处理；
 * 参数：
 *      @stage[in]： 当前行所处的阶段
 *      @line[in]: 当前行的内容
 * 返回值：
 *      0表示成功，-1表示失败
 ***************************************************************************************************/
static int handleLine(ACoACInstance *pInst, int stage, char *line) {
    switch (stage) {
    case 1:
        handleUsers(pInst, line);
        break;
    case 2:
        if (handleAttributes(line)) {
            return -1;
        }
        break;
    case 3:
        handleDefaultValues(line);
        break;
    case 4:
        handleUAV(pInst, line);
        break;
    case 5:
        handleRule(pInst, line);
        break;
    case 6:
        handleSpec(pInst, line);
        break;
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "Unexpected line: %s\n", line);
        return -1;
    }
    return 0;
}

ACoACInstance *readACoACInstance(char *acoacFilePath) {
    logACoAC(__func__, __LINE__, 0, INFO, "[start] reading ACoAC instance from file %s\n", acoacFilePath);

    FILE *file = fopen(acoacFilePath, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", acoacFilePath);
        return NULL;
    }

    // 初始化全局变量
    initGlobalVars();
    ACoACInstance *pInst = createACoACInstance();

    // 0: initial, 1: users, 2: attributes, 3: default value, 4: UAV, 5: rules, 6: spec
    int stage = 0;
    int line_count = 0;

    // 动态分配初始缓冲区
    size_t buffer_size = 1024;
    char *line = (char *)malloc(buffer_size);
    if (line == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to allocate memory for line buffer\n");
        fclose(file);
        return NULL;
    }

    while (1) {
        // 尝试读取一行
        if (fgets(line, buffer_size, file) == NULL) {
            // 文件结束或读取错误
            break;
        }

        line_count++;

        // 检查是否需要扩展缓冲区
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] != '\n' && !feof(file)) {
            // 行被截断，需要扩展缓冲区
            size_t new_size = buffer_size * 2;
            char *new_line = (char *)realloc(line, new_size);
            if (new_line == NULL) {
                logACoAC(__func__, __LINE__, 0, ERROR, "Failed to reallocate memory for line buffer\n");
                free(line);
                fclose(file);
                return NULL;
            }
            line = new_line;
            buffer_size = new_size;

            // 继续读取同一行的剩余部分
            while (fgets(line + line_len, buffer_size - line_len, file) != NULL) {
                line_len = strlen(line);
                if (line_len > 0 && line[line_len - 1] == '\n') {
                    break; // 行已完整读取
                }
                if (feof(file)) {
                    break; // 文件结束
                }
                // 再次扩展缓冲区
                new_size = buffer_size * 2;
                new_line = (char *)realloc(line, new_size);
                if (new_line == NULL) {
                    logACoAC(__func__, __LINE__, 0, ERROR, "Failed to reallocate memory for line buffer\n");
                    free(line);
                    fclose(file);
                    return NULL;
                }
                line = new_line;
                buffer_size = new_size;
            }
        }

        // 处理完整的行
        char *trimmed_line = strtrim(line);
        // Check if the line is empty after trimming
        if (strlen(trimmed_line) == 0) {
            continue;
        }

        // Process the line
        if (strcmp(trimmed_line, USERS) == 0) {
            stage = 1;
        } else if (strcmp(trimmed_line, ATTRIBUTES) == 0) {
            stage = 2;
        } else if (strcmp(trimmed_line, DEFAULT_VALUE) == 0) {
            stage = 3;
        } else if (strcmp(trimmed_line, UAV) == 0) {
            stage = 4;
        } else if (strcmp(trimmed_line, RULES) == 0) {
            stage = 5;
        } else if (strcmp(trimmed_line, SPEC) == 0) {
            stage = 6;
        } else {
            if (handleLine(pInst, stage, trimmed_line)) {
                // Error in processing the line
                free(line);
                fclose(file);
                return NULL;
            }
        }
    }

    // 释放动态分配的内存
    free(line);
    fclose(file);

    logACoAC(__func__, __LINE__, 0, INFO, "[end] reading ACoAC instance from file %s\n", acoacFilePath);
    return pInst;
}