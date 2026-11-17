#pragma once

class Toast
{
public:
    void Show(const char* text, bool persistent = false);
    void Update(float dt);
    void Draw() const;
    void Clear()
    {
        m_left = 0;
        m_persistent = false;
    }
    const char* Text() const
    {
        return m_text;
    }
private:
    char m_text[512] = {};
    float m_left = 0;
    bool m_persistent = false;
};
