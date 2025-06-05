#include "acoac_boundcal.h"
#include "acoac_utils.h"

/**
 * Estimate a loose bound for an ACoAC instance.
 * It equals to the product of the domain sizes of all attributes.
 *
 * @param pInst[in]: An ACoAC instance
 * @return The loose bound of the ACoAC instance
 */
static BigInteger computeLooseBound(ACoACInstance *pInst) {
    BigInteger bound = iBigInteger.createFromInt(1);
    BigInteger tmp;
    HashNodeIterator *it = iHashMap.NewIterator(pInst->pMapAttr2Dom);
    char *attr;
    int domSize;
    while (it->HasNext(it)) {
        HashNode *pNode = it->GetNext(it);
        attr = istrCollection.GetElement(pscAttrs, *(int *)pNode->key);
        if (strcmp(attr, "Admin") == 0) {
            continue;
        }
        domSize = iHashSet.Size(*(HashSet **)pNode->value);
        tmp = iBigInteger.multiplyByInt(bound, domSize);
        iBigInteger.finalize(bound);
        bound = tmp;
    }
    iHashMap.DeleteIterator(it);
    return bound;
}

// /**
//  * attr'是目标属性，attr''是其它属性
//  * |Dom(attr''1)|*...*|Dom(attr''m)|*(|Dom(attr'1)|*...*|Dom(attr'n)| - 1)
//  * @param pInst[in]: 待翻译的ACoAC实例
//  * @return
//  */
// static BigInteger compute2(ACoACInstance *pInst) {
//     BigInteger boundPart1 = iBigInteger.createFromInt(1);
//     BigInteger boundPart2 = iBigInteger.createFromInt(1);
//     BigInteger tmp;
//     int domSize;
//     char *attr;
//     HashNode *pNode;
//     HashNodeIterator *it = iHashMap.NewIterator(pInst->pMapAttr2Dom);
//     while (it->HasNext(it)) {
//         pNode = it->GetNext(it);
//         attr = istrCollection.GetElement(pscAttrs, *(int *)pNode->key);
//         if (strcmp(attr, "Admin") == 0) {
//             continue;
//         }
//         domSize = iHashSet.Size(*(HashSet **)pNode->value);
//         if (!iHashMap.ContainsKey(pInst->pmapQueryAVs, pNode->key)) {
//             tmp = iBigInteger.multiplyByInt(boundPart1, domSize);
//             iBigInteger.finalize(boundPart1);
//             boundPart1 = tmp;
//         } else {
//             tmp = iBigInteger.multiplyByInt(boundPart2, domSize);
//             iBigInteger.finalize(boundPart2);
//             boundPart2 = tmp;
//         }
//     }
//     iHashMap.DeleteIterator(it);

//     tmp = iBigInteger.subtractByInt(boundPart2, 1);
//     BigInteger bound = iBigInteger.multiply(boundPart1, tmp);
//     iBigInteger.finalize(boundPart1);
//     iBigInteger.finalize(boundPart2);
//     iBigInteger.finalize(tmp);
//     return bound;
// }

/**
 * Use binary search to insert an integer into a descendingly sorted array, and keep the array descending.
 *
 * @param array[in]: The array to insert the integer into
 * @param len[in]: The length of the array
 * @param n[in]: The integer to insert
 */
void insertDesc(int *array, int len, int n) {
    int left = 0, right = len - 1, mid;
    while (left <= right) {
        mid = (left + right) / 2;
        if (array[mid] > n) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    int i;
    for (i = len; i > left; i--) {
        array[i] = array[i - 1];
    }
    array[left] = n;
}

/**
 * Estimate a tight bound for an ACoAC instance.
 *
 * Let A_1 be a set of attributes, where a belongs to A_1 if and only if there is no rule with (a,InitUAV(u_t, a)) as the target attribute value.
 * Let A be the set of all attributes, and A_2=A\A_1.
 * Let the attributes in A_1 be sorted in descending order of their domain sizes as a_11,a_12,...,a_1m.
 * Let the attributes in A_2 be a_21,a_22,...,a_2n.
 * Then the upper bound is less than or equal to |Dom(a_21)|*|Dom(a_22)|*...|Dom(a_2n)|*P,
 * where P=1+(|Dom(a_11)|-1)+(|Dom(a_11)|-1)*(|Dom(a_12)|-1)+...+(|Dom(a_11)|-1)*...*(|Dom(a_1m)|-1).
 *
 * @param pInst[in]: An ACoAC instance
 * @return The tight bound of the ACoAC instance
 */
static BigInteger computeTightBound(ACoACInstance *pInst) {
    HashMap *pMapInitAVs = iHashBasedTable.GetRow(pInst->pTableInitState, &pInst->queryUserIdx);
    int attrNum = iHashMap.Size(pInst->pMapAttr2Dom);
    int attrs1[attrNum], attrs4[attrNum], attrs1MinusQueryAttrs[attrNum], attrs4MinusQueryAttrs[attrNum];
    int attrs1Len = 0, attrs4Len = 0, attrs1MinusQueryAttrsLen = 0, attrs4MinusQueryAttrsLen = 0;

    int *pAttrIdx, *pInitValIdx, *pQueryValIdx, domSize;
    char *attr;
    HashNode *node;
    HashNodeIterator *it = iHashMap.NewIterator(pInst->pMapAttr2Dom);
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        pAttrIdx = (int *)node->key;
        attr = istrCollection.GetElement(pscAttrs, *pAttrIdx);
        if (strcmp(attr, "Admin") == 0) {
            continue;
        }
        domSize = iHashSet.Size(*(HashSet **)node->value);
        pInitValIdx = (int *)iHashMap.Get(pMapInitAVs, pAttrIdx);
        pQueryValIdx = (int *)iHashMap.Get(pInst->pmapQueryAVs, pAttrIdx);
        if (pInitValIdx == NULL || iHashBasedTable.Get(pInst->pTableTargetAV2Rule, pAttrIdx, pInitValIdx) == NULL) {
            // attr is non restorable, i.e., once the initial value is modified, it cannot be restored
            if (pQueryValIdx == NULL) {
                // attr is not a target attribute of the query, so it is not already satisfied
                attrs4MinusQueryAttrs[attrs4MinusQueryAttrsLen++] = domSize;
                insertDesc(attrs4, attrs4Len++, domSize);
            } else if (pInitValIdx == NULL || *pQueryValIdx != *pInitValIdx) {
                // attr is a target attribute of the query, but the initial value is not equal to the query value, so it is not already satisfied
                insertDesc(attrs4, attrs4Len++, domSize);
            }
        } else {
            if (pQueryValIdx == NULL) {
                attrs1MinusQueryAttrs[attrs1MinusQueryAttrsLen++] = domSize;
            }
            // attr is restorable, i.e., once the initial value is modified, it can be restored
            attrs1[attrs1Len++] = domSize;
        }
    }
    iHashMap.DeleteIterator(it);
    // Calculate P=1+(|Dom(a_11)|-1)+(|Dom(a_11)|-1)*(|Dom(a_12)|-1)+...+(|Dom(a_11)|-1)*...*(|Dom(a_1m)|-1)
    BigInteger boundPart1 = iBigInteger.createFromInt(1);
    BigInteger product = iBigInteger.createFromInt(1);
    BigInteger tmp;
    int i;
    for (i = 0; i < attrs4Len; i++) {
        tmp = iBigInteger.multiplyByInt(product, attrs4[i] - 1);
        iBigInteger.finalize(product);
        product = tmp;

        tmp = iBigInteger.add(boundPart1, product);
        iBigInteger.finalize(boundPart1);
        boundPart1 = tmp;
    }
    iBigInteger.finalize(product);

    BigInteger boundPart2 = iBigInteger.createFromInt(1);
    for (i = 0; i < attrs1Len; i++) {
        tmp = iBigInteger.multiplyByInt(boundPart2, attrs1[i]);
        iBigInteger.finalize(boundPart2);
        boundPart2 = tmp;
    }

    BigInteger boundMinus = iBigInteger.createFromInt(1);
    for (i = 0; i < attrs1MinusQueryAttrsLen; i++) {
        tmp = iBigInteger.multiplyByInt(boundMinus, attrs1MinusQueryAttrs[i]);
        iBigInteger.finalize(boundMinus);
        boundMinus = tmp;
    }
    for (i = 0; i < attrs4MinusQueryAttrsLen; i++) {
        tmp = iBigInteger.multiplyByInt(boundMinus, attrs4MinusQueryAttrs[i] - 1);
        iBigInteger.finalize(boundMinus);
        boundMinus = tmp;
    }

    tmp = iBigInteger.multiply(boundPart1, boundPart2);
    BigInteger bound = iBigInteger.subtract(tmp, boundMinus);
    iBigInteger.finalize(boundPart1);
    iBigInteger.finalize(boundPart2);
    iBigInteger.finalize(boundMinus);
    iBigInteger.finalize(tmp);

    return bound;
}

BigInteger computeBound(ACoACInstance *pInst, int tl) {
    switch (tl) {
    case 1:
        return computeLooseBound(pInst);
    case 2:
        return computeTightBound(pInst);
    default:
        return ZERO;
    }
}