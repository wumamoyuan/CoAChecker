#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>

#include "acoac_utils.h"

#define TIMEOUT -1
#define ERROR -2
#define FILE_NOT_FOUND -3

static double convert(char *totalCost)
{
    totalCost[strlen(totalCost) - 1] = '\0';
    char *minute = strtrim(strtok(totalCost, "m"));
    char *second = strtrim(strtok(NULL, "m"));
    double sec = atoi(minute) * 60 + atof(second);
    return sec;
}

static int analyzeLogFile(char *logFile, int tool, double *arr, double *timecost)
{
    FILE *fp = fopen(logFile, "r");
    if (fp == NULL)
    {
        printf("Failed to open file %s!\n", logFile);
        return 1;
    }

    int readResult = 0;
    char result[20] = {'\0'};

    int enableAbsRef = 0;
    char *pattern = "rules => ([0-9]+)/([0-9]+)";
    regex_t regex;
    regcomp(&regex, pattern, REG_EXTENDED);
    regmatch_t pmatch[3];

    // 动态分配初始缓冲区
    size_t buffer_size = 1024;
    char *line = (char *)malloc(buffer_size);
    if (line == NULL)
    {
        printf("Failed to allocate memory for line buffer\n");
        fclose(fp);
        return 1;
    }

    while (1)
    {
        // 尝试读取一行
        if (fgets(line, buffer_size, fp) == NULL)
        {
            // 文件结束或读取错误
            break;
        }

        // 检查是否需要扩展缓冲区
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] != '\n' && !feof(fp))
        {
            // 行被截断，需要扩展缓冲区
            size_t new_size = buffer_size * 2;
            char *new_line = (char *)realloc(line, new_size);
            if (new_line == NULL)
            {
                printf("Failed to reallocate memory for line buffer\n");
                free(line);
                fclose(fp);
                return 1;
            }
            line = new_line;
            buffer_size = new_size;

            // 继续读取同一行的剩余部分
            while (fgets(line + line_len, buffer_size - line_len, fp) != NULL)
            {
                line_len = strlen(line);
                if (line_len > 0 && line[line_len - 1] == '\n')
                {
                    break; // 行已完整读取
                }
                if (feof(fp))
                {
                    break; // 文件结束
                }
                // 再次扩展缓冲区
                new_size = buffer_size * 2;
                new_line = (char *)realloc(line, new_size);
                if (new_line == NULL)
                {
                    printf("Failed to reallocate memory for line buffer\n");
                    free(line);
                    fclose(fp);
                    return 1;
                }
                line = new_line;
                buffer_size = new_size;
            }
        }

        if (strncmp(line, "real", 4) == 0)
        {
            // printf("%s", line);
            char *timeLine = line + 4;
            timeLine = strtrim(timeLine);
            *timecost = convert(timeLine);
        }
        else if (tool < 4)
        {
            if (strstr(line, "***RESULT***") != NULL)
            {
                readResult = 1;
            }
            else if (readResult)
            {
                strcpy(result, strtrim(line));
                readResult = 0;
            }
            else if (strncmp(line, "Exception in", 12) == 0 && strstr(line, "process hasn't exited") != NULL)
            {
                strcpy(result, "timeout");
            }
            else if (strstr(line, "[Start] abstracting policy") != NULL)
            {
                enableAbsRef = 1;
            }
            else if (arr != NULL && regexec(&regex, line, 3, pmatch, 0) == 0)
            {
                line[pmatch[1].rm_eo] = '\0';
                line[pmatch[2].rm_eo] = '\0';
                *arr = atof(line + pmatch[1].rm_so) / atof(line + pmatch[2].rm_so);
            }
        }
        else if (tool == 4)
        {
            if (strstr(line, "[COMPLETED] Result:") != NULL || readResult)
            {
                readResult = 0;
                if (strstr(line, "ERROR_OCCURRED") != NULL)
                {
                    strcpy(result, "error");
                }
                else if (strstr(line, "TIMEOUT") != NULL)
                {
                    strcpy(result, "timeout");
                }
                else if (strstr(line, "GOAL_REACHABLE") != NULL)
                {
                    strcpy(result, "reachable");
                }
                else if (strstr(line, "GOAL_UNREACHABLE") != NULL)
                {
                    strcpy(result, "unreachable");
                }
                else
                {
                    readResult = 1;
                }
            }
        }
        else
        {
            if (strstr(line, "The ARBAC policy is safe") != NULL)
            {
                strcpy(result, "unreachable");
            }
            else if (strstr(line, "The ARBAC policy may not be safe") != NULL)
            {
                strcpy(result, "reachable");
            }
            else if (strstr(line, "There is something wrong with the analyzed file") != NULL)
            {
                strcpy(result, "error");
            }
        }
    }
    if (line != NULL)
    {
        free(line);
    }
    fclose(fp);

    if (strcmp(result, "timeout") == 0)
    {
        *timecost = TIMEOUT;
    }
    else if (strcmp(result, "error") == 0)
    {
        *timecost = ERROR;
    }
    else if (strcmp(result, "reachable") != 0 && strcmp(result, "unreachable") != 0)
    {
        printf("Unexpected result: %s, log file: %s\n", result, logFile);
        exit(1);
    }
    if (arr != NULL && (strcmp(result, "reachable") != 0 || enableAbsRef == 0))
    {
        *arr = -1;
    }
    return 0;
}

static int analyzeEfficiency(char *logDir, char *outputCsvFile)
{
    char *logFile = (char *)malloc(strlen(logDir) + 50);

    int toolNum = 6;
    char *toolNames[] = {"coachecker", "coachecker-noslicing", "coachecker-noabsref", "coachecker-smc", "mohawk", "vac"};
    char *shortToolNames[] = {"C.w.a", "C.wo.p", "C.wo.ar", "C.wo.be", "M", "V"};
    int toolsDetected[] = {0, 0, 0, 0, 0, 0};
    char *suffixes[] = {"all", "noslicing", "noabsref", "smc", "mohawk", "vac"};

    int i, j, ret;
    int instanceNum = 50;
    double timecost, timecosts[instanceNum][toolNum];
    for (i = 1; i <= instanceNum; i++)
    {
        for (j = 0; j < toolNum; j++)
        {
            sprintf(logFile, "%s/output%d-%s.txt", logDir, i, suffixes[j]);
            if (access(logFile, F_OK) == -1)
            {
                timecost = FILE_NOT_FOUND;
                if (toolsDetected[j] == 1)
                {
                    printf("\nThe number of log files of tool %s is less than %d\n", toolNames[j], instanceNum);
                    exit(1);
                }
            }
            else
            {
                if (toolsDetected[j] == 0)
                {
                    if (i == 1)
                    {
                        toolsDetected[j] = 1;
                    }
                    else
                    {
                        printf("\nThe number of log files of tool %s is less than %d\n", toolNames[j], instanceNum);
                        exit(1);
                    }
                }
                ret = analyzeLogFile(logFile, j, NULL, &timecost);
                if (ret)
                {
                    printf("\nFailed to analyze file %s!\n", logFile);
                    return 1;
                }
            }
            timecosts[i - 1][j] = timecost;
        }
    }

    FILE *fp = NULL;
    if (outputCsvFile != NULL && access(outputCsvFile, F_OK) == -1)
    {
        fp = fopen(outputCsvFile, "w");
        fprintf(fp, "dataset");
    }
    printf("dataset");

    for (i = 0; i < toolNum; i++)
    {
        if (toolsDetected[i] == 0)
        {
            continue;
        }
        printf(",SR-%s", shortToolNames[i]);
        if (fp != NULL)
        {
            fprintf(fp, ",SR-%s", shortToolNames[i]);
        }
    }
    for (i = 0; i < toolNum; i++)
    {
        if (toolsDetected[i] == 0)
        {
            continue;
        }
        for (j = i + 1; j < toolNum; j++)
        {
            if (toolsDetected[j] == 0)
            {
                continue;
            }
            printf(",AVT-%s,AVT-%s,TD-%s-%s", shortToolNames[i], shortToolNames[j], shortToolNames[i], shortToolNames[j]);
            if (fp != NULL)
            {
                fprintf(fp, ",AVT-%s,AVT-%s,TD-%s-%s", shortToolNames[i], shortToolNames[j], shortToolNames[i], shortToolNames[j]);
            }
        }
    }
    printf("\n");
    if (fp != NULL)
    {
        fprintf(fp, "\n");
    }

    if (outputCsvFile != NULL && fp == NULL)
    {
        fp = fopen(outputCsvFile, "a");
    }

    char *dataset = logDir + strlen(logDir) - 1;
    while (dataset >= logDir && *dataset != '/')
    {
        dataset--;
    }
    dataset++;
    printf("%s", dataset);
    if (fp != NULL)
    {
        fprintf(fp, "%s", dataset);
    }

    int count;
    for (i = 0; i < toolNum; i++)
    {
        if (toolsDetected[i] == 0)
        {
            continue;
        }
        count = 0;
        for (j = 0; j < instanceNum; j++)
        {
            if (timecosts[j][i] <= 0)
            {
                continue;
            }
            count++;
        }
        printf(",%d%%", count * 100 / instanceNum);
        if (fp != NULL)
        {
            fprintf(fp, ",%d%%", count * 100 / instanceNum);
        }
    }

    int k;
    double avgTimeCostI, avgTimeCostJ;
    for (i = 0; i < toolNum; i++)
    {
        if (toolsDetected[i] == 0)
        {
            continue;
        }
        for (j = i + 1; j < toolNum; j++)
        {
            if (toolsDetected[j] == 0)
            {
                continue;
            }
            // Compare the average time cost of tool i and tool j
            avgTimeCostI = 0, avgTimeCostJ = 0;
            count = 0;
            for (k = 0; k < instanceNum; k++)
            {
                if (timecosts[k][i] <= 0 || timecosts[k][j] <= 0)
                {
                    continue;
                }
                count++;
                avgTimeCostI += timecosts[k][i];
                avgTimeCostJ += timecosts[k][j];
            }
            avgTimeCostI /= count;
            avgTimeCostJ /= count;
            printf(",%.3f,%.3f,%.3f", avgTimeCostI, avgTimeCostJ, avgTimeCostJ - avgTimeCostI);
            if (fp != NULL)
            {
                fprintf(fp, ",%.3f,%.3f,%.3f", avgTimeCostI, avgTimeCostJ, avgTimeCostJ - avgTimeCostI);
            }
        }
    }
    printf("\n");
    if (fp != NULL)
    {
        fprintf(fp, "\n");
        fclose(fp);
    }

    return 0;
}

static int computeARR(char *logDir, char *outputCsvFile)
{
    char *logFile = (char *)malloc(strlen(logDir) + 50);

    int i, j, ret;
    int instanceNum = 50, arrcnt = 0;
    double timecost, arr, arrsum = 0;
    for (i = 1; i <= instanceNum; i++)
    {
        sprintf(logFile, "%s/output%d-all.txt", logDir, i);
        if (access(logFile, F_OK) == -1)
        {
            printf("\nThe number of log files is less than %d\n", instanceNum);
            return 1;
        }

        ret = analyzeLogFile(logFile, 0, &arr, &timecost);
        if (ret)
        {
            printf("\nFailed to analyze file %s!\n", logFile);
            return 1;
        }
        if (arr >= 0)
        {
            arrsum += arr;
            arrcnt++;
        }
    }

    char arrstr[10];
    if (arrcnt == 0)
    {
        sprintf(arrstr, "none");
    }
    else
    {
        sprintf(arrstr, "%.4f", arrsum / arrcnt);
    }
    printf("ARR: %s\n", arrstr);

    if (outputCsvFile != NULL)
    {
        FILE *fp = NULL;
        if (access(outputCsvFile, F_OK) == -1)
        {
            fp = fopen(outputCsvFile, "w");
            fprintf(fp, "log directory,ARR\n");
        }
        else
        {
            fp = fopen(outputCsvFile, "a");
        }
        fprintf(fp, "%s, %s\n", logDir, arrstr);
        fclose(fp);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char *helpMessage = "Usage: log_analyzer\
        \n[-a|--arr]                    computing the average abstraction refinement rate\
        \n[-l|--logdir] <arg>           the directory saving log files\
        \n[-o|--output-csvfile] <arg>   the path of the output csv file\n";

    int arr = 0;
    char *logDir = NULL;
    char *outputCsvFile = NULL;

    int unrecognized = 0;

    static struct option long_options[] = {
        {"arr", no_argument, 0, 'a'},
        {"logdir", required_argument, 0, 'l'},
        {"output-csvfile", required_argument, 0, 'o'},
        {0, 0, 0, 0}};

    int c;
    while (1)
    {
        int option_index = 0;

        c = getopt_long(argc, argv, "l:o:a", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'l':
            logDir = (char *)malloc(strlen(optarg) + 1);
            strcpy(logDir, optarg);
            break;
        case 'o':
            outputCsvFile = (char *)malloc(strlen(optarg) + 1);
            strcpy(outputCsvFile, optarg);
            break;
        case 'a':
            arr = 1;
            break;
        default:
            unrecognized = 1;
            printf("Unrecognized option: %s\n", argv[optind]);
            break;
        }
    }

    if (unrecognized)
    {
        printf("%s\n", helpMessage);
        return 1;
    }

    if (logDir == NULL)
    {
        printf("Error: log directory is required\n");
        printf("%s\n", helpMessage);
        return 1;
    }
    // printf("log directory: %s\n", logDir);

    if (arr)
    {
        return computeARR(logDir, outputCsvFile);
    }
    return analyzeEfficiency(logDir, outputCsvFile);
}
