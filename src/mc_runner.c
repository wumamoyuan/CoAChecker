// #define _POSIX_C_SOURCE 199309L

#include "mc_runner.h"
#include "acoac_utils.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT_MESSAGE "timeout"
#define TIMEOUT_MESSAGE_LEN 7
#define MEMORY_OUT_MESSAGE "memory out"
#define MEMORY_OUT_MESSAGE_LEN 10

/**
 * 执行Linux命令，等待命令执行结束，并返回命令执行结果。如果等待至超时时间命令仍未结束，则强制杀死子进程，并返回超时错误信息。
 * @param cmdPath[in]: 命令路径
 * @param args[in]: 命令参数
 * @param timeout[in]: 超时时间
 * @return 命令执行结果
 */
static char *run(char *cmdPath, char *args[], char *resultFilePath, long timeout) {
    int i = 1;
    char *cmd = (char *)malloc(strlen(args[0]) + 1);
    strcpy(cmd, args[0]);
    while (args[i] != NULL) {
        cmd = (char *)realloc(cmd, strlen(cmd) + strlen(args[i]) + 3);
        strcat(cmd, ", ");
        strcat(cmd, args[i]);
        i++;
    }
    logACoAC(__func__, __LINE__, 0, INFO, "cmd: [%s]\n", cmd);
    free(cmd);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to create pipe\n");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {                     // Child process
        close(pipefd[0]);               // Close read end
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
        dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
        close(pipefd[1]);

        execv(cmdPath, args);
        // If execv returns, it means there was an error
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to execute command\n");
        exit(1);
    }

    // Parent process
    close(pipefd[1]); // Close write end

    // Read output with timeout
    char *output = NULL;
    size_t output_size = 0;
    char buffer[4096];
    ssize_t bytes_read;
    time_t start_time = time(NULL);

    while ((time(NULL) - start_time) < timeout) {
        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(pipefd[0], &read_fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(pipefd[0] + 1, &read_fds, NULL, NULL, &tv);
        if (ret == -1) {
            logACoAC(__func__, __LINE__, 0, ERROR, "Select failed\n");
            break;
        } else if (ret == 0) {
            // Timeout in select, check if process is still running
            if (waitpid(pid, NULL, WNOHANG) != 0) {
                break; // Process has finished
            }
            continue;
        }

        bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break;
        }

        buffer[bytes_read] = '\0';
        char *new_output = realloc(output, output_size + bytes_read + 1);
        if (new_output == NULL) {
            logACoAC(__func__, __LINE__, 0, ERROR, "Failed to allocate memory\n");
            close(pipefd[0]);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            if (output) {
                free(output);
            }
            FILE *fp = fopen(resultFilePath, "w");
            fprintf(fp, "%s\n", MEMORY_OUT_MESSAGE);
            fclose(fp);

            output = (char *)malloc(MEMORY_OUT_MESSAGE_LEN + 2);
            sprintf(output, "%s\n", MEMORY_OUT_MESSAGE);
            return output;
        }
        output = new_output;
        strcpy(output + output_size, buffer);
        output_size += bytes_read;
    }

    close(pipefd[0]);

    // Check if we timed out
    if ((time(NULL) - start_time) >= timeout) {
        logACoAC(__func__, __LINE__, 0, WARNING, "Command execution timed out\n");
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        char *newOutput = output ? (char *)malloc(output_size + TIMEOUT_MESSAGE_LEN + 2) : (char *)malloc(TIMEOUT_MESSAGE_LEN + 2);
        sprintf(newOutput, "%s\n%s", TIMEOUT_MESSAGE, output ? output : "");
        if (output) {
            free(output);
        }
        FILE *fp = fopen(resultFilePath, "w");
        fputs(newOutput, fp);
        fclose(fp);
        return newOutput;
    }

    // Wait for child process to finish
    int status;
    waitpid(pid, &status, 0);
    logACoAC(__func__, __LINE__, 0, INFO, "exit value: %d\n", status);

    // if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    //     logACoAC(__func__, __LINE__, 0, ERROR, "Command execution failed");
    //     if (output) {
    //         free(output);
    //     }
    //     return strdup("Command execution failed");
    // }

    FILE *fp = fopen(resultFilePath, "w");
    if (output) {
        fprintf(fp, "%s", output);
    }
    fclose(fp);

    return output ? output : strdup("");
}

char *runModelChecker(char *modelCheckerPath, char *nusmvFilePath, char *resultFilePath, long timeout, char *bound) {
    int bmc;
    char *args[6];
    if (bound == NULL) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] running model checker on smc mode\n");
        bmc = 0;
        args[0] = modelCheckerPath;
        args[1] = nusmvFilePath;
        args[2] = NULL;
    } else {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] running model checker on bmc mode, the bound is set to %s\n", bound);
        bmc = 1;
        args[0] = modelCheckerPath;
        args[1] = "-bmc";
        args[2] = "-bmc_length";
        args[3] = bound;
        args[4] = nusmvFilePath;
        args[5] = NULL;
    }
    clock_t startRun = clock();

    char *result = run(modelCheckerPath, args, resultFilePath, timeout);

    double spentTime = (clock() - startRun) / CLOCKS_PER_SEC * 1000;
    logACoAC(__func__, __LINE__, 0, INFO, "[end] running model checker on %s mode, cost => %.2fms\n", bmc ? "bmc" : "smc", spentTime);
    return result;
}

#include "acoac_translator.h"
#include <regex.h>

#define PATTERN_SMC_UNREACHABLE "-- specification .* is true"
#define PATTERN_BMC_UNREACHABLE "-- no counterexample found with bound"
#define PATTERN_BMC_UNREACHABLE_LEN 37

int findRule(HashMap *state, HashBasedTable *pTableTargetAV2Rule, AdminstrativeAction action) {
    int attrIdx = getAttrIndex(action.attr);
    int valIdx;
    if(getValueIndex(getAttrType(action.attr), action.val, &valIdx) != 0) {
        return -2;
    }
    HashSet **ppSetCandidateRules = (HashSet **)iHashBasedTable.Get(pTableTargetAV2Rule, &attrIdx, &valIdx);
    if (ppSetCandidateRules != NULL) {
        int ruleIdx;
        Rule *pRule;
        HashSetIterator *itSet = iHashSet.NewIterator(*ppSetCandidateRules);
        while (itSet->HasNext(itSet)) {
            ruleIdx = *(int *)itSet->GetNext(itSet);
            pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
            if (iRule.CanBeManaged(pRule, state)) {
                iHashMap.Put(state, &attrIdx, &valIdx);
                return ruleIdx;
            }
        }
    }
    return -1;
}

ACoACResult analyzeModelCheckerOutput(char *output, ACoACInstance *pInst, char *boundStr, int showRules) {
    logACoAC(__func__, __LINE__, 0, INFO, "analyzing the output of NuSMV\n");

    int waitAction = 0;
    int reachable = 0;
    char *attr, *val;

    if (output == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "the output of NuSMV is NULL\n");
        // todo: 错误处理
        return (ACoACResult){ACoAC_RESULT_ERROR, NULL, NULL};
    }
    char *line = strtok(output, "\n");
    // 分析是否超时
    if (strcmp(line, TIMEOUT_MESSAGE) == 0) {
        return (ACoACResult){ACoAC_RESULT_TIMEOUT, NULL, NULL};
    }

    char *variable, *p;
    Vector *pVecActions = iVector.Create(sizeof(AdminstrativeAction), 10);
    AdminstrativeAction action;
    regex_t pattern;
    regcomp(&pattern, PATTERN_SMC_UNREACHABLE, REG_EXTENDED);
    while (line != NULL) {
        if (boundStr != NULL && strncmp(line, PATTERN_BMC_UNREACHABLE, PATTERN_BMC_UNREACHABLE_LEN) == 0) {
            // 有界模型检测模式下，如果直到上界bound都没有counterexample被找到，那么说明结果为unreacheable
            line = strtrim(line + PATTERN_BMC_UNREACHABLE_LEN);
            if (strcmp(line, boundStr) == 0) {
                regfree(&pattern);
                return (ACoACResult){ACoAC_RESULT_UNREACHABLE, NULL, NULL};
            }
            line = strtok(NULL, "\n");
            continue;
        }

        // 符号模型检测模型下，使用正则表达式匹配判断是否unreachable
        if (regexec(&pattern, line, 0, NULL, 0) == 0) {
            regfree(&pattern);
            return (ACoACResult){ACoAC_RESULT_UNREACHABLE, NULL, NULL};
        }

        if (strstr(line, "State:")) {
            // 如果line中包含“State:”，意味者是目标状态是可达的，需要将解析出的管理操作记录下来
            if (reachable) {
                action = (AdminstrativeAction){pInst->queryUserIdx, pInst->queryUserIdx, attr, val};
                iVector.Add(pVecActions, &action);
            }
            reachable = 1;
            waitAction = 1;
            line = strtok(NULL, "\n");
            continue;
        }
        if (waitAction) {
            // 已遇见"State:"，所以当前line是管理操作，需要解析
            variable = strdup(line);
            p = variable;
            while (*p != '=' && *p != '\0') {
                p++;
            }
            if (*p != '\0') {
                *(p++) = '\0';
            } else {
                p = NULL;
            }
            variable = strtrim(variable);
            if (strcmp(variable, "attr") == 0) {
                attr = p;
                attr = strtrim(attr);
                attr[strlen(attr) - ALIAS_SUFFIX_LEN] = '\0';
            } else if (strcmp(variable, "val") == 0) {
                val = p;
                val = strtrim(val);
            }
        }
        line = strtok(NULL, "\n");
    }
    regfree(&pattern);

    if (!reachable) {
        return (ACoACResult){ACoAC_RESULT_ERROR, NULL, NULL};
    }
    if (showRules) {
        HashMap *pMapState = iHashMap.Create(sizeof(int), sizeof(int), IntHashCode, IntEqual);
        HashNode *node;
        HashNodeIterator *itMap = iHashMap.NewIterator(iHashBasedTable.GetRow(pInst->pTableInitState, &pInst->queryUserIdx));
        while (itMap->HasNext(itMap)) {
            node = itMap->GetNext(itMap);
            iHashMap.Put(pMapState, node->key, node->value);
        }
        iHashMap.DeleteIterator(itMap);

        Vector *pVecRules = iVector.Create(sizeof(Rule), iVector.Size(pVecActions));
        int i;
        for (i = 0; i < iVector.Size(pVecActions); i++) {
            AdminstrativeAction action = *(AdminstrativeAction *)iVector.GetElement(pVecActions, i);
            int ruleIdx = findRule(pMapState, pInst->pTableTargetAV2Rule, action);
            if (ruleIdx < 0) {
                logACoAC(__func__, __LINE__, 0, ERROR, "find no corresponding rule!\n");
            }
            iVector.Add(pVecRules, &ruleIdx);
        }
        return (ACoACResult){ACoAC_RESULT_REACHABLE, pVecActions, pVecRules};
    }
    return (ACoACResult){ACoAC_RESULT_REACHABLE, pVecActions, NULL};
}