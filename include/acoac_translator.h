#ifndef _ACoAC_TRANSLATOR_H
#define _ACoAC_TRANSLATOR_H

#include "acoac_inst.h"

#define ALIAS_SUFFIX "_2"
#define ALIAS_SUFFIX_LEN 2

int translate(ACoACInstance *instance, char *nusmvFilePath, int sliced);

#endif // _ACoAC_TRANSLATOR_H