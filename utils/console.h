/**
 * *****************************************************************************
 * @file        console.h
 * @brief       
 * @date        2023-09-28
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef CA_CONSOLE_HEADER_GUARD
#define CA_CONSOLE_HEADER_GUARD

#include <string>

#define RESET "\033[0m"
#define BLACK "\033[30m"	/* Black */
#define RED "\033[31m"		/* Red */
#define GREEN "\033[32m"	/* Green */
#define YELLOW "\033[33m"	/* Yellow */
#define BLUE "\033[34m"		/* Blue */

typedef enum ConsoleColor
{
    CONSOLE_COLOR_BLACK,
    CONSOLE_COLOR_RED,
    CONSOLE_COLOR_GREEN,
    CONSOLE_COLOR_YELLOW,
    CONSOLE_COLOR_BLUE,
    kConsoleColorPurple,
    kConsoleColor_Dark_Green,
    CONSOLE_COLOR_WHITE,

} ConsoleColor;

/**
 * @brief       
 * 
 */
class CaConsole
{
public:
    /**
     * @brief       Construct a new Ca Console object
     * 
     * @param       foregroundColor: 
     * @param       backgroundColor: 
     * @param       highlight: 
     */
    CaConsole(const ConsoleColor foregroundColor = CONSOLE_COLOR_WHITE, 
                const ConsoleColor backgroundColor = CONSOLE_COLOR_BLACK, 
                bool highlight = false);
    
    /**
     * @brief       Destroy the Ca Console object
     * 
     */
    ~CaConsole();
    
    /**
     * @brief       Returns the Color
     * 
     * @return      const std::string 
     */
    const std::string Color(); 

    /**
     * @brief       Set the Color object
     * 
     * @param       foregroundColor: 
     * @param       backgroundColor: 
     * @param       highlight: 
     */
    void SetColor(const ConsoleColor foregroundColor = CONSOLE_COLOR_WHITE, 
                const ConsoleColor backgroundColor = CONSOLE_COLOR_BLACK, 
                bool highlight = false);
    
    /**
     * @brief       
     * 
     * @return      const std::string 
     */
    const std::string Reset(); //Reset

    /**
     * @brief       
     * 
     */
    void Clear(); //Clear

    operator const char * ();
    operator char * ();

private:
    std::string colorStringFlag;
    std::string colorString;
    bool _isHightLight;
};

#endif // !CA_CONSOLE_HEADER_GUARD