#pragma once

#include <cstdio>
#include <string>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include "time.hpp"

void getRSS(){
    int pid = getpid();
    auto str = "cat /proc/" + std::to_string(pid) + "/status | grep VmRSS > /tmp/rss.out";
    system(str.c_str());
    std::cout << std::ifstream("/tmp/rss.out").rdbuf();
    system("rm /tmp/rss.out");
}

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
