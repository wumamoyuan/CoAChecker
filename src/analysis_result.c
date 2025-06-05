#include "analysis_result.h"
#include "acoac_utils.h"

/**
 * Print an administrative action.
 * 
 * @param action[in]: The administrative action to print
 */
static void printAction(AdminstrativeAction action) {
    printf("(");
    printf("%s,", istrCollection.GetElement(pscUsers, action.adminIdx));
    printf("%s,", istrCollection.GetElement(pscUsers, action.userIdx));
    printf("%s,", action.attr);
    printf("%s", action.val);
    printf(")");
}

void printResult(ACoACResult result, int showRules) {
    printf("****************RESULT****************\n");
    switch (result.code) {
    case ACoAC_RESULT_REACHABLE:
        printf("reachable\n");
        break;
    case ACoAC_RESULT_UNREACHABLE:
        printf("unreachable\n");
        break;
    case ACoAC_RESULT_TIMEOUT:
        printf("timeout\n");
        break;
    case ACoAC_RESULT_ERROR:
        printf("error\n");
        break;
    default:
        logACoAC(__func__, __LINE__, 0, ERROR, "Unexpected value: %d\n", result.code);
    }

    if (result.pVecActions != NULL) {
        printf("****************ACTIONS****************\n");
        int cnt = 1;
        AdminstrativeAction action;
        int ruleIdx;
        char *rStr;
        Rule *pRule;
        int i;
        if (showRules && result.pVecRules != NULL) {
            for (i = 0; i < iVector.Size(result.pVecActions); i++) {
                action = *(AdminstrativeAction *)iVector.GetElement(result.pVecActions, i);
                ruleIdx = *(int *)iVector.GetElement(result.pVecRules, i);
                printf("Step%d:\t", cnt++);
                printAction(action);
                if (ruleIdx >= 0) {
                    pRule = (Rule *)iVector.GetElement(pVecRules, ruleIdx);
                    rStr = RuleToString(&pRule);
                    printf(", %s", rStr);
                    free(rStr);
                }
                printf("\n");
            }
        } else {
            for (i = 0; i < iVector.Size(result.pVecActions); i++) {
                action = *(AdminstrativeAction *)iVector.GetElement(result.pVecActions, i);
                printf("Step%d:\t", cnt++);
                printAction(action);
                printf("\n");
            }
        }
    }
    printf("\n");
}