#include "StrLib.h"

namespace StrLib
{
    string ToString(const wstring& wideString)
    {
        if (wideString.empty())
            return {};
        const auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wideString[0], static_cast<int>(wideString.size()), nullptr, 0, nullptr, nullptr);
        string multiByteString(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wideString[0], static_cast<int>(wideString.size()), &multiByteString[0], sizeNeeded, nullptr, nullptr);
        return multiByteString;
    }

    wstring ToWstring(const string& multiByteString)
    {
        if (multiByteString.empty())
            return {};
        const auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &multiByteString[0], static_cast<int>(multiByteString.size()), nullptr, 0);
        wstring wideString(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &multiByteString[0], static_cast<int>(multiByteString.size()), &wideString[0], sizeNeeded);
        return wideString;
    }

    wstring ToWstring(const LPCCH multiByteString, const int size)
    {
        if (multiByteString == nullptr)
            return {};
        const auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, multiByteString, size, nullptr, 0);
        wstring wideString(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &multiByteString[0], size, &wideString[0], sizeNeeded);
        return wideString;
    }
    
    void UnEscapeXml(string& str)
    {
        static const vector escaping
    	{
            pair<string, string>("&amp;", "&"),
            pair<string, string>("&apos;", "\'"),
            pair<string, string>("&quot;", "\""),
            pair<string, string>("&lt;", "<"),
            pair<string, string>("&gt;", ">")
        };

        for (const auto& [from, to] : escaping)
	        ReplaceAll(str, from, to);
    }

    void TrimStart(string& str)
    {
        str.erase(str.begin(), ranges::find_if(str, [](const char ch) { return ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' && ch != '\v' && ch != '\f'; }));
    }

    void TrimEnd(string& str)
    {
        str.erase(std::find_if(str.rbegin(), str.rend(), [](const char ch) { return ch != ' ' && ch != '\r' && ch != '\n' && ch != '\t' && ch != '\v' && ch != '\f'; }).base(), str.end());
    }

    void Trim(string& str)
    {
        TrimEnd(str);
        TrimStart(str);
    }
}
