//
// Copyright (c) 2008-2020 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Core/StringUtils.h"

#include <cstdio>

#include "../DebugNew.h"

namespace Urho3D
{

static const ea::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

unsigned CountElements(const char* buffer, char separator)
{
    if (!buffer)
        return 0;

    const char* endPos = buffer + CStringLength(buffer);
    const char* pos = buffer;
    unsigned ret = 0;

    while (pos < endPos)
    {
        if (*pos != separator)
            break;
        ++pos;
    }

    while (pos < endPos)
    {
        const char* start = pos;

        while (start < endPos)
        {
            if (*start == separator)
                break;

            ++start;
        }

        if (start == endPos)
        {
            ++ret;
            break;
        }

        const char* end = start;

        while (end < endPos)
        {
            if (*end != separator)
                break;

            ++end;
        }

        ++ret;
        pos = end;
    }

    return ret;
}

bool ToBool(const ea::string& source)
{
    return ToBool(source.c_str());
}

bool ToBool(const char* source)
{
    unsigned length = CStringLength(source);

    for (unsigned i = 0; i < length; ++i)
    {
        auto c = (char)tolower(source[i]);
        if (c == 't' || c == 'y' || c == '1')
            return true;
        else if (c != ' ' && c != '\t')
            break;
    }

    return false;
}

int ToInt(const ea::string& source, int base)
{
    return ToInt(source.c_str(), base);
}

int ToInt(const char* source, int base)
{
    if (!source)
        return 0;

    // Shield against runtime library assert by converting illegal base values to 0 (autodetect)
    if (base < 2 || base > 36)
        base = 0;

    return (int)strtol(source, nullptr, base);
}

long long ToInt64(const char* source, int base)
{
    if (!source)
        return 0;

    // Shield against runtime library assert by converting illegal base values to 0 (autodetect)
    if (base < 2 || base > 36)
        base = 0;

    return strtoll(source, nullptr, base);
}

long long ToInt64(const ea::string& source, int base)
{
    return ToInt64(source.c_str(), base);
}

unsigned ToUInt(const ea::string& source, int base)
{
    return ToUInt(source.c_str(), base);
}

unsigned long long ToUInt64(const char* source, int base)
{
    if (!source)
        return 0;

    // Shield against runtime library assert by converting illegal base values to 0 (autodetect)
    if (base < 2 || base > 36)
        base = 0;

    return strtoull(source, nullptr, base);
}

unsigned long long ToUInt64(const ea::string& source, int base)
{
    return ToUInt64(source.c_str(), base);
}

unsigned ToUInt(const char* source, int base)
{
    if (!source)
        return 0;

    if (base < 2 || base > 36)
        base = 0;

    return (unsigned)strtoul(source, nullptr, base);
}

float ToFloat(const ea::string& source)
{
    return ToFloat(source.c_str());
}

float ToFloat(const char* source)
{
    if (!source)
        return 0;

    return (float)strtod(source, nullptr);
}

double ToDouble(const ea::string& source)
{
    return ToDouble(source.c_str());
}

double ToDouble(const char* source)
{
    if (!source)
        return 0;

    return strtod(source, nullptr);
}

Color ToColor(const ea::string& source)
{
    return ToColor(source.c_str());
}

Color ToColor(const char* source)
{
    Color ret;

    unsigned elements = CountElements(source, ' ');
    if (elements < 3)
        return ret;

    auto* ptr = (char*)source;
    ret.r_ = (float)strtod(ptr, &ptr);
    ret.g_ = (float)strtod(ptr, &ptr);
    ret.b_ = (float)strtod(ptr, &ptr);
    if (elements > 3)
        ret.a_ = (float)strtod(ptr, &ptr);

    return ret;
}

IntRect ToIntRect(const ea::string& source)
{
    return ToIntRect(source.c_str());
}

IntRect ToIntRect(const char* source)
{
    IntRect ret(IntRect::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 4)
        return ret;

    auto* ptr = (char*)source;
    ret.left_ = (int)strtol(ptr, &ptr, 10);
    ret.top_ = (int)strtol(ptr, &ptr, 10);
    ret.right_ = (int)strtol(ptr, &ptr, 10);
    ret.bottom_ = (int)strtol(ptr, &ptr, 10);

    return ret;
}

IntVector2 ToIntVector2(const ea::string& source)
{
    return ToIntVector2(source.c_str());
}

IntVector2 ToIntVector2(const char* source)
{
    IntVector2 ret(IntVector2::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 2)
        return ret;

    auto* ptr = (char*)source;
    ret.x_ = (int)strtol(ptr, &ptr, 10);
    ret.y_ = (int)strtol(ptr, &ptr, 10);

    return ret;
}

IntVector3 ToIntVector3(const ea::string& source)
{
    return ToIntVector3(source.c_str());
}

IntVector3 ToIntVector3(const char* source)
{
    IntVector3 ret(IntVector3::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 3)
        return ret;

    auto* ptr = (char*)source;
    ret.x_ = (int)strtol(ptr, &ptr, 10);
    ret.y_ = (int)strtol(ptr, &ptr, 10);
    ret.z_ = (int)strtol(ptr, &ptr, 10);

    return ret;
}

Rect ToRect(const ea::string& source)
{
    return ToRect(source.c_str());
}

Rect ToRect(const char* source)
{
    Rect ret(Rect::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 4)
        return ret;

    auto* ptr = (char*)source;
    ret.min_.x_ = (float)strtod(ptr, &ptr);
    ret.min_.y_ = (float)strtod(ptr, &ptr);
    ret.max_.x_ = (float)strtod(ptr, &ptr);
    ret.max_.y_ = (float)strtod(ptr, &ptr);

    return ret;
}

Quaternion ToQuaternion(const ea::string& source)
{
    return ToQuaternion(source.c_str());
}

Quaternion ToQuaternion(const char* source)
{
    unsigned elements = CountElements(source, ' ');
    auto* ptr = (char*)source;

    if (elements < 3)
        return Quaternion::IDENTITY;
    else if (elements < 4)
    {
        // 3 coords specified: conversion from Euler angles
        float x, y, z;
        x = (float)strtod(ptr, &ptr);
        y = (float)strtod(ptr, &ptr);
        z = (float)strtod(ptr, &ptr);

        return Quaternion(x, y, z);
    }
    else
    {
        // 4 coords specified: full quaternion
        Quaternion ret;
        ret.w_ = (float)strtod(ptr, &ptr);
        ret.x_ = (float)strtod(ptr, &ptr);
        ret.y_ = (float)strtod(ptr, &ptr);
        ret.z_ = (float)strtod(ptr, &ptr);

        return ret;
    }
}

Vector2 ToVector2(const ea::string& source)
{
    return ToVector2(source.c_str());
}

Vector2 ToVector2(const char* source)
{
    Vector2 ret(Vector2::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 2)
        return ret;

    auto* ptr = (char*)source;
    ret.x_ = (float)strtod(ptr, &ptr);
    ret.y_ = (float)strtod(ptr, &ptr);

    return ret;
}

Vector3 ToVector3(const ea::string& source)
{
    return ToVector3(source.c_str());
}

Vector3 ToVector3(const char* source)
{
    Vector3 ret(Vector3::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 3)
        return ret;

    auto* ptr = (char*)source;
    ret.x_ = (float)strtod(ptr, &ptr);
    ret.y_ = (float)strtod(ptr, &ptr);
    ret.z_ = (float)strtod(ptr, &ptr);

    return ret;
}

Vector4 ToVector4(const ea::string& source, bool allowMissingCoords)
{
    return ToVector4(source.c_str(), allowMissingCoords);
}

Vector4 ToVector4(const char* source, bool allowMissingCoords)
{
    Vector4 ret(Vector4::ZERO);

    unsigned elements = CountElements(source, ' ');
    auto* ptr = (char*)source;

    if (!allowMissingCoords)
    {
        if (elements < 4)
            return ret;

        ret.x_ = (float)strtod(ptr, &ptr);
        ret.y_ = (float)strtod(ptr, &ptr);
        ret.z_ = (float)strtod(ptr, &ptr);
        ret.w_ = (float)strtod(ptr, &ptr);

        return ret;
    }
    else
    {
        if (elements > 0)
            ret.x_ = (float)strtod(ptr, &ptr);
        if (elements > 1)
            ret.y_ = (float)strtod(ptr, &ptr);
        if (elements > 2)
            ret.z_ = (float)strtod(ptr, &ptr);
        if (elements > 3)
            ret.w_ = (float)strtod(ptr, &ptr);

        return ret;
    }
}

Variant ToVectorVariant(const ea::string& source)
{
    return ToVectorVariant(source.c_str());
}

Variant ToVectorVariant(const char* source)
{
    Variant ret;
    unsigned elements = CountElements(source, ' ');

    switch (elements)
    {
    case 1:
        ret.FromString(VariantType::Float, source);
        break;

    case 2:
        ret.FromString(VariantType::Vector2, source);
        break;

    case 3:
        ret.FromString(VariantType::Vector3, source);
        break;

    case 4:
        ret.FromString(VariantType::Vector4, source);
        break;

    case 9:
        ret.FromString(VariantType::Matrix3, source);
        break;

    case 12:
        ret.FromString(VariantType::Matrix3X4, source);
        break;

    case 16:
        ret.FromString(VariantType::Matrix4, source);
        break;

    default:
        // Illegal input. Return variant remains empty
        break;
    }

    return ret;
}

Matrix3 ToMatrix3(const ea::string& source)
{
    return ToMatrix3(source.c_str());
}

Matrix3 ToMatrix3(const char* source)
{
    Matrix3 ret(Matrix3::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 9)
        return ret;

    auto* ptr = (char*)source;
    ret.m00_ = (float)strtod(ptr, &ptr);
    ret.m01_ = (float)strtod(ptr, &ptr);
    ret.m02_ = (float)strtod(ptr, &ptr);
    ret.m10_ = (float)strtod(ptr, &ptr);
    ret.m11_ = (float)strtod(ptr, &ptr);
    ret.m12_ = (float)strtod(ptr, &ptr);
    ret.m20_ = (float)strtod(ptr, &ptr);
    ret.m21_ = (float)strtod(ptr, &ptr);
    ret.m22_ = (float)strtod(ptr, &ptr);

    return ret;
}

Matrix3x4 ToMatrix3x4(const ea::string& source)
{
    return ToMatrix3x4(source.c_str());
}

Matrix3x4 ToMatrix3x4(const char* source)
{
    Matrix3x4 ret(Matrix3x4::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 12)
        return ret;

    auto* ptr = (char*)source;
    ret.m00_ = (float)strtod(ptr, &ptr);
    ret.m01_ = (float)strtod(ptr, &ptr);
    ret.m02_ = (float)strtod(ptr, &ptr);
    ret.m03_ = (float)strtod(ptr, &ptr);
    ret.m10_ = (float)strtod(ptr, &ptr);
    ret.m11_ = (float)strtod(ptr, &ptr);
    ret.m12_ = (float)strtod(ptr, &ptr);
    ret.m13_ = (float)strtod(ptr, &ptr);
    ret.m20_ = (float)strtod(ptr, &ptr);
    ret.m21_ = (float)strtod(ptr, &ptr);
    ret.m22_ = (float)strtod(ptr, &ptr);
    ret.m23_ = (float)strtod(ptr, &ptr);

    return ret;
}

Matrix4 ToMatrix4(const ea::string& source)
{
    return ToMatrix4(source.c_str());
}

Matrix4 ToMatrix4(const char* source)
{
    Matrix4 ret(Matrix4::ZERO);

    unsigned elements = CountElements(source, ' ');
    if (elements < 16)
        return ret;

    auto* ptr = (char*)source;
    ret.m00_ = (float)strtod(ptr, &ptr);
    ret.m01_ = (float)strtod(ptr, &ptr);
    ret.m02_ = (float)strtod(ptr, &ptr);
    ret.m03_ = (float)strtod(ptr, &ptr);
    ret.m10_ = (float)strtod(ptr, &ptr);
    ret.m11_ = (float)strtod(ptr, &ptr);
    ret.m12_ = (float)strtod(ptr, &ptr);
    ret.m13_ = (float)strtod(ptr, &ptr);
    ret.m20_ = (float)strtod(ptr, &ptr);
    ret.m21_ = (float)strtod(ptr, &ptr);
    ret.m22_ = (float)strtod(ptr, &ptr);
    ret.m23_ = (float)strtod(ptr, &ptr);
    ret.m30_ = (float)strtod(ptr, &ptr);
    ret.m31_ = (float)strtod(ptr, &ptr);
    ret.m32_ = (float)strtod(ptr, &ptr);
    ret.m33_ = (float)strtod(ptr, &ptr);

    return ret;
}

ea::string ToStringBool(bool value)
{
    return value ? "true" : "false";
}

ea::string ToString(void* value)
{
    return ToStringHex((unsigned)(size_t)value);
}

ea::string ToStringHex(unsigned value)
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%08x", value);
    return ea::string(tempBuffer);
}

void BufferToString(ea::string& dest, const void* data, unsigned size)
{
    // Precalculate needed string size
    const auto* bytes = (const unsigned char*)data;
    unsigned length = 0;
    for (unsigned i = 0; i < size; ++i)
    {
        // Room for separator
        if (i)
            ++length;

        // Room for the value
        if (bytes[i] < 10)
            ++length;
        else if (bytes[i] < 100)
            length += 2;
        else
            length += 3;
    }

    dest.resize(length);
    unsigned index = 0;

    // Convert values
    for (unsigned i = 0; i < size; ++i)
    {
        if (i)
            dest[index++] = ' ';

        if (bytes[i] < 10)
        {
            dest[index++] = '0' + bytes[i];
        }
        else if (bytes[i] < 100)
        {
            dest[index++] = (char)('0' + bytes[i] / 10);
            dest[index++] = (char)('0' + bytes[i] % 10);
        }
        else
        {
            dest[index++] = (char)('0' + bytes[i] / 100);
            dest[index++] = (char)('0' + bytes[i] % 100 / 10);
            dest[index++] = (char)('0' + bytes[i] % 10);
        }
    }
}

void StringToBuffer(ea::vector<unsigned char>& dest, const ea::string& source)
{
    StringToBuffer(dest, source.c_str());
}

void StringToBuffer(ea::vector<unsigned char>& dest, const char* source)
{
    if (!source)
    {
        dest.clear();
        return;
    }

    unsigned size = CountElements(source, ' ');
    dest.resize(size);

    bool inSpace = true;
    unsigned index = 0;
    unsigned value = 0;

    // Parse values
    const char* ptr = source;
    while (*ptr)
    {
        if (inSpace && *ptr != ' ')
        {
            inSpace = false;
            value = (unsigned)(*ptr - '0');
        }
        else if (!inSpace && *ptr != ' ')
        {
            value *= 10;
            value += *ptr - '0';
        }
        else if (!inSpace && *ptr == ' ')
        {
            dest[index++] = (unsigned char)value;
            inSpace = true;
        }

        ++ptr;
    }

    // Write the final value
    if (!inSpace && index < size)
        dest[index] = (unsigned char)value;
}

void BufferToHexString(ea::string& dest, const void* data, unsigned size)
{
    dest.resize(size * 2);

    const auto* bytes = static_cast<const unsigned char*>(data);
    for (unsigned i = 0; i < size * 2; ++i)
    {
        const unsigned digit = i % 2
            ? bytes[i / 2] & 0xf
            : bytes[i / 2] >> 4;

        assert(digit < 16);
        if (digit < 10)
            dest[i] = '0' + digit;
        else
            dest[i] = 'a' + digit - 10;
    }
}

bool HexStringToBuffer(ea::vector<unsigned char>& dest, const ea::string_view& source)
{
    dest.resize(source.size() / 2);

    for (unsigned i = 0; i < source.size(); ++i)
    {
        const char ch = source[i];

        unsigned digit = 0;
        if (ch >= '0' && ch <= '9')
            digit = ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            digit = 10 + ch - 'A';
        else if (ch >= 'a' && ch <= 'f')
            digit = 10 + ch - 'a';
        else
            return false;

        assert(digit < 16);
        if (i % 2 == 0)
            dest[i / 2] = digit << 4;
        else
            dest[i / 2] |= digit;
    }

    // Fail if extra symbols in string
    return source.size() % 2 == 0;
}

unsigned GetStringListIndex(const ea::string& value, const ea::string* strings, unsigned defaultIndex, bool caseSensitive)
{
    return GetStringListIndex(value.c_str(), strings, defaultIndex, caseSensitive);
}

unsigned GetStringListIndex(const char* value, const ea::string* strings, unsigned defaultIndex, bool caseSensitive)
{
    unsigned i = 0;

    while (!strings[i].empty())
    {
        if (!Compare(strings[i].c_str(), value, caseSensitive))
            return i;
        ++i;
    }

    return defaultIndex;
}

unsigned GetStringListIndex(const char* value, const char* const* strings, unsigned defaultIndex, bool caseSensitive)
{
    unsigned i = 0;

    while (strings[i])
    {
        if (!Compare(value, strings[i], caseSensitive))
            return i;
        ++i;
    }

    return defaultIndex;
}

ea::string ToString(const char* formatString, ...)
{
    ea::string ret;
    va_list args;
    va_start(args, formatString);
    ret.append_sprintf_va_list(formatString, args);
    va_end(args);
    return ret;
}

bool IsAlpha(unsigned ch)
{
    return ch < 256 ? isalpha(ch) != 0 : false;
}

bool IsDigit(unsigned ch)
{
    return ch < 256 ? isdigit(ch) != 0 : false;
}

unsigned ToUpper(unsigned ch)
{
    return (unsigned)toupper(ch);
}

unsigned ToLower(unsigned ch)
{
    return (unsigned)tolower(ch);
}

ea::string GetFileSizeString(unsigned long long memorySize)
{
    static const char* memorySizeStrings = "kMGTPE";

    ea::string output;

    if (memorySize < 1024)
    {
        output = ea::to_string(memorySize) + " b";
    }
    else
    {
        const auto exponent = (int)(log((double)memorySize) / log(1024.0));
        const double majorValue = ((double)memorySize) / pow(1024.0, exponent);
        char buffer[64];
        memset(buffer, 0, 64);
        sprintf(buffer, "%.1f", majorValue);
        output = buffer;
        output += " ";
        output += memorySizeStrings[exponent - 1];
    }

    return output;
}

// Implementation of base64 decoding originally by Ren� Nyffenegger.
// Modified by Konstantin Guschin and Lasse Oorni

/*
base64.cpp and base64.h

Copyright (C) 2004-2017 Ren� Nyffenegger

This source code is provided 'as-is', without any express or implied
warranty. In no event will the author be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this source code must not be misrepresented; you must not
claim that you wrote the original source code. If you use this source code
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original source code.

3. This notice may not be removed or altered from any source distribution.

Ren� Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/

static inline bool IsBase64(char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

ea::string EncodeBase64(const ea::vector<unsigned char>& buffer)
{
    ea::string ret;

    const unsigned char* bytesToEncode = buffer.data();
    unsigned length = buffer.size();

    int i = 0;
    int j = 0;
    unsigned char charArray3[3];
    unsigned char charArray4[4];

    while (length--)
    {
        charArray3[i++] = *(bytesToEncode++);
        if (i == 3)
        {
            charArray4[0] = (charArray3[0] & 0xfc) >> 2;
            charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
            charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
            charArray4[3] = charArray3[2] & 0x3f;

            for (i = 0; (i < 4); i++)
                ret += base64_chars[charArray4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
            charArray3[j] = '\0';

        charArray4[0] = (charArray3[0] & 0xfc) >> 2;
        charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
        charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[charArray4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

ea::vector<unsigned char> DecodeBase64(ea::string encodedString)
{
    int inLen = encodedString.length();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char charArray4[4], charArray3[3];
    ea::vector<unsigned char> ret;

    while (inLen-- && (encodedString[in_] != '=') && IsBase64(encodedString[in_]))
    {
        charArray4[i++] = encodedString[in_];
        in_++;

        if (i == 4)
        {
            for (i = 0; i < 4; i++)
                charArray4[i] = base64_chars.find(charArray4[i]);

            charArray3[0] = (charArray4[0] << 2u) + ((charArray4[1] & 0x30u) >> 4u);
            charArray3[1] = ((charArray4[1] & 0xfu) << 4u) + ((charArray4[2] & 0x3cu) >> 2u);
            charArray3[2] = ((charArray4[2] & 0x3u) << 6u) + charArray4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(charArray3[i]);

            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j <4; j++)
            charArray4[j] = 0;

        for (j = 0; j <4; j++)
            charArray4[j] = base64_chars.find(charArray4[j]);

        charArray3[0] = (charArray4[0] << 2u) + ((charArray4[1] & 0x30u) >> 4u);
        charArray3[1] = ((charArray4[1] & 0xfu) << 4u) + ((charArray4[2] & 0x3cu) >> 2u);
        charArray3[2] = ((charArray4[2] & 0x3u) << 6u) + charArray4[3];

        for (j = 0; (j < i - 1); j++)
            ret.push_back(charArray3[j]);
    }

    return ret;
}

}
