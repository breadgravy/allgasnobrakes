#pragma once

#include <cstdio>

void printDiv(const char* str) {
    static int phaseno = 1;
    printf("\n-----------------------------------------------------------------"
           "---------------\n");
    printf(" Phase %d : %s\n", phaseno++, str);
    printf("-------------------------------------------------------------------"
           "-------------\n");
}

#define ERR(...)                                                                                      \
    {                                                                                              \
        fprintf(stderr, RED __VA_ARGS__);                                                          \
        fprintf(stderr, RESET);                                                          \
        exit(1);                                                                                   \
    }
