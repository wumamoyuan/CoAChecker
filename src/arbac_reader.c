#include "acoac_io.h"
#include "acoac_utils.h"
#include <regex.h>

#define ROLES "Roles"
#define ROLES_LEN 5

#define USERS "Users"
#define USERS_LEN 5

#define UA "UA"
#define UA_LEN 2

#define CR "CR"
#define CR_LEN 2

#define CA "CA"
#define CA_LEN 2

#define ADMIN "ADMIN"
#define ADMIN_LEN 5

#define SPEC "SPEC"
#define SPEC_LEN 4

/**
 * 解析角色列表，将角色添加到ACoACInstance中
 * @param line 格式为：role1 role2 role3 ... roleN;
 */
static void handleRoles(ACoACInstance *pInst, char *line) {
    if (strcmp(line, ";") == 0) {
        return;
    }

    AttrType attrType = BOOLEAN;
    int defValIdx = 0;
    int roleNum = 0;

    char *role;
    int roleLen = 0;

    char *token = strtok(line, " "), *nextToken = NULL;
    while (token != NULL) {
        role = strtrim(token);
        roleLen = strlen(role);
        nextToken = strtok(NULL, " ");
        if (nextToken == NULL) {
            if (roleLen > 0 && role[roleLen - 1] == ';') {
                role[roleLen - 1] = '\0';
                roleLen--;
            }
        }

        if (roleLen > 0 && !iDictionary.Contains(pdictAttr2Index, role)) {
            roleNum = istrCollection.Size(pscAttrs);
            iDictionary.Insert(pdictAttr2Index, role, &roleNum);
            istrCollection.Add(pscAttrs, role);
            iHashMap.Put(pmapAttr2Type, &roleNum, &attrType);
            iHashMap.Put(pmapAttr2DefVal, &roleNum, &defValIdx);
            logACoAC(__func__, __LINE__, 0, DEBUG, "Add attribute: %s, index: %d\n", role, roleNum);
            roleNum++;
        }
        token = nextToken;
    }
}

/**
 * 解析用户列表，将用户添加到ACoACInstance中
 * @param line 格式为：user1 user2 user3 ... userN;
 */
static void handleUsers(ACoACInstance *pInst, char *line) {
    char *user;
    int userLen = 0;

    char *token = strtok(line, " ");
    char *nextToken = NULL;
    int userNum = 0;

    while (token != NULL) {
        user = strtrim(token);
        userLen = strlen(user);
        nextToken = strtok(NULL, " ");
        if (nextToken == NULL) {
            if (userLen > 0 && user[userLen - 1] == ';') {
                // Remove trailing semicolon
                user[userLen - 1] = '\0';
                userLen--;
            }
        }

        if (userLen > 0 && iDictionary.Insert(pdictUser2Index, user, &userNum)) {
            istrCollection.Add(pscUsers, user);
            iVector.Add(pInst->pVecUserIndices, &userNum);
            userNum++;
            logACoAC(__func__, __LINE__, 0, DEBUG, "Add %d user: %s\n", userNum, user);
        }
        token = nextToken;
    }
}

/*能够匹配出<>中的内容，例如<1,2,3>，<1,2,3,4>，<1,2,3,4,5>，会匹配出1,2,3，1,2,3,4，1,2,3,4,5*/
#define PATTERN_STR "<(.*?)>"
regex_t pattern;
int compiled = 0;

/**
 * 解析用户初始属性值列表，将用户初始属性值添加到ACoACInstance中
 * 如果line为分号，不做任何处理。
 * 如果line不为分号，判断line是否是形如<user, role>的字符串，可能最后会带一个分号。
 * 如果是，将user, attr, value分别加入到instance的userAttrValueSet中。
 * 如果不是，抛出异常。
 * @param line 格式为：<user, role> <user, role> ... <user, role>;
 */
static void handleUA(ACoACInstance *pInst, char *line) {
    if (!compiled) {
        regcomp(&pattern, PATTERN_STR, REG_EXTENDED);
        compiled = 1;
    }

    regmatch_t pmatch[2];
    int rc, addUAVRet;
    char *uaStr, *user, *role;
    while (line != NULL) {
        rc = regexec(&pattern, line, 2, pmatch, 0);
        if (rc != 0) {
            break;
        }
        uaStr = line + pmatch[1].rm_so;
        if (line[pmatch[1].rm_eo] == '\0') {
            line = NULL;
        } else {
            line[pmatch[1].rm_eo] = '\0';
            line = line + pmatch[1].rm_eo + 1;
        }
        user = strtok(uaStr, ",");
        role = strtok(NULL, ",");

        if (user == NULL || role == NULL || strtok(NULL, ",") != NULL) {
            logACoAC(__func__, __LINE__, 0, ERROR, "UA should be form of <user, role>\n");
            exit(1);
        }

        addUAVRet = addUAV(pInst, strtrim(user), strtrim(role), "true");
        if (addUAVRet) {
            logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle UA: <%s, %s>\n", user, role);
            exit(addUAVRet);
        }
    }
}

/**
 * 判断condStr是否是一个合法的条件，如果是，返回Condition对象，否则抛出异常。
 * 合法的条件格式为：TRUE或role1 & role2 & ... & roleN，其中每个角色前可以有一个-号，表示取反
 * 如果condStr为TRUE，返回一个Condition对象，其中的atomConds为空集合
 * 如果condStr不为TRUE，将condStr按照&符号分割，得到role1, role2, ..., roleN
 * 对于每个role，判断其是否以-开头，然后添加到Condition对象的atomConds中
 * @param condStr
 * @return
 */
static HashSet *handleCondition(char *condStr) {
    HashSet *pSetCondition = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
    if (strcmp("TRUE", condStr) == 0) {
        return pSetCondition;
    }

    int roleIdx;
    char *role, *token = strtok(condStr, "&");
    AtomCondition atomCond;
    while (token != NULL) {
        role = strtrim(token);
        if (strlen(role) > 0 && role[0] == '-') {
            role = role + 1;
            roleIdx = getAttrIndex(role);
            atomCond = (AtomCondition){
                .attribute = roleIdx,
                .value = 0,
                .op = EQUAL};
        } else {
            roleIdx = getAttrIndex(role);
            atomCond = (AtomCondition){
                .attribute = roleIdx,
                .value = 1,
                .op = EQUAL};
        }
        iHashSet.Add(pSetCondition, &atomCond);
        token = strtok(NULL, "&");
    }

    return pSetCondition;
}

/**
 * 解析CA规则，将CA规则添加到ACoACInstance中
 * 规则的格式为：<adminCond, usercond, targetRole> <adminCond, userCond, targetRole> ... <adminCond, userCond, targetRole>;
 * @param line
 */
static void handleCA(ACoACInstance *pInst, char *line) {
    if (!compiled) {
        regcomp(&pattern, PATTERN_STR, REG_EXTENDED);
        compiled = 1;
    }

    regmatch_t pmatch[2];
    int rc;
    char *caStr, *adminCondStr, *userCondStr, *targetRole;
    int roleIdx;
    HashSet *adminCond, *userCond;
    Rule *r;
    while (line != NULL) {
        rc = regexec(&pattern, line, 2, pmatch, 0);
        if (rc != 0) {
            break;
        }
        caStr = line + pmatch[1].rm_so;
        if (line[pmatch[1].rm_eo] == '\0') {
            line = NULL;
        } else {
            line[pmatch[1].rm_eo] = '\0';
            line = line + pmatch[1].rm_eo + 1;
        }

        adminCondStr = strtok(caStr, ",");
        userCondStr = strtok(NULL, ",");
        targetRole = strtok(NULL, ",");

        if (adminCondStr == NULL || userCondStr == NULL || targetRole == NULL || strtok(NULL, ",") != NULL) {
            logACoAC(__func__, __LINE__, 0, ERROR, "CA should be form of <adminCond, userCond, targetRole>\n");
            exit(1);
        }

        roleIdx = getAttrIndex(strtrim(targetRole));

        adminCond = handleCondition(strtrim(adminCondStr));
        userCond = handleCondition(strtrim(userCondStr));
        r = iRule.Create(adminCond, userCond, roleIdx, 1);
        iVector.Add(pVecRules, r);
        free(r);
        int ruleIdx = iVector.Size(pVecRules) - 1;
        addRule(pInst, ruleIdx);
    }
}

/**
 * 解析CR规则，将CR规则添加到ACoACInstance中
 * 规则的格式为：<adminCond, targetRole> <adminCond, targetRole> ... <adminCond, targetRole>;
 * @param line
 */
static void handleCR(ACoACInstance *pInst, char *line) {
    if (!compiled) {
        regcomp(&pattern, PATTERN_STR, REG_EXTENDED);
        compiled = 1;
    }

    regmatch_t pmatch[2];
    int rc;
    char *crStr, *adminCondStr, *targetRole;
    int roleIdx;
    HashSet *adminCond, *userCond;
    Rule *r;
    while (line != NULL) {
        rc = regexec(&pattern, line, 2, pmatch, 0);
        if (rc != 0) {
            break;
        }
        crStr = line + pmatch[1].rm_so;
        if (line[pmatch[1].rm_eo] == '\0') {
            line = NULL;
        } else {
            line[pmatch[1].rm_eo] = '\0';
            line = line + pmatch[1].rm_eo + 1;
        }

        adminCondStr = strtok(crStr, ",");
        targetRole = strtok(NULL, ",");

        if (adminCondStr == NULL || targetRole == NULL || strtok(NULL, ",") != NULL) {
            logACoAC(__func__, __LINE__, 0, ERROR, "CR should be form of <adminCond, targetRole>\n");
            exit(1);
        }

        roleIdx = getAttrIndex(strtrim(targetRole));

        adminCond = handleCondition(strtrim(adminCondStr));
        userCond = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
        r = iRule.Create(adminCond, userCond, roleIdx, 0);
        iVector.Add(pVecRules, r);
        free(r);
        int ruleIdx = iVector.Size(pVecRules) - 1;
        addRule(pInst, ruleIdx);
    }
}

/**
 * 解析查询语句，将查询语句添加到ACoACInstance中
 * 查询语句的格式为：user role;
 * @param line
 */
static void handleSpec(ACoACInstance *pInst, char *line) {
    if (strcmp(";", line) == 0) {
        return;
    }

    char *queryUser = strtok(line, " ");
    char *role = strtok(NULL, " ");
    if (queryUser == NULL || role == NULL || strtok(NULL, " ") != NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "spec should be form of user, role\n");
        exit(1);
    }

    pInst->queryUserIdx = getUserIndex(strtrim(queryUser));
    role = strtrim(role);

    if (role[strlen(role) - 1] == ';') {
        role[strlen(role) - 1] = '\0';
    }
    int roleIdx = getAttrIndex(role);
    int valueIdx = 1;

    iHashMap.Put(pInst->pmapQueryAVs, &roleIdx, &valueIdx);
    logACoAC(__func__, __LINE__, 0, INFO, "query user: %s\n", queryUser);
}

/**
 * 处理一行数据。根据stage的值，调用不同的处理方法。
 * stage=1时，表示当前行为角色列表，调用handleRoles方法处理；
 * stage=2时，表示当前行为用户列表，调用handleUsers方法处理；
 * stage=3时，表示当前行为用户-角色关系，调用handleUA方法处理；
 * stage=4时，表示当前行为角色-角色关系，调用handleCR方法处理；
 * stage=5时，表示当前行为角色-属性关系，调用handleCA方法处理；
 * stage=6时，表示当前行为管理员列表，不用做任何处理；
 * stage=7时，表示当前行为查询列表，调用handleSpec方法处理；
 * @param stage 当前行所处的阶段
 * @param line 当前行的内容
 */
static int handleLine(ACoACInstance *pInst, int stage, char *line) {
    switch (stage) {
    case 1:
        handleRoles(pInst, line);
        break;
    case 2:
        handleUsers(pInst, line);
        break;
    case 3:
        handleUA(pInst, line);
        break;
    case 4:
        handleCR(pInst, line);
        break;
    case 5:
        handleCA(pInst, line);
        break;
    case 6:
        break;
    case 7:
        handleSpec(pInst, line);
        break;
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "unexpect line: %s\n", line);
        return -1;
    }
    return 0;
}

ACoACInstance *readARBACInstance(char *arbacFilePath) {
    logACoAC(__func__, __LINE__, 0, INFO, "[start] reading ARBAC instance from file %s\n", arbacFilePath);

    FILE *file = fopen(arbacFilePath, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", arbacFilePath);
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
        if (strlen(trimmed_line) == 0) {
            continue;
        }

        if (strncmp(trimmed_line, ROLES, ROLES_LEN) == 0) {
            stage = 1;
            trimmed_line += ROLES_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, USERS, USERS_LEN) == 0) {
            stage = 2;
            trimmed_line += USERS_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, UA, UA_LEN) == 0) {
            stage = 3;
            trimmed_line += UA_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, CR, CR_LEN) == 0) {
            stage = 4;
            trimmed_line += CR_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, CA, CA_LEN) == 0) {
            stage = 5;
            trimmed_line += CA_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, ADMIN, ADMIN_LEN) == 0) {
            stage = 6;
            trimmed_line += ADMIN_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else if (strncmp(trimmed_line, SPEC, SPEC_LEN) == 0) {
            stage = 7;
            trimmed_line += SPEC_LEN;
            trimmed_line = strtrim(trimmed_line);
            if (strlen(trimmed_line) > 0) {
                handleLine(pInst, stage, trimmed_line);
            }
        } else {
            handleLine(pInst, stage, trimmed_line);
        }
    }

    // 释放动态分配的内存
    free(line);
    fclose(file);

    logACoAC(__func__, __LINE__, 0, INFO, "[end] reading ARBAC instance from file %s\n", arbacFilePath);
    return pInst;
}
