
#include "./string_util.h"


void StringUtil::Trim(std::string& str, bool bLeft, bool bRight)
{
    static const std::string delims = " \t\r\n";
    if(bRight)
        str.erase(str.find_last_not_of(delims)+1); // trim right
    if(bLeft)
        str.erase(0, str.find_first_not_of(delims)); // trim left
}


void StringUtil::SplitString(const std::string& s, const std::string& c ,std::vector<std::string>& v)
{
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2)
    {
        std::string str = s.substr(pos1, pos2-pos1);

        if (!str.empty())
        {
            v.push_back(str);
        }
        
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }

    if(pos1 != s.length())
    {
        std::string str = s.substr(pos1);
        if (!str.empty())
        {
            v.push_back(str);
        }
    }
}

void StringUtil::SplitString(const std::string& str, const std::string& delim,
    std::multimap<std::string, std::string>& result) {

    std::string::size_type pos1, pos2;
    pos2 = str.find(delim);
    pos1 = 0;
    int count = 0;
    while (pos2 != std::string::npos) {
        ++count;
        std::string keyValue = str.substr(pos1, pos2 - pos1);

        size_t hyphenPos = keyValue.find("-");
        std::string key = keyValue.substr(0, hyphenPos);
        std::string value = keyValue.substr(hyphenPos + 1);

        result.insert({ key, value });

        pos1 = pos2 + delim.size();
        pos2 = str.find(delim, pos1);
    }
    
    if (count == 0)
    {
        std::string keyValue = str.substr(pos1);

        size_t hyphenPos = keyValue.find("-");
        std::string key = keyValue.substr(0, hyphenPos);
        std::string value = keyValue.substr(hyphenPos + 1);

        result.insert({ key, value });
    }

}

void StringUtil::SplitStringToSet(const std::string& s,  const std::string& c, std::set<std::string>& v)
{
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2)
    {
        std::string str = s.substr(pos1, pos2-pos1);

        if (!str.empty())
        {
            v.insert(str);
        }
        
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }

    if(pos1 != s.length())
    {
        std::string str = s.substr(pos1);
        if (!str.empty())
        {
            v.insert(str);
        }
    }
}

int64_t StringUtil::StringToNumber(const std::string &s) {
    int64_t number = 0;
    for (const char& c : s)
    {
        number += static_cast<int64_t>(c);
    }
    return number;
}
