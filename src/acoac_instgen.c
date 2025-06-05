#include "acoac_io.h"
#include "acoac_utils.h"

#include <getopt.h>
#include <time.h>

#define STRING_ATTRIBUTE_PREFIX "s"
#define NUMBER_ATTRIBUTE_PREFIX "n"
#define BOOLEAN_ATTRIBUTE_PREFIX "b"

typedef struct {
    int nUsers;
    int nAttrs;
    int nAttrsForType[3];
    int maxDomSize;
    int *domSize;
    double initAVDefaultRate;
    int nRules;
    int minAtomConds;
    int maxAtomConds;
    int *nAtomConds;
    int nQueryAV;
} GenParam;

static GenParam createGenParam(int nUsers, int nStrAttrs, int nNumAttrs, int nBoolAttrs, int maxDomSize,
                               double initAVDefaultRate, int nRules, int minAtomConds, int maxAtomConds, int nQueryAV) {
    GenParam genParam;
    genParam.nUsers = nUsers;

    genParam.nAttrs = nStrAttrs + nNumAttrs + nBoolAttrs;
    genParam.nAttrsForType[STRING] = nStrAttrs;
    genParam.nAttrsForType[INTEGER] = nNumAttrs;
    genParam.nAttrsForType[BOOLEAN] = nBoolAttrs;

    genParam.maxDomSize = maxDomSize;
    genParam.domSize = (int *)malloc(sizeof(int) * (1 + nStrAttrs + nNumAttrs + nBoolAttrs));

    genParam.initAVDefaultRate = initAVDefaultRate;
    genParam.nRules = nRules;

    genParam.minAtomConds = minAtomConds;
    genParam.maxAtomConds = maxAtomConds;
    genParam.nAtomConds = (int *)malloc(sizeof(int) * nRules);
    int i;
    for (i = 0; i < nRules; i++) {
        genParam.nAtomConds[i] = rand() % (maxAtomConds - minAtomConds + 1) + minAtomConds;
    }

    genParam.nQueryAV = nQueryAV;
    return genParam;
}

static char *getAttrName(int i) {
    return "Attr";
}

static void genValForAttr(GenParam *genParam, int attrIdx, char *value) {
    AttrType attrType = getAttrTypeByIdx(attrIdx);
    int n = rand() % genParam->domSize[attrIdx];
    char *attrName;
    switch (attrType) {
    case STRING:
        attrName = istrCollection.GetElement(pscAttrs, attrIdx);
        sprintf(value, "%s_%d", attrName, n);
        break;
    case INTEGER:
        sprintf(value, "%d", n);
        break;
    case BOOLEAN:
        strcpy(value, rand() % 2 == 0 ? "true" : "false");
        break;
    }
}

typedef struct _LinkedListNode {
    int n;
    struct _LinkedListNode *next;
} LinkedListNode, *LinkedList;

/**
 * Select n random integers from [0, m)
 *
 * @param m[in] The range of the integers
 * @param n[in] The number of integers to select
 * @param selected[out] The selected integers
 */
static void selectFrom(int m, int n, int *selected) {
    if (m < n) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Invalid parameters: the number of integers to select (%d) is greater than the range (%d)\n", n, m);
        exit(1);
    }
    if (n < 0) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Invalid parameters: the number of integers to select (%d) is less than 0\n", n);
        exit(1);
    }

    LinkedList pList = (LinkedList)malloc(sizeof(LinkedListNode));
    pList->next = NULL;
    LinkedListNode *pTail = pList;
    for (int i = 0; i < m; i++) {
        LinkedListNode *pNode = (LinkedListNode *)malloc(sizeof(LinkedListNode));
        pNode->n = i;
        pNode->next = NULL;
        pTail->next = pNode;
        pTail = pNode;
    }

    int selectedIndex;
    LinkedListNode *p, *q;
    for (int i = 0; i < n; i++) {
        selectedIndex = rand() % (m - i);
        p = pList;
        for (int j = 0; j < selectedIndex; j++) {
            p = p->next;
        }
        q = p->next;
        selected[i] = q->n;
        p->next = q->next;
        free(q);
    }

    p = pList;
    while (p != NULL) {
        q = p;
        p = p->next;
        free(q);
    }
}

static Rule *genRule(GenParam *genParam, int nAtomConds) {
    // 1. Generate admin condition (Admin=true)
    HashSet *adminCond = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
    AtomCondition atomCond = {.attribute = 0, .value = 1, .op = EQUAL};
    iHashSet.Add(adminCond, &atomCond);

    // 2. Generate user condition
    HashSet *userCond = iHashSet.Create(sizeof(AtomCondition), iAtomCondition.HashCode, iAtomCondition.Equal);
    int j, attrIdx, valIdx, ret;
    AttrType attrType;
    comparisonOperator op;
    char val[20];
    for (j = 0; j < nAtomConds; j++) {
        // 3.1 Randomly select an attribute
        attrIdx = rand() % genParam->nAttrs + 1;
        // 3.2 Generate a value for the attribute
        genValForAttr(genParam, attrIdx, val);
        attrType = getAttrTypeByIdx(attrIdx);
        ret = getValueIndex(attrType, val, &valIdx);
        if (ret) {
            logACoAC(__func__, __LINE__, 0, ERROR, "Failed to generate a value for the attribute: %s, %s\n", istrCollection.GetElement(pscAttrs, attrIdx), val);
            exit(ret);
        }
        // 3.3 If the attribute is integer, generate a random operator, otherwise set the operator to EQUAL
        op = attrType == INTEGER ? (comparisonOperator)(rand() % 6) : EQUAL;

        atomCond = (AtomCondition){.attribute = attrIdx, .value = valIdx, .op = op};
        iHashSet.Add(userCond, &atomCond);
    }

    // 4. Randomly select a target attribute
    attrIdx = rand() % genParam->nAttrs + 1;
    char *targetAttr = istrCollection.GetElement(pscAttrs, attrIdx);

    // 5. Randomly select a target attribute value
    genValForAttr(genParam, attrIdx, val);
    ret = getValueIndex(getAttrTypeByIdx(attrIdx), val, &valIdx);
    if (ret) {
        logACoAC(__func__, __LINE__, 0, ERROR, "Failed to generate a value for the attribute: %s, %s\n", istrCollection.GetElement(pscAttrs, attrIdx), val);
        exit(ret);
    }

    return iRule.Create(adminCond, userCond, attrIdx, valIdx);
}

static ACoACInstance *generate(GenParam *genParam) {
    initGlobalVars();
    ACoACInstance *pInst = createACoACInstance();

    int i;

    // Generate users u0, u1, u2, etc.
    char username[20];
    for (i = 0; i < genParam->nUsers; ++i) {
        sprintf(username, "u%d", i);
        istrCollection.Add(pscUsers, username);
        iDictionary.Insert(pdictUser2Index, username, &i);
        iVector.Add(pInst->pVecUserIndices, &i);
    }

    char attrNamePrefix[3];
    attrNamePrefix[STRING] = 's';
    attrNamePrefix[INTEGER] = 'n';
    attrNamePrefix[BOOLEAN] = 'b';

    // Generate attributes
    char attrName[20];
    int attrIdx = 0;
    AttrType attrType = BOOLEAN;
    int defValIdx = 0;

    // The first attribute is Admin, a boolean attribute with default value "false"
    istrCollection.Add(pscAttrs, "Admin");
    iDictionary.Insert(pdictAttr2Index, "Admin", &attrIdx);
    iHashMap.Put(pmapAttr2Type, &attrIdx, &attrType);
    iHashMap.Put(pmapAttr2DefVal, &attrIdx, &defValIdx);
    genParam->domSize[attrIdx] = 2;
    attrIdx++;

    // The first string value is ""
    int stringValCnt = 0;
    istrCollection.Add(pscValues, "");
    iDictionary.Insert(pdictValue2Index, "", &stringValCnt);
    stringValCnt++;

    // Generate attributes for each data type
    char attrVal[22];
    for (attrType = 0; attrType < 3; ++attrType) {
        for (i = 0; i < genParam->nAttrsForType[attrType]; i++) {
            sprintf(attrName, "%c%d", attrNamePrefix[attrType], i);

            istrCollection.Add(pscAttrs, attrName);
            iDictionary.Insert(pdictAttr2Index, attrName, &attrIdx);
            iHashMap.Put(pmapAttr2Type, &attrIdx, &attrType);

            if (attrType == STRING) {
                sprintf(attrVal, "%s_0", attrName);
                istrCollection.Add(pscValues, attrVal);
                iDictionary.Insert(pdictValue2Index, attrVal, &stringValCnt);
                iHashMap.Put(pmapAttr2DefVal, &attrIdx, &stringValCnt);
                stringValCnt++;
            } else {
                iHashMap.Put(pmapAttr2DefVal, &attrIdx, &defValIdx);
            }

            genParam->domSize[attrIdx] = attrType == BOOLEAN ? 2 : rand() % genParam->maxDomSize + 1;
            attrIdx++;
        }
    }

    // Generate initial attribute values for each user
    int userIdx;
    for (userIdx = 0; userIdx < genParam->nUsers; userIdx++) {
        sprintf(username, "u%d", userIdx);
        for (attrIdx = 1; attrIdx <= genParam->nAttrs; attrIdx++) {
            // Generate a random number between 0 and 1.
            // If it is less than nInitAVDefaultRate, use default value, otherwise generate a random value.
            int useDefaultVal = (rand() / (double)RAND_MAX) < genParam->initAVDefaultRate;
            if (!useDefaultVal) {
                genValForAttr(genParam, attrIdx, attrVal);
                addUAV(pInst, username, istrCollection.GetElement(pscAttrs, attrIdx), attrVal);
            }
        }
    }
    // Set the first user as the admin
    addUAV(pInst, "u0", "Admin", "true");

    // Generate rules
    Rule *r;
    for (i = 0; i < genParam->nRules; i++) {
        r = genRule(genParam, genParam->nAtomConds[i]);
        iVector.Add(pVecRules, r);
        free(r);
        addRule(pInst, i);
    }

    // Set the last user as the query user
    pInst->queryUserIdx = genParam->nUsers - 1;

    // Generate query attribute values
    // Select nQueryAV random attribute indexes from [0, nAttrs)
    int attrIndexes[genParam->nQueryAV];
    selectFrom(genParam->nAttrs, genParam->nQueryAV, attrIndexes);

    int valIdx;
    for (i = 0; i < genParam->nQueryAV; i++) {
        attrIdx = attrIndexes[i] + 1;
        // Generate a random value for the attribute
        genValForAttr(genParam, attrIdx, attrVal);
        int ret = getValueIndex(getAttrTypeByIdx(attrIdx), attrVal, &valIdx);
        if (ret) {
            logACoAC(__func__, __LINE__, 0, ERROR, "Failed to handle query av: %s, %s\n", istrCollection.GetElement(pscAttrs, attrIdx), attrVal);
            exit(ret);
        }
        iHashMap.Put(pInst->pmapQueryAVs, &attrIdx, &valIdx);
    }

    return pInst;
}

int main(int argc, char **argv) {
    char *usage = "Usage: ./acoac_instgen\n"
                  "  -u <number of users>\n"
                  "  -b <number of boolean attributes>\n"
                  "  -n <number of integer attributes>\n"
                  "  -s <number of string attributes>\n"
                  "  -d <domain size>\n"
                  "  -i <initial attribute value default rate>\n"
                  "  -r <number of rules>\n"
                  "  -l <minimum number of atom conditions per rule>\n"
                  "  -h <maximum number of atom conditions per rule>\n"
                  "  -q <number of query attributes>\n"
                  "  -o <output file>\n";

    int nUsers, nBoolAttrs, nIntAttrs, nStrAttrs, maxDomSize, initAVDefaultRate, nRules, minAtomConds, maxAtomConds, nQueryAV;
    char *outputFilePath;

    int opt;
    while ((opt = getopt(argc, argv, "u:b:n:s:d:i:r:l:h:q:o:")) != -1) {
        switch (opt) {
        case 'u':
            nUsers = atoi(optarg);
            break;
        case 'b':
            nBoolAttrs = atoi(optarg);
            break;
        case 'n':
            nIntAttrs = atoi(optarg);
            break;
        case 's':
            nStrAttrs = atoi(optarg);
            break;
        case 'd':
            maxDomSize = atoi(optarg);
            break;
        case 'i':
            initAVDefaultRate = atof(optarg);
            break;
        case 'r':
            nRules = atoi(optarg);
            break;
        case 'l':
            minAtomConds = atoi(optarg);
            break;
        case 'h':
            maxAtomConds = atoi(optarg);
            break;
        case 'q':
            nQueryAV = atoi(optarg);
            break;
        case 'o':
            outputFilePath = (char *)malloc(sizeof(char) * strlen(optarg));
            strcpy(outputFilePath, optarg);
            break;
        default:
            logACoAC(__func__, __LINE__, 0, ERROR, usage);
            exit(1);
        }
    }

    unsigned int seed = (unsigned int)time(NULL);
    seed += StringHashCode(&outputFilePath);
    srand(seed);
    GenParam genParam = createGenParam(nUsers, nStrAttrs, nIntAttrs, nBoolAttrs, maxDomSize, initAVDefaultRate, nRules, minAtomConds, maxAtomConds, nQueryAV);
    ACoACInstance *pInst = generate(&genParam);
    writeACoACInstance(pInst, outputFilePath);
    return 0;
}
