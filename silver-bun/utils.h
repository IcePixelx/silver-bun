#pragma once

#ifndef USE_PRECOMPILED_HEADERS
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>
#else
#pragma message("ADD PRECOMPILED HEADERS TO SILVER-BUN.")
#endif // !USE_PRECOMPILED_HEADERS

namespace Utils
{
    std::vector<int> PatternToBytes(const std::string& svInput);
    std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(const std::string& svInput);
    std::vector<int> StringToBytes(const std::string& svInput, bool bNullTerminator);
    std::pair<std::vector<uint8_t>, std::string> StringToMaskedBytes(const std::string& svInput, bool bNullTerminator);
}

typedef const unsigned char* rsig_t;