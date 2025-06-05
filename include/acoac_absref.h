#ifndef ACoAC_ABS_REF_H
#define ACoAC_ABS_REF_H

#include "acoac_inst.h"

typedef struct AbsRef {
    // The current round of abstraction refinement
    int round;

    // The set of rules selected by the forward rule-selection strategy
    HashSet *pSetF;
    HashMap *pMapReachableAVs;
    HashMap *pMapReachableAVsInc;

    // The set of rules selected by the backward rule-selection strategy
    HashSet *pSetB;
    HashMap *pMapUsefulAVs;
    HashMap *pMapUsefulAVsInc;

    // The ACoAC instance before abstraction refinement
    ACoACInstance *pOriInst;

    // The global rule list before abstraction refinement
    Vector *pOriVecRules;
} AbsRef;

/**
 * Allocate necessary memory for the abstraction refinement.
 * 
 * @param pOriInst[in]: The ACoAC instance before abstraction refinement
 * @return The AbsRef instance
 */
AbsRef *createAbsRef(ACoACInstance *pOriInst);

/**
 * Generate an abstract sub-policy and get a smaller ACoAC instance.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 * @return A smaller ACoAC instance containing the abstracted sub-policy
 */
ACoACInstance *abstract(AbsRef *pAbsRef);

/**
 * Refine the ACoAC instance by adding more rules to the sub-policy.
 * If the instance cannot be refined, return NULL.
 * 
 * @param pAbsRef[in]: The AbsRef instance
 * @return The refined ACoAC instance
 */
ACoACInstance *refine(AbsRef *pAbsRef);

#endif //ACoAC_ABS_REF_H