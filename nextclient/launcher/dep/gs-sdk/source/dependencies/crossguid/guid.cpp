/*
The MIT License (MIT)

Copyright (c) 2014 Graeme Hill (http://graemehill.ca)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "guid.h"
#include <string.h>
#include <stdio.h>

#ifdef GUID_LIBUUID
#include <cstdlib>
#include <random>
#endif

#ifdef GUID_CFUUID
#include <CoreFoundation/CFUUID.h>
#endif

#ifdef GUID_WINDOWS
#include <objbase.h>
#endif

#ifdef GUID_ANDROID
#include <jni.h>
#endif

#include <iostream>

using namespace std;

// overload << so that it's easy to convert to a string
ostream &operator<<(ostream &s, const Guid &guid)
{
    return s << hex << setfill('0')
        << setw(2) << (int)guid._bytes[0]
        << setw(2) << (int)guid._bytes[1]
        << setw(2) << (int)guid._bytes[2]
        << setw(2) << (int)guid._bytes[3]
        << "-"
        << setw(2) << (int)guid._bytes[4]
        << setw(2) << (int)guid._bytes[5]
        << "-"
        << setw(2) << (int)guid._bytes[6]
        << setw(2) << (int)guid._bytes[7]
        << "-"
        << setw(2) << (int)guid._bytes[8]
        << setw(2) << (int)guid._bytes[9]
        << "-"
        << setw(2) << (int)guid._bytes[10]
        << setw(2) << (int)guid._bytes[11]
        << setw(2) << (int)guid._bytes[12]
        << setw(2) << (int)guid._bytes[13]
        << setw(2) << (int)guid._bytes[14]
        << setw(2) << (int)guid._bytes[15];
}

// create a guid from vector of bytes
Guid::Guid(const vector<unsigned char> &bytes)
{
    _bytes = bytes;
}

// create a guid from array of bytes
Guid::Guid(const unsigned char *bytes)
{
    _bytes.assign(bytes, bytes + 16);
}

// converts a single hex char to a number (0 - 15)
unsigned char hexDigitToChar(char ch)
{
    if (ch > 47 && ch < 58)
        return ch - 48;

    if (ch > 96 && ch < 103)
        return ch - 87;

    if (ch > 64 && ch < 71)
        return ch - 55;

    return 0;
}

// converts the two hexadecimal characters to an unsigned char (a byte)
unsigned char hexPairToChar(char a, char b)
{
    return hexDigitToChar(a) * 16 + hexDigitToChar(b);
}

// create a guid from string
Guid::Guid(const char* fromString)
{
    _bytes.clear();

    char charOne, charTwo;
    bool lookingForFirstChar = true;

    size_t size = strlen(fromString);

    for(size_t i = 0; i < size; ++i)
    {
        if (fromString[i] == '-')
            continue;

        if (lookingForFirstChar)
        {
            charOne = fromString[i];
            lookingForFirstChar = false;
        }
        else
        {
            charTwo = fromString[i];
            auto byte = hexPairToChar(charOne, charTwo);
            _bytes.push_back(byte);
            lookingForFirstChar = true;
        }
    }
}

// create empty guid
Guid::Guid()
{
    _bytes = vector<unsigned char>(16, 0);
}

// copy constructor
Guid::Guid(const Guid &other)
{
    _bytes = other._bytes;
}

// overload assignment operator
Guid &Guid::operator=(const Guid &other)
{
    _bytes = other._bytes;
    return *this;
}

// overload equality operator
bool Guid::operator==(const Guid &other) const
{
    return _bytes == other._bytes;
}

// overload inequality operator
bool Guid::operator!=(const Guid &other) const
{
    return !((*this) == other);
}

void Guid::to_string(char* out)
{
    snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        (int)_bytes[0], (int)_bytes[1], (int)_bytes[2], (int)_bytes[3],
        (int)_bytes[4], (int)_bytes[5], (int)_bytes[6], (int)_bytes[7],
        (int)_bytes[8], (int)_bytes[9], (int)_bytes[10], (int)_bytes[11],
        (int)_bytes[12], (int)_bytes[13], (int)_bytes[14], (int)_bytes[15]);
}

// This is the linux friendly implementation, but it could work on other
// systems that have libuuid available
#ifdef GUID_LIBUUID
unsigned char random_char()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0,255);
    return static_cast<unsigned char>(dis(gen));
}

void generate_hex(size_t len, std::vector<char>& out)
{
    for(size_t i = 0; i < len; ++i)
    {
        const auto rc = random_char();
        char s[3];
        snprintf(s, 3, "%02x", int(rc));
        
        out.push_back(s[0]);
        out.push_back(s[1]);
    }
}

Guid GuidGenerator::newGuid()
{
    std::vector<char> result;
    generate_hex(4, result);
    result.push_back('-');
    generate_hex(2, result);
    result.push_back('-');
    generate_hex(2, result);
    result.push_back('-');
    generate_hex(2, result);
    result.push_back('-');
    generate_hex(4, result);
    result.push_back('\0');

    return result.data();
}
#endif

// this is the mac and ios version
#ifdef GUID_CFUUID
Guid GuidGenerator::newGuid()
{
    auto newId = CFUUIDCreate(NULL);
    auto bytes = CFUUIDGetUUIDBytes(newId);
    CFRelease(newId);

    const unsigned char byteArray[16] =
    {
        bytes.byte0,
        bytes.byte1,
        bytes.byte2,
        bytes.byte3,
        bytes.byte4,
        bytes.byte5,
        bytes.byte6,
        bytes.byte7,
        bytes.byte8,
        bytes.byte9,
        bytes.byte10,
        bytes.byte11,
        bytes.byte12,
        bytes.byte13,
        bytes.byte14,
        bytes.byte15
    };
    return byteArray;
}
#endif

// obviously this is the windows version
#ifdef GUID_WINDOWS
Guid GuidGenerator::newGuid()
{
    GUID newId;
    CoCreateGuid(&newId);

    const unsigned char bytes[16] =
    {
        (newId.Data1 >> 24) & 0xFF,
        (newId.Data1 >> 16) & 0xFF,
        (newId.Data1 >> 8) & 0xFF,
        (newId.Data1) & 0xff,

        (newId.Data2 >> 8) & 0xFF,
        (newId.Data2) & 0xff,

        (newId.Data3 >> 8) & 0xFF,
        (newId.Data3) & 0xFF,

        newId.Data4[0],
        newId.Data4[1],
        newId.Data4[2],
        newId.Data4[3],
        newId.Data4[4],
        newId.Data4[5],
        newId.Data4[6],
        newId.Data4[7]
    };

    return bytes;
}
#endif

// android version that uses a call to a java api
#ifdef GUID_ANDROID
GuidGenerator::GuidGenerator(JNIEnv *env)
{
    _env = env;
    _uuidClass = env->FindClass("java/util/UUID");
    _newGuidMethod = env->GetStaticMethodID(_uuidClass, "randomUUID", "()Ljava/util/UUID;");
    _mostSignificantBitsMethod = env->GetMethodID(_uuidClass, "getMostSignificantBits", "()J");
    _leastSignificantBitsMethod = env->GetMethodID(_uuidClass, "getLeastSignificantBits", "()J");
}

Guid GuidGenerator::newGuid()
{
    jobject javaUuid = _env->CallStaticObjectMethod(_uuidClass, _newGuidMethod);
    jlong mostSignificant = _env->CallLongMethod(javaUuid, _mostSignificantBitsMethod);
    jlong leastSignificant = _env->CallLongMethod(javaUuid, _leastSignificantBitsMethod);

    unsigned char bytes[16] =
    {
        (mostSignificant >> 56) & 0xFF,
        (mostSignificant >> 48) & 0xFF,
        (mostSignificant >> 40) & 0xFF,
        (mostSignificant >> 32) & 0xFF,
        (mostSignificant >> 24) & 0xFF,
        (mostSignificant >> 16) & 0xFF,
        (mostSignificant >> 8) & 0xFF,
        (mostSignificant) & 0xFF,
        (leastSignificant >> 56) & 0xFF,
        (leastSignificant >> 48) & 0xFF,
        (leastSignificant >> 40) & 0xFF,
        (leastSignificant >> 32) & 0xFF,
        (leastSignificant >> 24) & 0xFF,
        (leastSignificant >> 16) & 0xFF,
        (leastSignificant >> 8) & 0xFF,
        (leastSignificant) & 0xFF,
    };
    return bytes;
}
#endif
