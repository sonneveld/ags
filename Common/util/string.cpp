//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include <algorithm>
#include <stdio.h> // sprintf
#include <stdlib.h>
#include <string.h>
#include "debug/assert.h"
#include "util/stream.h"
#include "util/string.h"
#include "util/string_utils.h"
#include "util/math.h"

namespace AGS
{
namespace Common
{

String::String() { }
String::String(const String &str) : __data(str.__data) {}
String::String(const char *cstr) {
    cstr = cstr ? cstr : "";
    __data.assign(cstr);
}
String::String(const std::string cppstr) : __data(cppstr) {}
String::~String() { }

void String::Read(Stream *in, size_t max_chars, bool stop_at_limit)
{
    __data.clear();

    if (!in) { return; }

    for(;;) {
        auto ichar = in->ReadByte();
        auto ch = (char)(ichar >= 0 ? ichar : 0);

        if (!ch) { break; }

        if (__data.length() < max_chars) {
            __data += ch;
        } else {
            if (stop_at_limit) { break; }
        }
    } 
}

void String::ReadCount(Stream *in, size_t count)
{
    __data.clear();

    if (!in) { return; }
    if (count <= 0) { return; }

    __data.resize(count);
    count = in->Read(&__data[0], count);
    __data.resize(count);
}

void String::Write(Stream *out) const
{
    if (!out) { return; }

    out->Write(GetCStr(), GetLength() + 1);
}

void String::WriteCount(Stream *out, size_t count) const
{
    if (!out) { return; }
    if (count <= 0) { return; }

    auto str_out_len = Math::Min(count - 1, GetLength());
    if (str_out_len > 0) {
        out->Write(GetCStr(), str_out_len);
    }
    
    auto null_out_len = count - str_out_len;
    if (null_out_len > 0) {
        out->WriteByteCount(0, null_out_len);
    }
}

int String::Compare(const char *cstr) const
{
    cstr = cstr ? cstr : "";
    return strcmp(GetCStr(), cstr);
}

int String::CompareNoCase(const char *cstr) const
{
    cstr = cstr ? cstr : "";
    return stricmp(GetCStr(), cstr);
}

int String::CompareLeft(const char *cstr, size_t count) const
{
    cstr = cstr ? cstr : "";
    return strncmp(GetCStr(), cstr, count != -1 ? count : strlen(cstr));
}

int String::CompareLeftNoCase(const char *cstr, size_t count) const
{
    cstr = cstr ? cstr : "";
    return strnicmp(GetCStr(), cstr, count != -1 ? count : strlen(cstr));
}

int String::CompareRightNoCase(const char *cstr, size_t count) const
{
    cstr = cstr ? cstr : "";
    count = count != -1 ? count : strlen(cstr);
    auto off = Math::Min(GetLength(), count);
    return strnicmp(GetCStr() + GetLength() - off, cstr, count);
}

size_t String::FindChar(char c, size_t from) const
{
    if (!c) { return -1; }
    auto result = __data.find(c, from);
    return result != std::string::npos ? result : -1;
}

size_t String::FindCharReverse(char c, size_t from) const
{
    if (!c) { return -1; }
    if (from == -1) {
        from = std::string::npos;
    }
    auto result = __data.rfind(c, from);
    return result != std::string::npos ? result : -1;
}

int String::ToInt() const
{
    try {
        return std::stoi(__data);
    } catch (std::invalid_argument &e) {
        return 0;
    }
}

/* static */ String String::FromFormat(const char *fcstr, ...)
{
    fcstr = fcstr ? fcstr : "";
    va_list argptr;
    va_start(argptr, fcstr);
    auto result = String::FromFormatV(fcstr, argptr);
    va_end(argptr);
    return result;
}

/* static */ String String::FromFormatV(const char *fcstr, va_list argptr)
{
    fcstr = fcstr ? fcstr : "";

    va_list argptr_cpy;
    va_copy(argptr_cpy, argptr);
    size_t length = vsnprintf(NULL, 0u, fcstr, argptr);

    auto data = std::string();
    data.resize(length);
    vsprintf(&data[0], fcstr, argptr_cpy);
    va_end(argptr_cpy);
    data.resize(length);

    return String(data);
}

/* static */ String String::FromStream(Stream *in, size_t max_chars, bool stop_at_limit)
{
    auto str = String();
    str.Read(in, max_chars, stop_at_limit);
    return str;
}

/* static */ String String::FromStreamCount(Stream *in, size_t count)
{
    auto str = String();
    str.ReadCount(in, count);
    return str;
}

String String::Lower() const
{
    auto str = *this;
    str.MakeLower();
    return str;
}

String String::Left(size_t count) const
{
    return __data.substr(0, count);
}

String String::Mid(size_t from, size_t count) const
{
    if (count <= 0) { return String(); }
    if (from >= __data.length()) { return String(); }
    return __data.substr(from, count);
}

std::vector<String> String::Split(String delims, int max_splits) const
{
    auto result = std::vector<String>();
    auto remaining = __data;
    auto remaining_splits = max_splits < 0 ? __data.length() : max_splits;

    while (remaining_splits > 0) {
        
        auto p = std::find_if(remaining.begin(), remaining.end(), 
            [&delims](int ch) {
                return delims.__data.find(ch) != std::string::npos;
        });

        if (p == remaining.end()) { break; }

        auto offset = p-remaining.begin();

        auto elem = remaining.substr(0, offset);
        result.push_back(elem);
        
        remaining = remaining.substr(offset+1);
        
        remaining_splits -= 1;
    }
    
    result.push_back(remaining);
    
    return result;
}

void String::Append(const char *cstr)
{
    cstr = cstr ? cstr : "";
    __data += cstr;
}

void String::AppendChar(char c)
{
    if (!c) { return; }
    __data += c;
}

void String::ClipLeft(size_t count)
{
    __data = __data.substr(count, __data.length()-count);
}

void String::ClipMid(size_t from, size_t count)
{
    __data.erase(from, count);
}

void String::ClipRight(size_t count)
{
    if (count <= 0) { return; }
    __data = __data.substr(0, __data.length() - count);
}

void String::Empty()
{
    __data.clear();
}

void String::Format(const char *fcstr, ...)
{
    fcstr = fcstr ? fcstr : "";

    va_list argptr;
    va_start(argptr, fcstr);

    *this = String::FromFormatV(fcstr, argptr).__data;

    va_end(argptr);
}

void String::Free()
{
    __data.clear();
    __data.resize(0);
    __data.shrink_to_fit();
 }

void String::MakeLower()
{
    strlwr(&__data[0]);
}

void String::Prepend(const char *cstr)
{
    cstr = cstr ? cstr : "";
    __data.insert(0, cstr);
}

void String::PrependChar(char c)
{
    if (!c) { return; }
    __data.insert(0, 1, c);
}

void String::Replace(char what, char with)
{
    if (!what) { return; }
    if (!with) { return; }
    if (what == with) { return; }

    auto rep_ptr = &__data[0];
    while (*rep_ptr)
    {
        if (*rep_ptr == what)
        {
            *rep_ptr = with;
        }
        rep_ptr++;
    }
}

void String::ReplaceMid(size_t from, size_t count, const char *cstr)
{
    cstr = cstr ? cstr : "";
    __data.replace(from, count, cstr);
}

void String::SetAt(size_t index, char c)
{
    if (!c) { return; }
    if (index >= GetLength()) { return; }

    __data[index] = c;
}

void String::SetString(const char *cstr, size_t length)
{
    cstr = cstr ? cstr : "";

    length = Math::Min(length, strlen(cstr));

    __data.assign(cstr);
    __data.resize(length);
}

void String::TrimRight(char c)
{
    // if c is 0, then trim spaces.

    __data.erase(std::find_if(__data.rbegin(), __data.rend(), [&c](int ch) {
        if (c) {
            return ch != c;
        }
        return (ch != ' ') && (ch != '\t') && (ch != '\r') && (ch != '\n');
    }).base(), __data.end());
}

void String::TruncateToLeft(size_t count)
{
    __data.resize(count);    
}

void String::TruncateToRight(size_t count)
{
    count = Math::Min(count, GetLength());
    __data = __data.substr(__data.length() - count, count);
}

String &String::operator=(const char *cstr)
{
    SetString(cstr);
    return *this;
}

} // namespace Common
} // namespace AGS
