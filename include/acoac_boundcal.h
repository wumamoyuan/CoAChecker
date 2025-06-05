#ifndef ACoAC_BOUND_CALCULATOR_H
#define ACoAC_BOUND_CALCULATOR_H

#include "acoac_inst.h"
#include "bigint.h"

/**
 * Bound estimation for an ACoAC instance. Used to invoke bounded model checking.
 * 
 * @param pInst[in]: The ACoAC instance
 * @param tightLevel[in]: The tight level of the bound
 * @return The bound of the ACoAC instance
 */
BigInteger computeBound(ACoACInstance *pInst, int tightLevel);

#endif // ACoAC_BOUND_CALCULATOR_H
