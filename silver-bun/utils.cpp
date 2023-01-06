#include "utils.h"

namespace Utils
{
    //----------------------------------------------------------------------------------------
    // Purpose: For converting a string pattern with wildcards to an array of bytes.
    //----------------------------------------------------------------------------------------
    std::vector<int> PatternToBytes(const std::string& svInput)
    {
        char* pszPatternStart = const_cast<char*>(svInput.c_str());
        char* pszPatternEnd = pszPatternStart + strlen(svInput.c_str());
        std::vector<int> vBytes = std::vector<int>{ };

        for (char* pszCurrentByte = pszPatternStart; pszCurrentByte < pszPatternEnd; ++pszCurrentByte)
        {
            if (*pszCurrentByte == '?')
            {
                ++pszCurrentByte;
                if (*pszCurrentByte == '?')
                {
                    ++pszCurrentByte; // Skip double wildcard.
                }
                vBytes.push_back(-1); // Push the byte back as invalid.
            }
            else
            {
                vBytes.push_back(strtoul(pszCurrentByte, &pszCurrentByte, 16));
            }
        }
        return vBytes;
    };

    //----------------------------------------------------------------------------------------
    // Purpose: For converting a string pattern with wildcards to an array of bytes and mask.
    //----------------------------------------------------------------------------------------
    std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(const std::string& svInput)
    {
        char* pszPatternStart = const_cast<char*>(svInput.c_str());
        char* pszPatternEnd = pszPatternStart + strlen(svInput.c_str());
        std::vector<uint8_t> vBytes = std::vector<uint8_t>{ };
        std::string svMask = std::string();

        for (char* pszCurrentByte = pszPatternStart; pszCurrentByte < pszPatternEnd; ++pszCurrentByte)
        {
            if (*pszCurrentByte == '?')
            {
                ++pszCurrentByte;
                if (*pszCurrentByte == '?')
                {
                    ++pszCurrentByte; // Skip double wildcard.
                }
                vBytes.push_back(0); // Push the byte back as invalid.
                svMask.append("?");
            }
            else
            {
                vBytes.push_back(strtoul(pszCurrentByte, &pszCurrentByte, 16));
                svMask.append("x");
            }
        }
        return make_pair(vBytes, svMask);
    };

    //----------------------------------------------------------------------------------------
    // Purpose: For converting a string to an array of bytes.
    //----------------------------------------------------------------------------------------
    std::vector<int> StringToBytes(const std::string& svInput, bool bNullTerminator)
    {
        char* pszStringStart = const_cast<char*>(svInput.c_str());
        char* pszStringEnd = pszStringStart + strlen(svInput.c_str());
        std::vector<int> vBytes = std::vector<int>{ };

        for (char* pszCurrentByte = pszStringStart; pszCurrentByte < pszStringEnd; ++pszCurrentByte)
        {
            // Dereference character and push back the byte.
            vBytes.push_back(*pszCurrentByte);
        }

        if (bNullTerminator)
        {
            vBytes.push_back(0x0);
        }
        return vBytes;
    };

    //----------------------------------------------------------------------------------------
    // Purpose: For converting a string to an array of masked bytes.
    //----------------------------------------------------------------------------------------
    std::pair<std::vector<uint8_t>, std::string> StringToMaskedBytes(const std::string& svInput, bool bNullTerminator)
    {
        char* pszStringStart = const_cast<char*>(svInput.c_str());
        char* pszStringEnd = pszStringStart + strlen(svInput.c_str());
        std::vector<uint8_t> vBytes = std::vector<uint8_t>{ };
        std::string svMask = std::string();

        for (char* pszCurrentByte = pszStringStart; pszCurrentByte < pszStringEnd; ++pszCurrentByte)
        {
            // Dereference character and push back the byte.
            vBytes.push_back(*pszCurrentByte);
            svMask.append("x");
        }

        if (bNullTerminator)
        {
            vBytes.push_back(0x0);
            svMask.append("x");
        }
        return make_pair(vBytes, svMask);
    };
}