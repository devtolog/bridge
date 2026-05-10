#include <string.h>
#include <gc.h>

char* bridge_concat(char* s1, char* s2) {
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* res = (char*)GC_MALLOC(len1 + len2 + 1);
    strcpy(res, s1);
    strcat(res, s2);
    return res;
}

char* bridge_repeat(char* s, int64_t times) {
    if (times <= 0) return (char*)GC_MALLOC(1); // 空文字を返す
    size_t len = strlen(s);
    char* res = (char*)GC_MALLOC(len * times + 1);
    res[0] = '\0';
    for (int64_t i = 0; i < times; i++) {
        strcat(res, s);
    }
    return res;
}