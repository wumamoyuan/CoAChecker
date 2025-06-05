#ifndef ACoAC_RESULT_H
#define ACoAC_RESULT_H

#define ACoAC_RESULT_ERROR -1
#define ACoAC_RESULT_TIMEOUT -2
#define ACoAC_RESULT_UNKNOWN -3
#define ACoAC_RESULT_REACHABLE 0
#define ACoAC_RESULT_UNREACHABLE 1

#include "acoac_inst.h"
#include "ccl/containers.h"

/* An administrative action, consisting of an administrator, a user, an attribute, and a value. */
typedef struct _AdminstrativeAction {
    // The index of the administrator who performs the action
    int adminIdx;
    // The index of the user whose attribute is modified by the action
    int userIdx;
    // The attribute that is modified
    char *attr;
    // The value that is assigned to the attribute
    char *val;
} AdminstrativeAction;

typedef struct _ACoACResult {
    // The result code, 0: reachable, 1: unreachable, -1: error, -2: timeout, -3: unknown
    int code;
    // The sequence of administrative actions that lead to the reachability of the target state
    Vector *pVecActions;
    // The sequence of rules that are used to authorize the sequence of administrative actions
    Vector *pVecRules;
} ACoACResult;

/**
 * Print the ACoAC-safety analysis result.
 * 
 * @param result[in]: The result of the ACoAC-safety analysis
 * @param showRules[in]: Whether to show the rules
 */
void printResult(ACoACResult result, int showRules);

#endif
