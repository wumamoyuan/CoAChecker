#include "acoac_absref.h"
#include "acoac_io.h"
#include "acoac_pruning.h"
#include "acoac_utils.h"

#include <getopt.h>
#include <regex.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define INST_FILE_SUFFIX ".aabac"

/**
 * 计算Global Pruning Rate和Local Pruning Rate
 * 如果resultFile不为NULL，则将结果保存到resultFile中
 *
 * @param instFile 实例文件路径
 * @param resultFile 结果文件路径
 */
static void computePR(char *instFile, char *resultFile) {
    ACoACInstance *pInst = readACoACInstance(instFile);
    if (pInst == NULL) {
        printf("Failed to read instance file!\n");
        return;
    }

    FILE *fp = NULL;
    if (resultFile != NULL) {
        if (access(resultFile, F_OK) == -1) {
            // 如果resultFile不存在，则创建它，并写入表头
            fp = fopen(resultFile, "w");
            fprintf(fp, "instFile,GPR_oldRuleNums,GPR_newRuleNums,GPR,LPR_oldRuleNums,LPR_newRuleNums,LPR\n");
        } else {
            fp = fopen(resultFile, "a");
        }
        if (fp == NULL) {
            printf("Failed to open result file!\n");
            return;
        }
    }

    init(pInst);

    // 记录global pruning前的规则数
    int nRulesBeforeGP = iHashSet.Size(pInst->pSetRuleIdxes);

    // 进行global pruning
    pInst = userCleaning(pInst);
    ACoACResult result = {.code = ACoAC_RESULT_UNKNOWN};
    pInst = slice(pInst, &result);

    if (result.code == ACoAC_RESULT_REACHABLE || result.code == ACoAC_RESULT_UNREACHABLE) {
        // 如果global pruning后即可判断实例的可达性，说明所有规则均为冗余，剪枝后规则数为0，并且不需要进行local pruning
        int nRulesAfterGP = 0;
        double GPR = (nRulesBeforeGP - nRulesAfterGP) * 100.0 / nRulesBeforeGP;
        logACoAC(__func__, __LINE__, 0, INFO, "oldRuleNums => %d, newRuleNums => %d, GPR => %.2f%%\n", nRulesBeforeGP, nRulesAfterGP, GPR);
        if (fp != NULL) {
            fprintf(fp, "%s,%d,%d,%.2f%%,-,-,-\n", instFile, nRulesBeforeGP, nRulesAfterGP, GPR);
            fclose(fp);
        }
        return;
    }

    // 记录剪枝后的规则数
    int nRulesAfterGP = iHashSet.Size(pInst->pSetRuleIdxes);
    double gpr = (nRulesBeforeGP - nRulesAfterGP) * 100.0 / nRulesBeforeGP;

    // 进行local pruning
    int cnt = 0;
    int capacity = 10;
    int *nRulesBeforeLP = (int *)malloc(capacity * sizeof(int));
    int *nRulesAfterLP = (int *)malloc(capacity * sizeof(int));

    AbsRef *pAbsRef = createAbsRef(pInst);
    ACoACInstance *next = abstract(pAbsRef);
    while (next != NULL) {
        if (cnt >= capacity) {
            capacity *= 2;
            nRulesBeforeLP = (int *)realloc(nRulesBeforeLP, capacity * sizeof(int));
            nRulesAfterLP = (int *)realloc(nRulesAfterLP, capacity * sizeof(int));
        }
        nRulesBeforeLP[cnt] = iHashSet.Size(next->pSetRuleIdxes);
        result = (ACoACResult){.code = ACoAC_RESULT_UNKNOWN};
        next = slice(next, &result);

        if (result.code == ACoAC_RESULT_REACHABLE) {
            nRulesAfterLP[cnt] = 0;
            cnt++;
            break;
        } else if (result.code == ACoAC_RESULT_UNREACHABLE) {
            nRulesAfterLP[cnt] = 0;
        } else {
            nRulesAfterLP[cnt] = iHashSet.Size(next->pSetRuleIdxes);
        }

        if (result.code != ACoAC_RESULT_REACHABLE && result.code != ACoAC_RESULT_UNREACHABLE) {
            nRulesAfterLP[cnt] = iHashSet.Size(next->pSetRuleIdxes);
        } else {
            nRulesAfterLP[cnt] = 0;
        }
        cnt++;
        next = refine(pAbsRef);
    }

    int i;
    double averageLPR = 0;
    for (i = 0; i < cnt; i++) {
        averageLPR += (nRulesBeforeLP[i] - nRulesAfterLP[i]) * 100.0 / nRulesBeforeLP[i];
    }
    averageLPR /= cnt;
    logACoAC(__func__, __LINE__, 0, INFO, "oldRuleNums => %d, newRuleNums => %d, GPR => %.2f%%, averageLPR => %.2f%%\n", nRulesBeforeGP, nRulesAfterGP, gpr, averageLPR);

    if (fp != NULL) {
        fprintf(fp, "%s,%d,%d,%.2f%%,", instFile, nRulesBeforeGP, nRulesAfterGP, gpr);
        for (i = 0; i < cnt; i++) {
            fprintf(fp, i == 0 ? "%d" : "-%d", nRulesBeforeLP[i]);
        }
        fprintf(fp, ",");
        for (i = 0; i < cnt; i++) {
            fprintf(fp, i == 0 ? "%d" : "-%d", nRulesAfterLP[i]);
        }
        fprintf(fp, ",%.2f%%\n", averageLPR);
        fclose(fp);
    }
}

static double computeGPRFromString(char *oldRuleNumStr, char *newRuleNumStr) {
    int oldRuleNum = atoi(oldRuleNumStr);
    int newRuleNum = atoi(newRuleNumStr);
    return (oldRuleNum - newRuleNum) * 100.0 / oldRuleNum;
}

static double computeLPRFromString(char *oldRuleNumStr, char *newRuleNumStr) {
    if (strcmp(oldRuleNumStr, "-") == 0) {
        return -1;
    }

    char *pOld = oldRuleNumStr;
    int cnt = 1;
    while (*pOld != '\0') {
        if (*pOld == '-') {
            cnt++;
        }
        pOld++;
    }
    pOld = oldRuleNumStr;
    int oldRuleNums[cnt];
    int newRuleNums[cnt];

    cnt = 0;
    oldRuleNumStr = strtok(oldRuleNumStr, "-");
    while (oldRuleNumStr != NULL) {
        oldRuleNums[cnt++] = atoi(oldRuleNumStr);
        oldRuleNumStr = strtok(NULL, "-");
    }

    cnt = 0;
    newRuleNumStr = strtok(newRuleNumStr, "-");
    while (newRuleNumStr != NULL) {
        newRuleNums[cnt++] = atoi(newRuleNumStr);
        newRuleNumStr = strtok(NULL, "-");
    }

    double sumPruningRate = 0;
    for (int i = 0; i < cnt; i++) {
        sumPruningRate += (oldRuleNums[i] - newRuleNums[i]) * 100.0 / oldRuleNums[i];
    }
    return sumPruningRate / cnt;
}

static void computeStatsAttrRule(char *resultFilePath) {
    FILE *fp = fopen(resultFilePath, "r");
    if (fp == NULL) {
        printf("Statistics file not found!\n");
        return;
    }

    int na = 4, nr = 10, ninst = 50;
    double gpr[na][nr][ninst];
    double lpr[na][nr][ninst];

    // 用正则表达式匹配实例文件名
    char *pattern = "MC5-Q1-V20-A([0-9]+)/P([0-9]+)/test([0-9]+)" INST_FILE_SUFFIX;
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    regmatch_t pmatch[4];

    int a, r, id;
    char aStr[10], rStr[10], idStr[10];
    char *instFile, *nRulesBeforeGPStr, *nRulesAfterGPStr, *nRulesBeforeLPStr, *nRulesAfterLPStr;

    char line[1024];
    // 跳过表头
    fgets(line, sizeof(line), fp);

    int lineNum = 1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        lineNum++;
        instFile = strtok(line, ",");

        // 解析instFile
        if (regexec(&regex, instFile, 4, pmatch, 0) == 0) {
            memcpy(aStr, instFile + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
            aStr[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
            memcpy(rStr, instFile + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
            rStr[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
            memcpy(idStr, instFile + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
            idStr[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
            a = atoi(aStr);
            r = atoi(rStr);
            id = atoi(idStr);

            if (a > 30 && a <= 150 && r > 0 && r <= nr * 500 && id > 0 && id <= ninst) {
                nRulesBeforeGPStr = strtok(NULL, ",");
                nRulesAfterGPStr = strtok(NULL, ",");
                gpr[a / 30 - 2][r / 500 - 1][id - 1] = computeGPRFromString(nRulesBeforeGPStr, nRulesAfterGPStr);

                strtok(NULL, ",");
                nRulesBeforeLPStr = strtok(NULL, ",");
                nRulesAfterLPStr = strtok(NULL, ",");
                lpr[a / 30 - 2][r / 500 - 1][id - 1] = computeLPRFromString(nRulesBeforeLPStr, nRulesAfterLPStr);
                continue;
            }
        }
        printf("Error parsing line: %d\n in result file: %s\n", lineNum, resultFilePath);
        exit(1);
    }

    int oldRuleNum, newRuleNum;
    double sumGPR, sumLPR;
    double gprList[nr], lprList[nr];
    int cntLPR;
    for (a = 2; a <= 1 + na; a++) {
        for (r = 1; r <= nr; r++) {
            // 计算平均剪枝率
            sumGPR = 0;
            sumLPR = 0;
            cntLPR = 0;
            for (int i = 1; i <= ninst; i++) {
                sumGPR += gpr[a - 2][r - 1][i - 1];
                if (lpr[a - 2][r - 1][i - 1] != -1) {
                    cntLPR++;
                    sumLPR += lpr[a - 2][r - 1][i - 1];
                }
            }
            gprList[r - 1] = sumGPR / ninst;
            lprList[r - 1] = sumLPR / cntLPR;
        }

        // 打印GPR和LPR
        printf("[GPR] MC5-Q1-V20-A%d", a * 30);
        for (int i = 0; i < nr; i++) {
            printf(",%.2f%%", gprList[i]);
        }
        printf("\n");
        printf("[LPR] MC5-Q1-V20-A%d", a * 30);
        for (int i = 0; i < nr; i++) {
            printf(",%.2f%%", lprList[i]);
        }
        printf("\n");
    }
}

static void computeStatsMaxCondVal(char *resultFilePath) {
    FILE *fp = fopen(resultFilePath, "r");
    if (fp == NULL) {
        printf("Statistics file not found!\n");
        return;
    }

    int nmc = 4, nv = 10, ninst = 50;
    double gpr[nmc][nv][ninst];
    double lpr[nmc][nv][ninst];

    // 用正则表达式匹配实例文件名
    char *pattern = "MC([0-9]+)-Q1-A90-R3000/V([0-9]+)/test([0-9]+)"INST_FILE_SUFFIX;
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    regmatch_t pmatch[4];

    int mc, v, id;
    char mcStr[10], vStr[10], idStr[10];
    char *instFile, *nRulesBeforeGPStr, *nRulesAfterGPStr, *nRulesBeforeLPStr, *nRulesAfterLPStr;

    char line[1024];
    // 跳过表头
    fgets(line, sizeof(line), fp);

    int lineNum = 1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        lineNum++;
        instFile = strtok(line, ",");

        // 解析instFile
        if (regexec(&regex, instFile, 4, pmatch, 0) == 0) {
            memcpy(mcStr, instFile + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
            mcStr[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
            memcpy(vStr, instFile + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
            vStr[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
            memcpy(idStr, instFile + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
            idStr[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
            mc = atoi(mcStr);
            v = atoi(vStr);
            id = atoi(idStr);

            if (mc > 2 && mc <= 2 + nmc && v > 0 && v <= nv * 10 && id > 0 && id <= ninst) {
                nRulesBeforeGPStr = strtok(NULL, ",");
                nRulesAfterGPStr = strtok(NULL, ",");
                gpr[mc - 3][v / 10 - 1][id - 1] = computeGPRFromString(nRulesBeforeGPStr, nRulesAfterGPStr);

                strtok(NULL, ",");
                nRulesBeforeLPStr = strtok(NULL, ",");
                nRulesAfterLPStr = strtok(NULL, ",");
                lpr[mc - 3][v / 10 - 1][id - 1] = computeLPRFromString(nRulesBeforeLPStr, nRulesAfterLPStr);
                continue;
            }
        }
        printf("Error parsing line: %d\n in result file: %s\n", lineNum, resultFilePath);
        exit(1);
    }

    int oldRuleNum, newRuleNum;
    double sumGPR, sumLPR;
    double gprList[nv], lprList[nv];
    int cntLPR;
    for (mc = 3; mc <= 2 + nmc; mc++) {
        for (v = 1; v <= nv; v++) {
            // 计算平均剪枝率
            sumGPR = 0;
            sumLPR = 0;
            cntLPR = 0;
            for (int i = 1; i <= ninst; i++) {
                sumGPR += gpr[mc - 3][v - 1][i - 1];
                if (lpr[mc - 3][v - 1][i - 1] != -1) {
                    cntLPR++;
                    sumLPR += lpr[mc - 3][v - 1][i - 1];
                }
            }
            gprList[v - 1] = sumGPR / ninst;
            lprList[v - 1] = sumLPR / cntLPR;
        }

        // 打印GPR和LPR
        printf("[GPR] MC%d-Q1-A90-R3000", mc);
        for (int i = 0; i < nv; i++) {
            printf(",%.2f%%", gprList[i]);
        }
        printf("\n");
        printf("[LPR] MC%d-Q1-A90-R3000", mc);
        for (int i = 0; i < nv; i++) {
            printf(",%.2f%%", lprList[i]);
        }
        printf("\n");
    }
}

static void computeStatsAttrQueryVal(char *resultFilePath) {
    FILE *fp = fopen(resultFilePath, "r");
    if (fp == NULL) {
        printf("Statistics file not found!\n");
        return;
    }

    int nq = 5, nv = 10, ninst = 50;
    double gpr[nq][nv][ninst];
    double lpr[nq][nv][ninst];

    // 用正则表达式匹配实例文件名
    char *pattern = "MC5-Q([0-9]+)-A60-R3000/V([0-9]+)/test([0-9]+)"INST_FILE_SUFFIX;
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    regmatch_t pmatch[4];

    int q, v, id;
    char qStr[10], vStr[10], idStr[10];
    char *instFile, *nRulesBeforeGPStr, *nRulesAfterGPStr, *nRulesBeforeLPStr, *nRulesAfterLPStr;

    char line[1024];
    // 跳过表头
    fgets(line, sizeof(line), fp);

    int lineNum = 1;
    while (fgets(line, sizeof(line), fp) != NULL) {
        lineNum++;
        instFile = strtok(line, ",");

        // 解析instFile
        if (regexec(&regex, instFile, 4, pmatch, 0) == 0) {
            memcpy(qStr, instFile + pmatch[1].rm_so, pmatch[1].rm_eo - pmatch[1].rm_so);
            qStr[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
            memcpy(vStr, instFile + pmatch[2].rm_so, pmatch[2].rm_eo - pmatch[2].rm_so);
            vStr[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';
            memcpy(idStr, instFile + pmatch[3].rm_so, pmatch[3].rm_eo - pmatch[3].rm_so);
            idStr[pmatch[3].rm_eo - pmatch[3].rm_so] = '\0';
            q = atoi(qStr);
            v = atoi(vStr);
            id = atoi(idStr);

            if (q > 0 && q <= nq && v > 0 && v <= nv * 10 && id > 0 && id <= ninst) {
                nRulesBeforeGPStr = strtok(NULL, ",");
                nRulesAfterGPStr = strtok(NULL, ",");
                gpr[q - 1][v / 10 - 1][id - 1] = computeGPRFromString(nRulesBeforeGPStr, nRulesAfterGPStr);

                strtok(NULL, ",");
                nRulesBeforeLPStr = strtok(NULL, ",");
                nRulesAfterLPStr = strtok(NULL, ",");
                lpr[q - 1][v / 10 - 1][id - 1] = computeLPRFromString(nRulesBeforeLPStr, nRulesAfterLPStr);
                continue;
            }
        }
        printf("Error parsing line: %d\n in result file: %s\n", lineNum, resultFilePath);
        exit(1);
    }

    int oldRuleNum, newRuleNum;
    double sumGPR, sumLPR;
    double gprList[nv], lprList[nv];
    int cntLPR;
    for (q = 1; q <= nq; q++) {
        for (v = 1; v <= nv; v++) {
            // 计算平均剪枝率
            sumGPR = 0;
            sumLPR = 0;
            cntLPR = 0;
            for (int i = 1; i <= ninst; i++) {
                sumGPR += gpr[q - 1][v - 1][i - 1];
                if (lpr[q - 1][v - 1][i - 1] != -1) {
                    cntLPR++;
                    sumLPR += lpr[q - 1][v - 1][i - 1];
                }
            }
            gprList[v - 1] = sumGPR / ninst;
            lprList[v - 1] = sumLPR / cntLPR;
        }

        // 打印GPR和LPR
        printf("[GPR] MC5-Q%d-A60-R3000", q);
        for (int i = 0; i < nv; i++) {
            printf(",%.2f%%", gprList[i]);
        }
        printf("\n");
        printf("[LPR] MC5-Q%d-A60-R3000", q);
        for (int i = 0; i < nv; i++) {
            printf(",%.2f%%", lprList[i]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    char *helpMessage = "Usage: %s <option> <argument>\n"
                        "Options:\n"
                        "  -s, --stat <a-r|mc-v|aq-v>   Compute statistics of given result file\n"
                        "  -i, --instance-file <arg>    The file of ACoAC-safety instance\n"
                        "  -r, --result-file <arg>      The file to save the experiment result\n";

    static struct option long_options[] = {
        {"stat", required_argument, 0, 's'},
        {"instance-file", required_argument, 0, 'i'},
        {"result-file", required_argument, 0, 'r'},
        {0, 0, 0, 0}};

    // char *mode = NULL;
    char *statType = NULL;
    char *instFilePath = NULL;
    char *resultFilePath = NULL;
    int unrecognized = 0;

    int c;
    while (1) {
        int option_index = 0;

        c = getopt_long_only(argc, argv, "s:i:r:", long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 's':
            statType = (char *)malloc(strlen(optarg) + 1);
            strcpy(statType, optarg);
            break;
        case 'i':
            instFilePath = (char *)malloc(strlen(optarg) + 1);
            strcpy(instFilePath, optarg);
            break;
        case 'r':
            resultFilePath = (char *)malloc(strlen(optarg) + 1);
            strcpy(resultFilePath, optarg);
            break;
        default:
            unrecognized = 1;
            break;
        }
    }

    if (unrecognized) {
        printf("Unrecognized option or mode is not set!\n");
        printf("%s\n", helpMessage);
        return 1;
    }

    if (statType == NULL) {
        if (instFilePath == NULL) {
            printf("Instance file is not specified!\n");
            printf("%s\n", helpMessage);
            return 1;
        }
        computePR(instFilePath, resultFilePath);
        return 0;
    }

    if (strcmp(statType, "a-r") == 0) {
        if (resultFilePath == NULL) {
            printf("Result file is not specified!\n");
            printf("%s\n", helpMessage);
            return 1;
        }
        computeStatsAttrRule(resultFilePath);
        return 0;
    }
    if (strcmp(statType, "mc-v") == 0) {
        if (resultFilePath == NULL) {
            printf("Result file is not specified!\n");
            printf("%s\n", helpMessage);
            return 1;
        }
        computeStatsMaxCondVal(resultFilePath);
        return 0;
    }
    if (strcmp(statType, "aq-v") == 0) {
        if (resultFilePath == NULL) {
            printf("Result file is not specified!\n");
            printf("%s\n", helpMessage);
            return 1;
        }
        computeStatsAttrQueryVal(resultFilePath);
        return 0;
    }

    printf("unrecogized statistics type: %s\n", statType);
    printf("%s\n", helpMessage);
    return 1;
}