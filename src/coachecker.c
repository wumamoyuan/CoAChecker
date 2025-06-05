#include <dirent.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "acoac_absref.h"
#include "acoac_boundcal.h"
#include "acoac_io.h"
#include "acoac_pruning.h"
#include "acoac_translator.h"
#include "acoac_utils.h"
#include "ccl/containers.h"
#include "hashmap.h"
#include "hashset.h"
#include "mc_runner.h"
#include "precheck.h"

#define ACoAC_SUFFIX ".aabac"
#define ACoAC_SUFFIX_LEN 6
#define ARBAC_SUFFIX ".arbac"
#define ARBAC_SUFFIX_LEN 6
#define MOHAWK_SUFFIX ".mohawk"
#define MOHAWK_SUFFIX_LEN 7
#define SMV_SUFFIX ".smv"
#define SMV_SUFFIX_LEN 4

#define SLICING_RESULT_FILE_NAME "slicingResult"
#define SLICING_RESULT_FILE_NAME_LEN 13

#define ABSTRACTION_REFINEMENT_RESULT_FILE_NAME "abstractionRefinementResult"
#define ABSTRACTION_REFINEMENT_RESULT_FILE_NAME_LEN 27

#define NUSMV_FILE_NAME "lastSmvInstance"
#define NUSMV_FILE_NAME_LEN 15

#define RESULT_FILE_NAME "smvOutput"
#define RESULT_FILE_NAME_LEN 9

#define RESULT_SUFFIX ".txt"
#define RESULT_SUFFIX_LEN 4

static ACoACResult verify(char *modelCheckerPath, char *instFilePath, char *logDir, int doPrechecking,
                          int doSlicing, int enableAbstractRefine, int useBMC, int tl, int showRules, long timeout) {
    ACoACInstance *pInst = NULL;

    // read the instance file
    int instFilePathLen = strlen(instFilePath);
    if (instFilePathLen >= ACoAC_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - ACoAC_SUFFIX_LEN, ACoAC_SUFFIX) == 0) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] parsing ACoAC instance file\n");
        pInst = readACoACInstance(instFilePath);
    } else if ((instFilePathLen >= ARBAC_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - ARBAC_SUFFIX_LEN, ARBAC_SUFFIX) == 0) ||
               (instFilePathLen >= MOHAWK_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - MOHAWK_SUFFIX_LEN, MOHAWK_SUFFIX) == 0)) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] translating arbac instance file\n");
        pInst = readARBACInstance(instFilePath);
    } else {
        logACoAC(__func__, __LINE__, 0, ERROR, "illegal file type\n");
        return (ACoACResult){.code = ACoAC_RESULT_ERROR};
    }
    if (pInst == NULL) {
        return (ACoACResult){.code = ACoAC_RESULT_ERROR};
    }

    // initialize the instance
    init(pInst);

    // pre-checking
    if (doPrechecking) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] pre-checking\n");
        clock_t startPreCheck = clock();
        ACoACResult result = preCheck(pInst);
        clock_t endPreCheck = clock();
        double time_spent = (double)(endPreCheck - startPreCheck) / CLOCKS_PER_SEC * 1000;
        logACoAC(__func__, __LINE__, 0, INFO, "[end] pre-checking, cost ==> %.2fms\n", time_spent);
        if (result.code != ACoAC_RESULT_UNKNOWN) {
            logACoAC(__func__, __LINE__, 0, INFO, "preCheck success\n");
            printResult(result, showRules);
            return result;
        }
        logACoAC(__func__, __LINE__, 0, INFO, "preCheck failed\n");
    }

    pInst = userCleaning(pInst);
    char *writePath;

    ACoACResult result = {.code = ACoAC_RESULT_UNKNOWN};
    if (doSlicing) {
        // Global pruning
        pInst = slice(pInst, &result);
        if (result.code != ACoAC_RESULT_UNKNOWN) {
            printResult(result, showRules);
            return result;
        }

        writePath = (char *)malloc(strlen(logDir) + SLICING_RESULT_FILE_NAME_LEN + ACoAC_SUFFIX_LEN + 2);
        sprintf(writePath, "%s/%s%s", logDir, SLICING_RESULT_FILE_NAME, ACoAC_SUFFIX);
        writeACoACInstance(pInst, writePath);
        free(writePath);
    }

    AbsRef *pAbsRef;
    ACoACInstance *next;
    char roundStr[10];
    char boundStr[15];
    char *nusmvFilePath, *resultFilePath, *nusmvOutput;

    if (enableAbstractRefine) {
        // Generate an abstract sub-policy
        pAbsRef = createAbsRef(pInst);
        next = abstract(pAbsRef);
    } else {
        // no abstraction refinement
        next = pInst;
    }

    // Start the loop of abstraction refinement
    while (next != NULL) {
        sprintf(roundStr, "%d", enableAbstractRefine ? pAbsRef->round : 0);

        if (enableAbstractRefine) {
            // Save the abstract sub-policy in the log directory
            writePath = (char *)malloc(strlen(logDir) + ABSTRACTION_REFINEMENT_RESULT_FILE_NAME_LEN + strlen(roundStr) + ACoAC_SUFFIX_LEN + 2);
            sprintf(writePath, "%s/%s%s%s", logDir, ABSTRACTION_REFINEMENT_RESULT_FILE_NAME, roundStr, ACoAC_SUFFIX);
            writeACoACInstance(next, writePath);
            free(writePath);

            if (doSlicing) {
                // Local pruning
                next = slice(next, &result);
                if (result.code == ACoAC_RESULT_REACHABLE || (result.code == ACoAC_RESULT_UNREACHABLE && !enableAbstractRefine)) {
                    // Abstraction refinement is disabled and the safety of the sub-policy is determined, output the result
                    // Abstraction refinement is enabled and the sub-policy is determined to be "unsafe", also output the result
                    printResult(result, showRules);
                    logACoAC(__func__, __LINE__, 0, INFO, "round => %s\n", roundStr);
                    return result;
                }
                if (result.code == ACoAC_RESULT_UNREACHABLE) {
                    // Abstraction refinement is enabled and the sub-policy is determined to be "safe", need refinement and re-verification
                    printResult(result, showRules);
                    next = refine(pAbsRef);
                    continue;
                }

                // Save the pruned sub-policy in the log directory
                writePath = (char *)malloc(strlen(logDir) + SLICING_RESULT_FILE_NAME_LEN + strlen(roundStr) + ACoAC_SUFFIX_LEN + 2);
                sprintf(writePath, "%s/%s%s%s", logDir, SLICING_RESULT_FILE_NAME, roundStr, ACoAC_SUFFIX);
                writeACoACInstance(next, writePath);
                free(writePath);
            }
        }

        int tooLarge = 0;
        if (useBMC) {
            // Bound estimation, if the bound exceeds the range of int, use INT_MAX as the bound
            BigInteger bound = computeBound(next, tl);
            if (bound.magLen > 1 || (bound.magLen == 1 && (bound.mag[0] >> 31) != 0)) {
                logACoAC(__func__, __LINE__, 0, WARNING, "bound is too large, use INT_MAX as bound\n");
                sprintf(boundStr, "%d", INT_MAX);
                tooLarge = 1;
            } else {
                sprintf(boundStr, "%d", bound.signum == 0 ? 0 : bound.mag[0]);
            }
            iBigInteger.finalize(bound);
        }

        // Translate the instance to a NuSMV file
        nusmvFilePath = (char *)malloc(strlen(logDir) + NUSMV_FILE_NAME_LEN + strlen(roundStr) + SMV_SUFFIX_LEN + 2);
        sprintf(nusmvFilePath, "%s/%s%s%s", logDir, NUSMV_FILE_NAME, roundStr, SMV_SUFFIX);
        if (translate(next, nusmvFilePath, doSlicing) != 0) {
            logACoAC(__func__, __LINE__, 0, ERROR, "failed to translate instance to nusmv file\n");
            result.code = ACoAC_RESULT_ERROR;
            printResult(result, showRules);
            return result;
        }

        // Call the model checker to verify the instance and save the result in the log directory
        resultFilePath = (char *)malloc(strlen(logDir) + RESULT_FILE_NAME_LEN + strlen(roundStr) + RESULT_SUFFIX_LEN + 2);
        sprintf(resultFilePath, "%s/%s%s%s", logDir, RESULT_FILE_NAME, roundStr, RESULT_SUFFIX);
        nusmvOutput = runModelChecker(modelCheckerPath, nusmvFilePath, resultFilePath, timeout, useBMC ? boundStr : NULL);

        // Analyze the result of the model checker
        result = analyzeModelCheckerOutput(nusmvOutput, next, useBMC ? boundStr : NULL, showRules);
        free(nusmvOutput);

        if (tooLarge && result.code == ACoAC_RESULT_UNREACHABLE) {
            // The bound exceeds the range of int and the model checker result is "unreachable", need re-verification in SMC mode
            nusmvOutput = runModelChecker(modelCheckerPath, nusmvFilePath, resultFilePath, timeout, NULL);
            result = analyzeModelCheckerOutput(nusmvOutput, next, NULL, showRules);
            free(nusmvOutput);
        }
        free(nusmvFilePath);
        free(resultFilePath);

        logACoAC(__func__, __LINE__, 0, INFO, "\n");
        printResult(result, showRules);

        // If abstraction refinement is disabled or the sub-policy is not determined to be "unsafe", output the result
        if (!enableAbstractRefine || result.code != ACoAC_RESULT_UNREACHABLE) {
            logACoAC(__func__, __LINE__, 0, INFO, "round => %s\n", roundStr);
            return result;
        }

        // Abstraction refinement is enabled and the sub-policy is determined to be "unsafe", need refinement and re-verification
        next = refine(pAbsRef);
    }
    logACoAC(__func__, __LINE__, 0, INFO, "round => %s\n", roundStr);
    return result;
}

#include <unistd.h>

char *computeBoundTightnessForFile(char *instFilePath, char *resultFile, int *scale) {
    ACoACInstance *pInst = NULL;

    // read the instance file
    int instFilePathLen = strlen(instFilePath);
    if (instFilePathLen >= ACoAC_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - ACoAC_SUFFIX_LEN, ACoAC_SUFFIX) == 0) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] parsing ACoAC instance file\n");
        pInst = readACoACInstance(instFilePath);
    } else if ((instFilePathLen >= ARBAC_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - ARBAC_SUFFIX_LEN, ARBAC_SUFFIX) == 0) ||
               (instFilePathLen >= MOHAWK_SUFFIX_LEN && strcmp(instFilePath + instFilePathLen - MOHAWK_SUFFIX_LEN, MOHAWK_SUFFIX) == 0)) {
        logACoAC(__func__, __LINE__, 0, INFO, "[start] translating arbac instance file\n");
        pInst = readARBACInstance(instFilePath);
    } else {
        logACoAC(__func__, __LINE__, 0, ERROR, "illegal file type\n");
        exit(1);
    }
    if (pInst == NULL) {
        exit(1);
    }

    init(pInst);

    // compute the trivial bound and the tightest bound
    BigInteger b1 = computeBound(pInst, 1);
    BigInteger b2 = computeBound(pInst, 2);

    finalizeACoACInstance(pInst);
    finalizeGlobalVars();

    char *s1 = iBigInteger.toString(b1);
    char *s2 = iBigInteger.toString(b2);
    int numZeros = strlen(s1) - strlen(s2);
    BigInteger extendedB2 = iBigInteger.bigMultiplyPowerTen(b2, numZeros + 10);
    iBigInteger.finalize(b2);

    BigInteger quotient;
    iBigInteger.divideKnuth(extendedB2, b1, &quotient, 0);
    quotient.signum = extendedB2.signum * b1.signum;
    iBigInteger.finalize(extendedB2);
    iBigInteger.finalize(b1);

    char *qstr = iBigInteger.toString(quotient);
    printf("%s,%s,%s,%s,%d\n", instFilePath, s1, s2, qstr, numZeros + 10);

    if (resultFile != NULL) {
        FILE *fp = NULL;
        if (access(resultFile, F_OK) == -1) {
            // if resultFile does not exist, create it and write the header
            fp = fopen(resultFile, "w");
            fprintf(fp, "instFile,b1,b2,tightness\n");
        } else {
            fp = fopen(resultFile, "a");
        }
        fprintf(fp, "%s,%s,%s,%s,%d\n", instFilePath, s1, s2, qstr, numZeros + 10);
        fclose(fp);
    }
    iBigInteger.finalize(quotient);
    free(s1);
    free(s2);
    if (scale != NULL) {
        *scale = numZeros + 10;
    }
    return qstr;
}

void computeBoundTightness(char *path, char *resultFile) {
    // Check if instDirPath exists
    if (access(path, F_OK) == -1) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Path %s does not exist\n", path);
        return;
    }

    // Check if instDirPath is a directory
    struct stat path_stat;
    stat(path, &path_stat);
    if (!S_ISDIR(path_stat.st_mode)) {
        computeBoundTightnessForFile(path, resultFile, NULL);
        return;
    }

    // Traverse the directory
    DIR *dir = opendir(path);
    if (dir == NULL) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to open directory %s\n", path);
        return;
    }

    int capacity = 8;
    int instCount = 0;
    char **tightnessInts = (char **)malloc(capacity * sizeof(char *));
    int *tightnessIntsLen = (int *)malloc(capacity * sizeof(int));
    int *scales = (int *)malloc(capacity * sizeof(int));
    int maxScale = 0, minScale = INT_MAX;

    // For each file in the directory, invoke computeBoundTightnessForFile to compute the tightness of the bound
    struct dirent *entry;
    char *fullPath;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path
        fullPath = (char *)malloc(strlen(path) + strlen(entry->d_name) + 2);
        sprintf(fullPath, "%s/%s", path, entry->d_name);

        // Check if it's a directory
        stat(fullPath, &path_stat);
        if (!S_ISDIR(path_stat.st_mode)) {
            // Check if file has valid extension
            int pathLen = strlen(fullPath);
            if ((pathLen >= ACoAC_SUFFIX_LEN && strcmp(fullPath + pathLen - ACoAC_SUFFIX_LEN, ACoAC_SUFFIX) == 0) ||
                (pathLen >= ARBAC_SUFFIX_LEN && strcmp(fullPath + pathLen - ARBAC_SUFFIX_LEN, ARBAC_SUFFIX) == 0) ||
                (pathLen >= MOHAWK_SUFFIX_LEN && strcmp(fullPath + pathLen - MOHAWK_SUFFIX_LEN, MOHAWK_SUFFIX) == 0)) {

                if (instCount >= capacity) {
                    capacity *= 2;
                    tightnessInts = (char **)realloc(tightnessInts, capacity * sizeof(char *));
                    tightnessIntsLen = (int *)realloc(tightnessIntsLen, capacity * sizeof(int));
                    scales = (int *)realloc(scales, capacity * sizeof(int));
                }
                tightnessInts[instCount] = computeBoundTightnessForFile(fullPath, NULL, scales + instCount);
                tightnessIntsLen[instCount] = strlen(tightnessInts[instCount]);
                if (scales[instCount] > maxScale) {
                    maxScale = scales[instCount];
                }
                if (scales[instCount] < minScale) {
                    minScale = scales[instCount];
                }
                instCount++;
            }
        }
        free(fullPath);
    }
    closedir(dir);

    // Compute the sum of the tightness of the bound for all files in the directory
    // For each file, the tightness of the bound is computed as tightnessInt * 10^(-scale)
    // To compute the sum, we pad tightnessInts[i] with (maxScales - scales[i]) 0s, so that their scales are the same
    // Then we add the tightnessInts from the least significant digit to the most significant digit
    int i, j, carry = 0;
    char instCountStr[20];
    sprintf(instCountStr, "%d", instCount);
    int ndigit = strlen(instCountStr) + 13 + maxScale - minScale;
    char *digits = (char *)malloc(ndigit * sizeof(char));
    digits[ndigit - 1] = '\0';
    int stop;
    for (i = 1; i < ndigit; i++) {
        stop = 1;
        for (j = 0; j < instCount; j++) {
            int index = tightnessIntsLen[j] - 1 + maxScale - scales[j] - i + 1;
            if (index < 0) {
                continue;
            }
            if (stop) {
                stop = 0;
            }
            if (index >= tightnessIntsLen[j]) {
                continue;
            }
            carry += tightnessInts[j][index] - '0';
        }
        if (stop) {
            break;
        }
        digits[ndigit - i - 1] = carry % 10 + '0';
        carry = carry / 10;
    }
    while (carry > 0) {
        digits[ndigit - i - 1] = carry % 10 + '0';
        carry = carry / 10;
        i++;
    }
    int scale = maxScale;
    int actualLen = strlen(digits + ndigit - i);
    digits[ndigit - i - 1] = digits[ndigit - i];
    digits[ndigit - i] = '.';
    scale -= actualLen - 1;

    double sum = atof(digits + ndigit - i - 1);
    double average = sum / instCount;
    while (average < 1) {
        scale++;
        average = average * 10;
    }
    printf("%s,%fE-%d\n", path, average, scale);
    if (resultFile != NULL) {
        FILE *fp = NULL;
        if (access(resultFile, F_OK) == -1) {
            // if resultFile does not exist, create it and write the header
            fp = fopen(resultFile, "w");
            fprintf(fp, "dataset,average tightness\n");
        } else {
            fp = fopen(resultFile, "a");
        }
        fprintf(fp, "%s,%fE-%d\n", path, average, scale);
        fclose(fp);
    }

    for (i = 0; i < instCount; i++) {
        free(tightnessInts[i]);
    }
    free(tightnessInts);
    free(tightnessIntsLen);
    free(scales);
    free(digits);
}

int main(int argc, char *argv[]) {
    int help = 0;
    int doPrechecking = 1;
    int doSlicing = 1;
    int enableAbstractRefine = 1;
    int useBMC = 1;
    int showRules = 1;

    int tl = 2;
    char *modelCheckerPath = NULL;
    char *inputPath = NULL;
    char *logDir = NULL;
    long timeout = 60;
    int computeTightness = 0;
    char *outputPath = NULL;

    int unrecognized = 0;

    char *helpMessage = "Usage: acoac-verifier\
        \n-tl|-b <arg>                   tight level, either 1 (loose) or 2 (tight)\
        \n-help|-h                       print the help text\
        \n-input|-i <arg>                acoac file path\
        \n-model_checker|-m <arg>        nusmv file path\
        \n-log_dir|-l <arg>              directory for storing logs\
        \n-no_absref|-a                  no abstraction refinement\
        \n-no_precheck|-p                no precheck\
        \n-no_slicing|-s                 no slicing\
        \n-no_rules|-r                   do not show the rules associated with the actions in the result\
        \n-smc|-n                        on smc mode\
        \n-timeout|-t <arg>              timeout in seconds\
        \n-compute_tightness|-c          compute the tightness of the bound\
        \n-output|-o <arg>               output file path for saving the tightness of the bound\n";

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"no_precheck", no_argument, 0, 'p'},
        {"no_slicing", no_argument, 0, 's'},
        {"no_absref", no_argument, 0, 'a'},
        {"smc", no_argument, 0, 'n'},
        {"tl", required_argument, 0, 'b'},
        {"no_rules", no_argument, 0, 'r'},
        {"model_checker", required_argument, 0, 'm'},
        {"input", required_argument, 0, 'i'},
        {"log_dir", required_argument, 0, 'l'},
        {"timeout", required_argument, 0, 't'},
        {"compute_tightness", no_argument, 0, 'c'},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}};

    int c;
    while (1) {
        int option_index = 0;

        c = getopt_long_only(argc, argv, "hpsanb:rm:i:l:t:co:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {
        case 'h':
            help = 1;
            break;
        case 'p':
            doPrechecking = 0;
            break;
        case 's':
            doSlicing = 0;
            break;
        case 'a':
            enableAbstractRefine = 0;
            break;
        case 'n':
            useBMC = 0;
            break;
        case 'r':
            showRules = 0;
            break;
        case 'b':
            tl = atoi(optarg);
            if (tl != 1 && tl != 2) {
                printf("tight level should be either 1 (loose) or 2 (tight)\n");
                return 0;
            }
            break;
        case 'm':
            modelCheckerPath = (char *)malloc(strlen(optarg) + 1);
            strcpy(modelCheckerPath, optarg);
            break;
        case 'i':
            inputPath = (char *)malloc(strlen(optarg) + 1);
            strcpy(inputPath, optarg);
            break;
        case 'l':
            logDir = (char *)malloc(strlen(optarg) + 1);
            strcpy(logDir, optarg);
            break;
        case 't':
            timeout = atol(optarg);
            break;
        case 'c':
            computeTightness = 1;
            break;
        case 'o':
            outputPath = (char *)malloc(strlen(optarg) + 1);
            strcpy(outputPath, optarg);
            break;
        default:
            unrecognized = 1;
            break;
        }
    }

    if (unrecognized) {
        printf("Found unrecognized option\n%s", helpMessage);
    } else if (help) {
        printf("%s", helpMessage);
    } else if (!inputPath) {
        printf("please input the file path of acoac instance\n%s", helpMessage);
    } else if (computeTightness) {
        computeBoundTightness(inputPath, outputPath);
    } else if (!modelCheckerPath) {
        printf("please input the file path of model checker\n%s", helpMessage);
    } else if (!logDir) {
        printf("please input the directory for storing logs\n%s", helpMessage);
    } else if (timeout <= 0) {
        printf("timeout must be greater than 0\n%s", helpMessage);
    } else {
        clock_t start = clock();
        verify(modelCheckerPath, inputPath, logDir, doPrechecking, doSlicing, enableAbstractRefine, useBMC, tl, showRules, timeout);
        clock_t end = clock();
        double time_spent = (double)(end - start) / CLOCKS_PER_SEC * 1000;
        logACoAC(__func__, __LINE__, 0, INFO, "end verification, cost => %.2fms\n", time_spent);
    }
    return 0;
}