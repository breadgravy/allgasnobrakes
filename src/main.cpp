#include <cstring>
#include <cstdio>

//#include "slre.h"
#include "Str.h"

int main() {
    int a = 10;
    Str16 endstr("end!");
    printf("start!\n");
    printf("%s\n",endstr.c_str());
    return a;
}
