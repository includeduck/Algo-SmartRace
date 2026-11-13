#pragma once

// String: a small owned-buffer string (the no-STL replacement for
// std::string). Used for names, parser tokens and log lines. Growable, but
// most uses in this project are short-lived or fixed-size, so this favours
// simplicity over sophistication.
class String
{
public:
    String();
    explicit String(const char* text);
    String(const String& other);
    String& operator=(const String& other);
    ~String();

    void Set(const char* text);
    void Append(const char* text);
    void Format(const char* fmt, ...);

    const char* CStr() const
    {
        return m_data;
    }

    int Length() const
    {
        return m_length;
    }

    bool Empty() const
    {
        return m_length == 0;
    }

    bool Equals(const char* text) const;
    bool Equals(const String& other) const;

    // Parses the whole string as a decimal integer / floating point number.
    // *ok (if given) is set to false if the string isn't entirely a valid
    // number. No exponent support in ToDouble — not needed by the .trk /
    // .cfg formats this project uses.
    int ToInt(bool* ok = nullptr) const;
    double ToDouble(bool* ok = nullptr) const;

    static int StrLen(const char* s);
    static bool StrEquals(const char* a, const char* b);
    static void StrCopy(char* dest, int destCapacity, const char* src);

private:
    void EnsureCapacity(int needed);

    char* m_data;
    int m_length;
    int m_capacity;
};
