#ifndef _ACoACData_H
#define _ACoACData_H

#include "ccl/containers.h"
#include "ccl/ccl_internal.h"
#include "hashbasedtable.h"
#include "acoac_rule.h"

typedef struct _ACoACInstance
{
    // List of user indices
    Vector *pVecUserIndices;
    
    // A map from an attribute to its domain
    // The attribute domain is a hashset of value indices
    HashMap *pMapAttr2Dom;

    // A map from a user-attribute pair to the initial value
    // E.g., if in the initial state, the value of attribute a for user u is v, then (u, a) -> v
    HashBasedTable *pTableInitState;
    
    // List of rule indices
    HashSet *pSetRuleIdxes;

    // A map from an attribute-value pair to a set of rules whose target is the pair
    // E.g., if rule r=(cond1, cond2, a, v), then (a, v) -> {r}
    HashBasedTable *pTableTargetAV2Rule;

    // A map from an attribute-value pair to a set of rules for which the pair is necessary for the rule to fire
    // Note: the admin condition is ignored
    // E.g., if rule r=(a'=v', a1=v1 & a2=v2, a3, v3), then (a1, v1) -> {r}, (a2, v2) -> {r}
    HashBasedTable *pTablePrecond2Rule;
    
    // The index of the target user in the safety query
    int queryUserIdx;

    // The attribute-value pairs in the safety query
    // E.g., if the safety query is "(u1, a1=v1 & a2=v2)", then pmapQueryAVs={(a1, v1), (a2, v2)}
    HashMap *pmapQueryAVs;
} ACoACInstance;

// A global string list storing all user names
extern strCollection *pscUsers;

// A global map from user names to their indices in the @{pscUsers} list
extern Dictionary *pdictUser2Index;

// A global string list storing all attribute names
extern strCollection *pscAttrs;

// A global map from attribute names to their indices in the @{pscAttrs} list
extern Dictionary *pdictAttr2Index;

// A global string list storing all string values
extern strCollection *pscValues;

// A global map from string values to their indices in the @{pscValues} list
extern Dictionary *pdictValue2Index;

// A global map from attribute indices to their data types
extern HashMap *pmapAttr2Type;

// A global map from attribute indices to the indices of their default values
extern HashMap *pmapAttr2DefVal;

// A global list storing all rules (NOT rule pointers)
extern Vector *pVecRules;

/**
 * Allocate memory for the global variables
 */
void initGlobalVars();

/**
 * Finalize the global variables
 */
void finalizeGlobalVars();

/**
 * Create an ACoAC instance and allocate memory for its members.
 * 
 * @return A pointer to the created ACoAC instance
 */
ACoACInstance *createACoACInstance();

/**
 * Free the memory allocated for the instance and its members.
 * 
 * @param pInst[in] A pointer to the ACoAC instance to be finalized
 */
void finalizeACoACInstance(ACoACInstance *pInst);

/**
 * Get the index of a user in the global list of users @{pscUsers}.
 * 
 * @param user[in] A user name
 * @return The index of the user, or -1 if the user does not exist
 */
int getUserIndex(char *user);

/**
 * Get the index of an attribute in the global list of attributes @{pscAttrs}.
 * 
 * @param attr[in] An attribute name
 * @return The index of the attribute, or -1 if the attribute does not exist
 */
int getAttrIndex(char *attr);

/**
 * Get the index of a given attribute value. It depends on the attribute datatype.
 * Specifically, for a boolean attribute, the value index is 1 if the value is "true", and 0 if the value is "false". 
 * For an integer attribute, the value index is the integer value itself.
 * For a string attribute, the value index is the index of the string in the global list of strings @{pscValues}.
 * 
 * @param attrType[in] The datatype of the attribute
 * @param value[in] An attribute value
 * @param pValueIdx[out] A pointer to the index of the value
 * @return 0 if the value index is successfully obtained, or -1 if
 *      1) the attribute is boolean and the given value is not "true" or "false",
 *      2) the attribute is integer and the given value is not an integer, or
 *      3) the attribute is string and the given value is not in the global list of strings @{pscValues}
 */
int getValueIndex(AttrType attrType, char *value, int *pValueIdx);

/**
 * Get the datatype of an attribute.
 * 
 * @param attr[in] An attribute name
 * @return The datatype of the attribute
 */
AttrType getAttrType(char *attr);

/**
 * Get the datatype of an attribute by its index.
 * 
 * @param attrIdx[in] The index of the attribute
 * @return The datatype of the attribute
 */
AttrType getAttrTypeByIdx(int attrIdx);

/**
 * Get the value of an attribute by its index. It depends on the attribute datatype.
 * If the attribute is boolean, the value is "true" if the index is 1, and "false" if the index is 0.
 * If the attribute is integer, the value is the integer itself.
 * If the attribute is string, the value is the string in the global list of strings @{pscValues} indexed by the given index.
 * 
 * @param attrType[in] The datatype of the attribute
 * @param valueIdx[in] The index of the value
 * @return The corresponding value
 */
char *getValueByIndex(AttrType attrType, int valueIdx);

/**
 * Get the initial value of an attribute for a given user.
 * 
 * @param pInst[in] The ACoAC instance
 * @param userIdx[in] The index of the user
 * @param attrIdx[in] The index of the attribute
 * @return The index of the initial value of the attribute 
 */
int getInitValue(ACoACInstance *pInst, int userIdx, int attrIdx);

/**
 * Add a user-attribute-value (UAV) to the initial state @{pTableInitState} of the ACoAC instance.
 * The attribute domain @{pMapAttr2Dom} is updated accordingly.
 * 
 * @param pInst[in] The ACoAC instance
 * @param user[in] The user name
 * @param attr[in] The attribute name
 * @param value[in] The value
 * @return 0 if the UAV is successfully added, error code otherwise
 */
int addUAV(ACoACInstance *pInst, char *user, char *attr, char *value);

/**
 * Add a user-attribute-value (UAV) By Index to the initial state @{pTableInitState} of the ACoAC instance.
 * The attribute domain @{pMapAttr2Dom} is updated accordingly.
 * 
 * @param pInst[in] The ACoAC instance
 * @param userIdx[in] The index of the user
 * @param attrIdx[in] The index of the attribute
 * @param valueIdx[in] The index of the value
 * @return 0 if the UAV is successfully added, error code otherwise
 */
int addUAVByIdx(ACoACInstance *pInst, int userIdx, int attrIdx, int valueIdx);

/**
 * Add a rule index to the list of rule indices @{pSetRuleIdxes} of the ACoAC instance.
 * The attribute domain @{pMapAttr2Dom}, tables @{pTableTargetAV2Rule} and @{pTablePrecond2Rule} are updated accordingly.
 * 
 * @param pInst[in] The ACoAC instance
 * @param ruleIdx[in] The index of the rule
 * @return 1 if the rule is successfully added, 0 if the rule already exists
 */
int addRule(ACoACInstance *pInst, int ruleIdx);

/**
 * Initialize the ACoAC instance.
 * 
 * @param pInst[in] The ACoAC instance
 */
void init(ACoACInstance *pInst);

/**
 * Convert a rule to a string. Used for printing the rule.
 * 
 * @param ppRule[in] A pointer to the rule
 * @return A string representation of the rule
 */
char *RuleToString(void *ppRule);

#endif // _ACoACData_H