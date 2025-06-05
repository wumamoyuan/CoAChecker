#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "acoac_utils.h"

void logACoAC(const char *func, int line, int logType, LogLevel logLevel, const char *format, ...) {
    char *logLevelStr = NULL;
    switch (logLevel) {
    case DEBUG:
        logLevelStr = "DEBUG";
        break;
    case INFO:
        logLevelStr = "INFO";
        break;
    case WARNING:
        logLevelStr = "WARNING";
        break;
    case ERROR:
        logLevelStr = "ERROR";
        break;
    default:
        logLevelStr = "UNKNOWN";
        break;
    }
    if(logLevel < INFO){
        return;
    }

    va_list arg;
    va_start(arg, format);
    time_t time_log;
    if (logType == 0) {
        time_log = time(NULL);
        struct tm *tm_log = localtime(&time_log);
        /*printf("%04d-%02d-%02d %02d:%02d:%02d INFO [%s](%d):  ", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday,
            tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec, func, line);*/
        printf("\033[47;31m%02d/%02d/%02d %02d:%02d:%02d [%s][%s(%d)]: \033[0m", tm_log->tm_year - 100, tm_log->tm_mon + 1, tm_log->tm_mday,
               tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec, logLevelStr, func, line);
        vprintf(format, arg);
        return;
    }
    FILE *p = fopen("log.txt", "a+");
    if (p == NULL) {
        return;
    }
    time_log = time(NULL);
    struct tm *tm_log = localtime(&time_log);
    fprintf(p, "%04d-%02d-%02d %02d:%02d:%02d %s [%s](%d):", tm_log->tm_year + 1900, tm_log->tm_mon + 1, tm_log->tm_mday,
            tm_log->tm_hour, tm_log->tm_min, tm_log->tm_sec, func, line);
    vfprintf(p, format, arg);
    va_end(arg);
    fflush(p);
    fclose(p);
}

/****************************************************************************************************
 * 功能：添加一个元素到数组中。
 *      如果数组未初始化，则初始化数组，初始容量为10；
 *      如果数组已满，则扩展数组容量为原来的两倍；
 *      如果数组未满，则直接添加元素，并更新数组大小。
 * 参数：
 *      @array[in]: 数组指针
 *      @arrayCapacity[in]: 数组容量指针
 *      @value[in]: 要添加的值
 *      @arraySize[in]: 数组大小指针
 * 返回值：
 *      无
 ****************************************************************************************************/
void add_into_array(char ***array, int *arrayCapacity, char *value, int *arraySize) {
    // Check if the array is full
    if (*arraySize >= *arrayCapacity) {
        // Double the capacity of the array
        *arrayCapacity *= 2;
        *array = (char **)realloc(*array, *arrayCapacity * sizeof(char *));
    }
    // Allocate memory for the new value and copy it into the array
    (*array)[*arraySize] = (char *)malloc((strlen(value) + 1) * sizeof(char));
    strcpy((*array)[*arraySize], value);
    (*arraySize)++;
}

/****************************************************************************************************
 * 功能：去除字符串首尾的空格。
 * 参数：
 *      @str[in]: 要处理的字符串
 * 返回值：
 *      去除空格后的字符串指针
 ****************************************************************************************************/
char *strtrim(char *str) {
    char *p = str;
    char *p1;
    if (p) {
        p1 = p + strlen(str) - 1;
        while (*p && isspace(*p))
            p++;
        while (p1 > p && isspace(*p1))
            *p1-- = '\0';
    }
    return p;
}

/****************************************************************************************************
 * 功能：字符串hash函数，输入一个字符串的指针和长度，返回hash值
 *       例：原条件为level:{3,6,9}，值域为level:{2,3,6}，那么更新后的条件为{3,6}
 *       注意：当某个属性的取值集合与值域相同时，说明该属性不会再影响此规则的生效条件，可以从条件中删除
 * 参数：
 *       key[in] 指向字符串首地址的指针
 * 返回值：
 *       字符串的hash值
 ***************************************************************************************************/
unsigned int StringHashCode(void *pStr) {
    // printf("测试StringHashCode函数1\n");
    if (pStr == NULL) {
        printf("[Error] StringHashCode: key is NULL\n");
        abort();
    }
    char *str = *(char **)pStr;
    if (str == NULL) {
        printf("[Error] StringHashCode: string is NULL\n");
        abort();
    }
    unsigned int hash = 0;
    while (*str) {
        hash = 31 * hash + *str;
        str++;
    }
    return hash;
}

int StringEqual(void *pStr1, void *pStr2) {
    // printf("测试StringEqual函数\n");
    if (pStr1 == NULL || pStr2 == NULL) {
        printf("[Error] StringEqual: key is NULL\n");
        abort();
    }
    char *str1 = *(char **)pStr1;
    char *str2 = *(char **)pStr2;
    if (str1 == NULL || str2 == NULL) {
        printf("[Error] StringEqual: string is NULL\n");
        abort();
    }
    return strcmp(str1, str2) == 0;
}

unsigned int IntHashCode(void *pInt) {
    if (pInt == NULL) {
        printf("[Error] StringHashCode: key is NULL\n");
        abort();
    }
    return *(unsigned int *)pInt;
}

int IntEqual(void *pInt1, void *pInt2) {
    if (pInt1 == NULL || pInt2 == NULL) {
        printf("[Error] IntEqual: key is NULL\n");
        abort();
    }
    return *(int *)pInt1 == *(int *)pInt2;
}

char *IntToString(void *pInt) {
    int i = *(int *)pInt;
    char *str = (char *)malloc(32);
    sprintf(str, "%d", i);
    return str;
}

char *StringToString(void *pStr) {
    char *str = *(char **)pStr;
    if (str == NULL) {
        str = "NULL";
    }
    return strdup(str);
}

char *mapToString(HashMap *map, char *(*keyToString)(void *key), char *(*valueToString)(void *value)) {
    HashNodeIterator *it = iHashMap.NewIterator(map);
    HashNode *node;
    char *key, *value;
    char *ret = (char *)malloc(3);
    int cur = 0;
    ret[cur++] = '{';
    int first = 1;
    int keyLen, valueLen;
    while (it->HasNext(it)) {
        node = it->GetNext(it);
        key = keyToString(node->key);
        value = valueToString(node->value);
        keyLen = strlen(key);
        valueLen = strlen(value);
        ret = (char *)realloc(ret, cur + keyLen + valueLen + 5);
        if (first) {
            first = 0;
        } else {
            ret[cur++] = ',';
            ret[cur++] = ' ';
        }
        memcpy(ret + cur, key, keyLen);
        cur += keyLen;
        ret[cur++] = '=';
        memcpy(ret + cur, value, valueLen);
        cur += valueLen;
        free(key);
        free(value);
    }
    ret[cur++] = '}';
    ret[cur++] = '\0';
    iHashMap.DeleteIterator(it);
    return ret;
}

/*char *vectorToString(Dictionary *dict, char *(*valueToString)(void *value))
{
    strCollection *keys = iDictionary.GetKeys(dict);
    Iterator *it = istrCollection.NewIterator(keys);
    char *key, *value;
    char *ret = (char *) malloc(3);
    int cur = 0;
    ret[cur++] = '{';
    int first = 1;
    int keyLen, valueLen, retLen = 1, newRetLen;
    for (key = it->GetFirst(it); key != NULL; key = it->GetNext(it))
    {
        value = valueToString(iDictionary.GetElement(dict, key));
        keyLen = strlen(key);
        valueLen = strlen(value);
        ret = (char *) realloc(ret, cur + keyLen + valueLen + 5);
        if (first)
        {
            first = 0;
        }
        else
        {
            ret[cur++] = ',';
            ret[cur++] = ' ';
        }
        memcpy(ret + cur, key, keyLen);
        cur += keyLen;
        ret[cur++] = '=';
        memcpy(ret + cur, value, valueLen);
        cur += valueLen;
        free(value);
    }
    ret[cur++] = '}';
    ret[cur++] = '\0';
    istrCollection.deleteIterator(it);
    istrCollection.Finalize(keys);
    return ret;
}*/