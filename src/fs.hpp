#pragma once

#include <string>

#include "err.hpp"

#define MAX_FILE_SIZE 200000

void printDiv(const char* str);

bool file_exists(std::string& filepath);
bool file_exists(const char* filepath);

size_t get_filesize(const char* filepath);
std::string get_abspath(const char* relpath);

[[nodiscard]] ErrCode slurpFile(char* filepath, char* contents);
void dumpSourceListing(char* source_buf);
