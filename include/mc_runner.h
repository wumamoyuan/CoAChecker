#ifndef NUSMV_RUNNER_H
#define NUSMV_RUNNER_H

#include "analysis_result.h"

char *runModelChecker(char *modelCheckerPath, char *nusmvFilePath, char *resultFilePath, long timeout, char *bound);
ACoACResult analyzeModelCheckerOutput(char *output, ACoACInstance *pInst, char *boundStr, int showRules);

#endif // NUSMV_RUNNER_H
