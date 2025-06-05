#ifndef ACoAC_PRUNING_H
#define ACoAC_PRUNING_H

#include "analysis_result.h"

ACoACInstance *userCleaning(ACoACInstance *pInst);

ACoACInstance *slice(ACoACInstance *pInst, ACoACResult *pResult);

#endif // _ACoAC_PRUNING_H
