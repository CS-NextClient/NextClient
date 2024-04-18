//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once

#include <vector>
#include "rapidjson/document.h"
#include "GameAnalytics.h"
#if USE_UWP
#include <string>
#include <locale>
#include <codecvt>
#include <exception>
#include "GALogger.h"
#endif

namespace gameanalytics
{
    namespace utilities
    {
        class GAUtilities
        {
        public:
            static const char* getPathSeparator();
            static void generateUUID(char* out);
            static void hmacWithKey(const char* key, const std::vector<char>& data, char* out);
            static bool stringMatch(const char* string, const char* pattern);
            static std::vector<char> gzipCompress(const char* data);

            // added for C++ port
            static bool isStringNullOrEmpty(const char* s);
            static void uppercaseString(char* s);
            static void lowercaseString(char* s);
            static bool stringVectorContainsString(const StringVector& vector, const char* search);
            static int64_t timeIntervalSince1970();
            static void printJoinStringArray(const StringVector& v, const char* format, const char* delimiter = ", ");
#if !USE_UWP
            static int base64_needed_encoded_length(int length_of_data);
            static void base64_encode(const unsigned char * src, int src_len, unsigned char *buf_);
#endif

#if USE_UWP
            inline static std::string ws2s(const std::wstring wstr)
            {
                try
                {
                    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(wstr);
                }
                catch(const std::exception& e)
                {
                    logging::GALogger::d("Error with ws2s: %S", wstr.c_str());
                    return "";
                }
            }

            inline static std::wstring s2ws(const std::string str)
            {
                try
                {
                    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
                }
                catch(const std::exception& e)
                {
                    logging::GALogger::d("Error with s2ws: %s", str.c_str());
                    return L"";
                }
            }
#endif
        private:
            static char pathSeparator[];
        };
    }
}
