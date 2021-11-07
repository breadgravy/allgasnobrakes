#include "slre.h"
#include <cstring>
#include <string>

bool re_match(const char* str, const char* regex) {
    return slre_match(regex, str, strlen(str), nullptr, 0, 0) >= 0;
}
bool re_match(const std::string& str, const char* regex) {
    return re_match(str.c_str(), regex);
}
