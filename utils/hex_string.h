#ifndef HEX_STRING_H
#define HEX_STRING_H
#include <string>


namespace mm
{
    namespace hex
    {
        std::string addHexPrefix(std::string str)
        {
            if(str.empty()) 
            {
                return str;
            }
            return "0x" + str;
        }
        
        std::string remove0xPrefix(std::string str_hex)
        {
            if (str_hex.length() > 2 && str_hex.substr(0, 2) == "0x") 
            {
                return str_hex.substr(2);
            }
            return str_hex;
        }

    }
}

#endif //HEX_STRING_H