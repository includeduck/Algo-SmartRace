#include "String.h"

#include <cstdio>
#include <cstdarg>

int String::StrLen(const char* s)
{
    int n = 0;
    while (s[n] != '\0') ++n;
    return n;
}

bool String::StrEquals(const char* a, const char* b)
{
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i]) return false;
        ++i;
    }
    return a[i] == b[i];
}

void String::StrCopy(char* dest, int destCapacity, const char* src)
{
    int i = 0;
    while (src[i] != '\0' && i < destCapacity - 1)
    {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

String::String()
    : m_data(nullptr), m_length(0), m_capacity(0)
{
    EnsureCapacity(1);
    m_data[0] = '\0';
}

String::String(const char* text)
    : m_data(nullptr), m_length(0), m_capacity(0)
{
    Set(text);
}

String::String(const String& other)
    : m_data(nullptr), m_length(0), m_capacity(0)
{
    Set(other.m_data);
}

String& String::operator=(const String& other)
{
    if (this != &other) Set(other.m_data);
    return *this;
}

String::~String()
{
    delete[] m_data;
}

void String::EnsureCapacity(int needed)
{
    if (needed <= m_capacity) return;

    int newCap = (m_capacity == 0) ? 16 : m_capacity * 2;
    if (newCap < needed) newCap = needed;

    char* newData = new char[newCap];
    if (m_data != nullptr) StrCopy(newData, newCap, m_data);
    else newData[0] = '\0';

    delete[] m_data;
    m_data = newData;
    m_capacity = newCap;
}

void String::Set(const char* text)
{
    int len = StrLen(text);
    EnsureCapacity(len + 1);
    StrCopy(m_data, m_capacity, text);
    m_length = len;
}

void String::Append(const char* text)
{
    int addLen = StrLen(text);
    EnsureCapacity(m_length + addLen + 1);
    for (int i = 0; i <= addLen; ++i) m_data[m_length + i] = text[i];
    m_length += addLen;
}

void String::Format(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Set(buffer);
}

bool String::Equals(const char* text) const
{
    return StrEquals(m_data, text);
}

bool String::Equals(const String& other) const
{
    return StrEquals(m_data, other.m_data);
}

int String::ToInt(bool* ok) const
{
    int i = 0;
    bool negative = false;

    if (m_data[i] == '+' || m_data[i] == '-')
    {
        negative = (m_data[i] == '-');
        ++i;
    }

    bool sawDigit = false;
    long value = 0;
    while (m_data[i] >= '0' && m_data[i] <= '9')
    {
        value = value * 10 + (m_data[i] - '0');
        ++i;
        sawDigit = true;
    }

    bool endsClean = (m_data[i] == '\0');
    if (ok != nullptr) *ok = sawDigit && endsClean;

    return static_cast<int>(negative ? -value : value);
}

double String::ToDouble(bool* ok) const
{
    int i = 0;
    bool negative = false;

    if (m_data[i] == '+' || m_data[i] == '-')
    {
        negative = (m_data[i] == '-');
        ++i;
    }

    bool sawDigit = false;
    double intPart = 0.0;
    while (m_data[i] >= '0' && m_data[i] <= '9')
    {
        intPart = intPart * 10.0 + (m_data[i] - '0');
        ++i;
        sawDigit = true;
    }

    double fracPart = 0.0;
    double fracScale = 1.0;
    if (m_data[i] == '.')
    {
        ++i;
        while (m_data[i] >= '0' && m_data[i] <= '9')
        {
            fracScale *= 10.0;
            fracPart = fracPart * 10.0 + (m_data[i] - '0');
            ++i;
            sawDigit = true;
        }
    }

    bool endsClean = (m_data[i] == '\0');
    if (ok != nullptr) *ok = sawDigit && endsClean;

    double value = intPart + (fracPart / fracScale);
    return negative ? -value : value;
}
