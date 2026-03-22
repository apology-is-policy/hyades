// figlet.c - ASCII Art Text Renderer
#include "figlet.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Font Character Definition
// ============================================================================

typedef struct {
    int c;              // Character code
    int width;          // Character width in columns
    const char **lines; // Array of strings, one per line
} FigChar;

typedef struct {
    const char *name;
    int height; // Number of lines per character
    const FigChar *chars;
    int char_count;
} FigFontDef;

// ============================================================================
// Standard Font (5 lines tall)
// ============================================================================

// Character 'A'
static const char *standard_A[] = {"    _    ", "   / \\   ", "  / _ \\  ", " / ___ \\ ",
                                   "/_/   \\_\\"};

// Character 'B'
static const char *standard_B[] = {" ____  ", "| __ ) ", "|  _ \\ ", "| |_) |", "|____/ "};

// Character 'C'
static const char *standard_C[] = {"  ____ ", " / ___|", "| |    ", "| |___ ", " \\____|"};

// Character 'D'
static const char *standard_D[] = {" ____  ", "|  _ \\ ", "| | | |", "| |_| |", "|____/ "};

// Character 'E'
static const char *standard_E[] = {" _____ ", "|  ___|", "| |_   ", "|  _|_ ", "|_____|"};

// Character 'F'
static const char *standard_F[] = {" _____ ", "|  ___|", "| |_   ", "|  _|  ", "|_|    "};

// Character 'G'
static const char *standard_G[] = {"  ____ ", " / ___|", "| |  _ ", "| |_| |", " \\____|"};

// Character 'H'
static const char *standard_H[] = {" _   _ ", "| | | |", "| |_| |", "|  _  |", "|_| |_|"};

// Character 'I'
static const char *standard_I[] = {" ___ ", "|_ _|", " | | ", " | | ", "|___|"};

// Character 'J'
static const char *standard_J[] = {"     _ ", "    | |", "    | |",
                                   " _  | |", "| |_| |", " \\___/ "};

// Character 'K'
static const char *standard_K[] = {" _  __", "| |/ /", "| ' / ", "| . \\ ", "|_|\\_\\"};

// Character 'L'
static const char *standard_L[] = {" _     ", "| |    ", "| |    ", "| |___ ", "|_____|"};

// Character 'M'
static const char *standard_M[] = {" __  __ ", "|  \\/  |", "| |\\/| |", "| |  | |", "|_|  |_|"};

// Character 'N'
static const char *standard_N[] = {" _   _ ", "| \\ | |", "|  \\| |", "| |\\  |", "|_| \\_|"};

// Character 'O'
static const char *standard_O[] = {"  ___  ", " / _ \\ ", "| | | |", "| |_| |", " \\___/ "};

// Character 'P'
static const char *standard_P[] = {" ____  ", "|  _ \\ ", "| |_) |", "|  __/ ", "|_|    "};

// Character 'Q'
static const char *standard_Q[] = {"  ___  ", " / _ \\ ", "| | | |", "| |_| |", " \\__\\_\\"};

// Character 'R'
static const char *standard_R[] = {" ____  ", "|  _ \\ ", "| |_) |", "|  _ < ", "|_| \\_\\"};

// Character 'S'
static const char *standard_S[] = {" ____  ", "/ ___| ", "\\___ \\ ", " ___) |", "|____/ "};

// Character 'T'
static const char *standard_T[] = {" _____ ", "|_   _|", "  | |  ", "  | |  ", "  |_|  "};

// Character 'U'
static const char *standard_U[] = {" _   _ ", "| | | |", "| | | |", "| |_| |", " \\___/ "};

// Character 'V'
static const char *standard_V[] = {"__     __", "\\ \\   / /", " \\ \\ / / ", "  \\ V /  ",
                                   "   \\_/   "};

// Character 'W'
static const char *standard_W[] = {"__        __", "\\ \\      / /", " \\ \\ /\\ / / ",
                                   "  \\ V  V /  ", "   \\_/\\_/   "};

// Character 'X'
static const char *standard_X[] = {"__  __", "\\ \\/ /", " \\  / ", " /  \\ ", "/_/\\_\\"};

// Character 'Y'
static const char *standard_Y[] = {"__   __", "\\ \\ / /", " \\ V / ", "  | |  ", "  |_|  "};

// Character 'Z'
static const char *standard_Z[] = {" _____", "|__  /", "  / / ", " / /_ ", "/____|"};

// Digits

// Character '0'
static const char *standard_0[] = {"  ___  ", " / _ \\ ", "| | | |", "| |_| |", " \\___/ "};

// Character '1'
static const char *standard_1[] = {" _ ", "/ |", "| |", "| |", "|_|"};

// Character '2'
static const char *standard_2[] = {" ____  ", "|___ \\ ", "  __) |", " / __/ ", "|_____|"};

// Character '3'
static const char *standard_3[] = {" _____ ", "|___ / ", "  |_ \\ ", " ___) |", "|____/ "};

// Character '4'
static const char *standard_4[] = {" _  _   ", "| || |  ", "| || |_ ", "|__   _|", "   |_|  "};

// Character '5'
static const char *standard_5[] = {" ____  ", "| ___| ", "|___ \\ ", " ___) |", "|____/ "};

// Character '6'
static const char *standard_6[] = {"  __   ", " / /_  ", "| '_ \\ ", "| (_) |", " \\___/ "};

// Character '7'
static const char *standard_7[] = {" _____ ", "|___  |", "   / / ", "  / /  ", " /_/   "};

// Character '8'
static const char *standard_8[] = {"  ___  ", " ( _ ) ", " / _ \\ ", "| (_) |", " \\___/ "};

// Character '9'
static const char *standard_9[] = {"  ___  ", " / _ \\ ", "| (_) |", " \\__, |", "   /_/ "};

// Space
static const char *standard_space[] = {"  ", "  ", "  ", "  ", "  "};

// Exclamation mark
static const char *standard_exclaim[] = {" _ ", "| |", "| |", "|_|", "(_)"};

// Question mark
static const char *standard_question[] = {" ___ ", "|__ \\", "  / /", " |_| ", " (_) "};

// Period
static const char *standard_period[] = {"   ", "   ", "   ", " _ ", "(_)"};

// Comma
static const char *standard_comma[] = {"   ", "   ", "   ", " _ ", "( )", "|/ "};

// Hyphen/Dash
static const char *standard_dash[] = {"      ", "      ", " ____ ", "|____|", "      "};

// Plus
static const char *standard_plus[] = {"       ", "   _   ", " _| |_ ", "|_   _|", "  |_|  "};

// Equals
static const char *standard_equals[] = {"       ", " _____ ", "|_____|", "|_____|", "       "};

// Forward slash
static const char *standard_slash[] = {"    __", "   / /", "  / / ", " / /  ", "/_/   "};

// Backslash
static const char *standard_backslash[] = {"__ ", "\\ \\", " \\ \\", "  \\ \\", "   \\_\\"};

// Standard font character table
static const FigChar standard_chars[] = {
    {'A', 9, standard_A},         {'B', 7, standard_B},       {'C', 7, standard_C},
    {'D', 7, standard_D},         {'E', 7, standard_E},       {'F', 7, standard_F},
    {'G', 7, standard_G},         {'H', 7, standard_H},       {'I', 5, standard_I},
    {'J', 7, standard_J},         {'K', 6, standard_K},       {'L', 7, standard_L},
    {'M', 9, standard_M},         {'N', 7, standard_N},       {'O', 7, standard_O},
    {'P', 7, standard_P},         {'Q', 7, standard_Q},       {'R', 7, standard_R},
    {'S', 7, standard_S},         {'T', 7, standard_T},       {'U', 7, standard_U},
    {'V', 9, standard_V},         {'W', 12, standard_W},      {'X', 6, standard_X},
    {'Y', 7, standard_Y},         {'Z', 6, standard_Z},       {'0', 7, standard_0},
    {'1', 3, standard_1},         {'2', 7, standard_2},       {'3', 7, standard_3},
    {'4', 8, standard_4},         {'5', 7, standard_5},       {'6', 7, standard_6},
    {'7', 7, standard_7},         {'8', 7, standard_8},       {'9', 7, standard_9},
    {' ', 2, standard_space},     {'!', 3, standard_exclaim}, {'?', 5, standard_question},
    {'.', 3, standard_period},    {',', 3, standard_comma},   {'-', 6, standard_dash},
    {'+', 7, standard_plus},      {'=', 7, standard_equals},  {'/', 6, standard_slash},
    {'\\', 4, standard_backslash}};

static const FigFontDef standard_font = {.name = "Standard",
                                         .height = 5,
                                         .chars = standard_chars,
                                         .char_count =
                                             sizeof(standard_chars) / sizeof(standard_chars[0])};

// ============================================================================
// Small Font (4 lines tall - compact)
// ============================================================================

// Character 'A'
static const char *small_A[] = {" _   ", "/_\\  ", "/ _ \\ ", "/_/ \\_\\"};

// Character 'B'
static const char *small_B[] = {" ___  ", "| _ ) ", "| _ \\ ", "|___/ "};

// Character 'C'
static const char *small_C[] = {"  ___ ", " / __|", "| (__ ", " \\___|"};

// Character 'D'
static const char *small_D[] = {" ___  ", "|   \\ ", "| |) |", "|___/ "};

// Character 'E'
static const char *small_E[] = {" ___ ", "| __|", "| _| ", "|___|"};

// Character 'F'
static const char *small_F[] = {" ___ ", "| __|", "| _| ", "|_|  "};

// Character 'G'
static const char *small_G[] = {"  ___ ", " / __|", "| (_ |", " \\___|"};

// Character 'H'
static const char *small_H[] = {" _  _ ", "| || |", "| __ |", "|_||_|"};

// Character 'I'
static const char *small_I[] = {" ___ ", "|_ _|", " | | ", "|___|"};

// Character 'J'
static const char *small_J[] = {"   _ ", "  | |", " _| |", "|___/ "};

// Character 'K'
static const char *small_K[] = {" _  __", "| |/ /", "| ' < ", "|_|\\_\\"};

// Character 'L'
static const char *small_L[] = {" _    ", "| |   ", "| |__ ", "|____|"};

// Character 'M'
static const char *small_M[] = {" __  __ ", "|  \\/  |", "| |\\/| |", "|_|  |_|"};

// Character 'N'
static const char *small_N[] = {" _  _ ", "| \\| |", "| .` |", "|_|\\_|"};

// Character 'O'
static const char *small_O[] = {"  ___  ", " / _ \\ ", "| (_) |", " \\___/ "};

// Character 'P'
static const char *small_P[] = {" ___  ", "| _ \\ ", "|  _/ ", "|_|   "};

// Character 'Q'
static const char *small_Q[] = {"  ___  ", " / _ \\ ", "| (_) |", " \\__\\_\\"};

// Character 'R'
static const char *small_R[] = {" ___  ", "| _ \\ ", "|   / ", "|_|_\\"};

// Character 'S'
static const char *small_S[] = {" ___ ", "/ __|", "\\__ \\", "|___/"};

// Character 'T'
static const char *small_T[] = {" _____ ", "|_   _|", "  | |  ", "  |_|  "};

// Character 'U'
static const char *small_U[] = {" _  _ ", "| || |", "| || |", " \\__/ "};

// Character 'V'
static const char *small_V[] = {"__   __", "\\ \\ / /", " \\ V / ", "  \\_/  "};

// Character 'W'
static const char *small_W[] = {"__      __", "\\ \\    / /", " \\ \\/\\/ / ", "  \\_/\\_/  "};

// Character 'X'
static const char *small_X[] = {"__  __", "\\ \\/ /", " >  < ", "/_/\\_\\"};

// Character 'Y'
static const char *small_Y[] = {"__   __", "\\ \\ / /", " \\ V / ", "  |_|  "};

// Character 'Z'
static const char *small_Z[] = {" ____", "|_  /", " / / ", "/___|"};

// Digits

// Character '0'
static const char *small_0[] = {"  ___  ", " / _ \\ ", "| | | |", " \\___/ "};

// Character '1'
static const char *small_1[] = {" _ ", "/ |", "| |", "|_|"};

// Character '2'
static const char *small_2[] = {" ___ ", "|_  )", " / / ", "/___|"};

// Character '3'
static const char *small_3[] = {" ___ ", "|__ \\", " |_ \\", "|___/"};

// Character '4'
static const char *small_4[] = {" _ _  ", "| | | ", "|_  _|", "  |_| "};

// Character '5'
static const char *small_5[] = {" ___ ", "| __|", "|__ \\", "|___/"};

// Character '6'
static const char *small_6[] = {"  __ ", " / / ", "/ _ \\", "\\___/"};

// Character '7'
static const char *small_7[] = {" ____ ", "|__  |", "  / / ", " /_/  "};

// Character '8'
static const char *small_8[] = {" ___ ", "( _ )", "/ _ \\", "\\___/"};

// Character '9'
static const char *small_9[] = {" ___ ", "/ _ \\", "\\_, /", " /_/ "};

// Space
static const char *small_space[] = {" ", " ", " ", " "};

// Exclamation mark
static const char *small_exclaim[] = {" _ ", "| |", "|_|", "(_)"};

// Question mark
static const char *small_question[] = {" ___ ", "|__ \\", " / / ", "/_/  "};

// Period
static const char *small_period[] = {"  ", "  ", " _", "(_)"};

// Comma
static const char *small_comma[] = {"  ", "  ", " _", "(_)"};

// Hyphen/Dash
static const char *small_dash[] = {"    ", " __ ", "|__|", "    "};

// Plus
static const char *small_plus[] = {"     ", " _|_ ", "|_|_|", "  |  "};

// Equals
static const char *small_equals[] = {"     ", " ___ ", "|___|", "     "};

// Forward slash
static const char *small_slash[] = {"   __", "  / /", " / / ", "/_/  "};

// Backslash
static const char *small_backslash[] = {"__ ", "\\ \\", " \\ \\", "  \\_\\"};

// Small font character table
static const FigChar small_chars[] = {
    {'A', 5, small_A},         {'B', 6, small_B},       {'C', 6, small_C},
    {'D', 6, small_D},         {'E', 5, small_E},       {'F', 5, small_F},
    {'G', 6, small_G},         {'H', 6, small_H},       {'I', 5, small_I},
    {'J', 5, small_J},         {'K', 6, small_K},       {'L', 6, small_L},
    {'M', 9, small_M},         {'N', 6, small_N},       {'O', 7, small_O},
    {'P', 6, small_P},         {'Q', 7, small_Q},       {'R', 6, small_R},
    {'S', 5, small_S},         {'T', 7, small_T},       {'U', 6, small_U},
    {'V', 7, small_V},         {'W', 10, small_W},      {'X', 6, small_X},
    {'Y', 7, small_Y},         {'Z', 5, small_Z},       {'0', 7, small_0},
    {'1', 3, small_1},         {'2', 5, small_2},       {'3', 5, small_3},
    {'4', 6, small_4},         {'5', 5, small_5},       {'6', 5, small_6},
    {'7', 6, small_7},         {'8', 5, small_8},       {'9', 5, small_9},
    {' ', 1, small_space},     {'!', 3, small_exclaim}, {'?', 5, small_question},
    {'.', 2, small_period},    {',', 2, small_comma},   {'-', 4, small_dash},
    {'+', 5, small_plus},      {'=', 5, small_equals},  {'/', 5, small_slash},
    {'\\', 4, small_backslash}};

static const FigFontDef small_font = {.name = "Small",
                                      .height = 4,
                                      .chars = small_chars,
                                      .char_count = sizeof(small_chars) / sizeof(small_chars[0])};

// ============================================================================
// Tiny Font (4 lines tall - ultra-condensed with lowercase & destructive overlap)
// ============================================================================

// Uppercase letters (4 lines)

// Character 'A'
static const char *tiny_A[] = {"   _    ", "  /_\\   ", " / _ \\  ", "/_/ \\_\\ ", "        "};

// Character 'B'
static const char *tiny_B[] = {" ___ ", "| _ )", "| _ \\", "|___/", "     "};

// Character 'C'
static const char *tiny_C[] = {"  ___ ", " / __|", "| (__ ", " \\___|", "      "};

// Character 'D'
static const char *tiny_D[] = {" ___  ", "|   \\ ", "| |) |", "|___/ ", "      "};

// Character 'E'
static const char *tiny_E[] = {" ___ ", "| __|", "| _| ", "|___|", "     "};

// Character 'F'
static const char *tiny_F[] = {" ___ ", "| __|", "| _| ", "|_|  ", "     "};

// Character 'G'
static const char *tiny_G[] = {"  ___ ", " / __|", "| (_ |", " \\___|", "      "};

// Character 'H'
static const char *tiny_H[] = {" _  _ ", "| || |", "| __ |", "|_||_|", "      "};

// Character 'I'
static const char *tiny_I[] = {" ___ ", "|_ _|", " | | ", "|___|", "     "};

// Character 'J'
static const char *tiny_J[] = {"   _ ", "  | |", " _| |", " \\__/", "     "};

// Character 'K'
static const char *tiny_K[] = {" _  __", "| |/ /", "| ' < ", "|_|\\_\\", "      "};

// Character 'L'
static const char *tiny_L[] = {" _    ", "| |   ", "| |__ ", "|____|", "      "};

// Character 'M'
static const char *tiny_M[] = {" __  __ ", "|  \\/  |", "| |\\/| |", "|_|  |_|", "        "};

// Character 'N'
static const char *tiny_N[] = {" _  _ ", "| \\| |", "| .` |", "|_|\\_|", "      "};

// Character 'O'
static const char *tiny_O[] = {"  ___  ", " / _ \\ ", "| (_) |", " \\___/ ", "       "};

// Character 'P'
static const char *tiny_P[] = {" ___  ", "| _ \\ ", "|  _/ ", "|_|   ", "      "};

// Character 'Q'
static const char *tiny_Q[] = {"  ___  ", " / _ \\ ", "| (_) |", " \\__\\_\\", "       "};

// Character 'R'
static const char *tiny_R[] = {" ___  ", "| _ \\ ", "|   / ", "|_|_\\ ", "      "};

// Character 'S'
static const char *tiny_S[] = {" ___ ", "/ __|", "\\__ \\", "|___/", "     "};

// Character 'T'
static const char *tiny_T[] = {" _____ ", "|_   _|", "  | |  ", "  |_|  ", "       "};

// Character 'U'
static const char *tiny_U[] = {" _  _ ", "| || |", "| || |", " \\__/ ", "      "};

// Character 'V'
static const char *tiny_V[] = {"__   __", "\\ \\ / /", " \\ V / ", "  \\_/  ", "       "};

// Character 'W'
static const char *tiny_W[] = {"__      __", "\\ \\    / /", " \\ \\/\\/ / ", "  \\_/\\_/  ",
                               "          "};

// Character 'X'
static const char *tiny_X[] = {"__  __", "\\ \\/ /", " >  < ", "/_/\\_\\", "      "};

// Character 'Y'
static const char *tiny_Y[] = {"__   __", "\\ \\ / /", " \\ V / ", "  |_|  ", "       "};

// Character 'Z'
static const char *tiny_Z[] = {" ____", "|_  /", " / / ", "/___|", "     "};

// Lowercase letters (4 lines, designed for overlap)
// Character 'a' - FIX: Make all lines width 6
static const char *tiny_a[] = {"      ", " __ _ ", "/ _` |", "\\__,_|", "      "};

// Character 'b' - ✅ Already correct (width 6)
static const char *tiny_b[] = {" _    ", "| |__ ", "| '_ \\", "|_.__/", "      "};

// Character 'c' - FIX: Make all lines width 5
static const char *tiny_c[] = {"     ", " __  ", "/ __|", "\\__| ", "     "};

// Character 'd' - FIX: Make all lines width 6
static const char *tiny_d[] = {"    _ ", " __| |", "/ _` |", "\\__,_|", "      "};

// Character 'e' - FIX: Make all lines width 5
static const char *tiny_e[] = {"     ", " ___ ", "/ -_)", "\\___|", "     "};

// Character 'f' - ✅ Already correct (width 5)
static const char *tiny_f[] = {"  __ ", " / _|", "|  _|", "|_|  ", "     "};

// Character 'g' - FIX: Make all lines width 6
static const char *tiny_g[] = {"      ", " __ _ ", "/ _` |", "\\__, |", "   |_/"};

// Character 'h' - FIX: Make all lines width 6
static const char *tiny_h[] = {" _    ", "| |_  ", "| ' \\ ", "|_||_|", "      "};

// Character 'i' - ✅ Already correct (width 3)
static const char *tiny_i[] = {" _ ", "(_)", "| |", "|_|", "   "};

// Character 'j' - ✅ Already correct (width 4)
static const char *tiny_j[] = {"  _ ", " (_)", " | |", "_/ |", "|__/"};

// Character 'k' - FIX: Make all lines width 5
static const char *tiny_k[] = {" _   ", "| |__", "| / /", "|_\\_\\", "     "};

// Character 'l' - ✅ Already correct (width 3)
static const char *tiny_l[] = {" _ ", "| |", "| |", "|_|", "   "};

// Character 'm' - FIX: Make all lines width 7
static const char *tiny_m[] = {"       ", " _ __  ", "| '  \\ ", "|_|_|_|", "       "};

// Character 'n' - FIX: Make all lines width 6
static const char *tiny_n[] = {"      ", " _ _  ", "| ' \\ ", "|_||_|", "      "};

// Character 'o' - FIX: Make all lines width 5
static const char *tiny_o[] = {"     ", " ___ ", "/ _ \\", "\\___/", "     "};

// Character 'p' - FIX: Line 0 should be width 6
static const char *tiny_p[] = {"      ", " _ __ ", "| '_ \\", "| .__/", "|_|   "};

// Character 'q' - FIX: Make all lines width 7
static const char *tiny_q[] = {"       ", "  __ _ ", " / _` |", " \\__, |", "    |_/"};

// Character 'r' - FIX: Make all lines width 5
static const char *tiny_r[] = {"     ", " _ _ ", "| '_|", "|_|  ", "     "};

// Character 's' - ✅ Already correct (width 4)
static const char *tiny_s[] = {"    ", " ___", "(_-<", "/__/", "    "};

// Character 't' - FIX: Make all lines width 5
static const char *tiny_t[] = {" _   ", "| |_ ", "|  _|", " \\__|", "     "};

// Character 'u' - FIX: Make all lines width 6
static const char *tiny_u[] = {"      ", " _  _ ", "| || |", " \\_,_|", "      "};

// Character 'v' - FIX: Make all lines width 6
static const char *tiny_v[] = {"      ", " __ __", " \\ V /", "  \\_/ ", "      "};

// Character 'w' - FIX: Make all lines width 9
static const char *tiny_w[] = {"         ", " __ __ __", " \\ V  V /", "  \\_/\\_/ ", "         "};

// Character 'x' - FIX: Make all lines width 7
static const char *tiny_x[] = {"       ", " __ __ ", " \\ \\_/ ", " /_/\\_\\", "       "};

// Character 'y' - FIX: Make all lines width 6
static const char *tiny_y[] = {"      ", " _  _ ", "| || |", " \\_, |", "   |_/"};

// Character 'z' - ✅ Already correct (width 4)
static const char *tiny_z[] = {"    ", " ___", "|_ /", "/__|", "    "};

// Digits (4 lines)

// Character '0'
static const char *tiny_0[] = {" ___ ", "|   |", "| | |", "|_|_|", "     "};

// Character '1'
static const char *tiny_1[] = {" _ ", "/ |", "| |", "|_|", "   "};

// Character '2'
static const char *tiny_2[] = {" ___ ", "|_  )", " / / ", "/___|", "     "};

// Character '3'
static const char *tiny_3[] = {" ____", "|__ /", " |_ \\", "|___/", "     "};

// Character '4'
static const char *tiny_4[] = {" _ _ ", "| | |", "|_  |", "  |_|", "     "};

// Character '5'
static const char *tiny_5[] = {" ___ ", "| __|", "|__ \\", "|___/", "     "};

// Character '6'
static const char *tiny_6[] = {" ___ ", "/ __|", "| _ \\", "\\___/", "     "};

// Character '7'
static const char *tiny_7[] = {" ____ ", "|__  |", "  / / ", " /_/  ", "      "};

// Character '8'
static const char *tiny_8[] = {" ___ ", "( _ )", "/ _ \\", "\\___/", "     "};

// Character '9'
static const char *tiny_9[] = {" ___ ", "/ _ \\", "\\_, /", " /_/ ", "     "};

// Space (4 lines)
static const char *tiny_space[] = {"   ", "   ", "   ", "   ", "   "};

// Exclamation (4 lines)
static const char *tiny_exclaim[] = {" _ ", "| |", "|_|", "(_)", "   "};

// Question (4 lines)
static const char *tiny_question[] = {" ___ ", "|__ \\", " |_/ ", " (_) ", "     "};

// Period (4 lines)
static const char *tiny_period[] = {" ", " ", " ", ".", " "};

// Comma (4 lines)
static const char *tiny_comma[] = {" ", " ", "_", ",", " "};

// Dash (4 lines)
static const char *tiny_dash[] = {"   ", "___", "   ", "   ", "   "};

// Colon (4 lines)
static const char *tiny_colon[] = {" ", ".", " ", ".", " "};

// Tiny font character table
static const FigChar tiny_chars[] = {
    // Uppercase
    {'A', 5, tiny_A},
    {'B', 5, tiny_B},
    {'C', 5, tiny_C},
    {'D', 6, tiny_D},
    {'E', 5, tiny_E},
    {'F', 5, tiny_F},
    {'G', 6, tiny_G},
    {'H', 6, tiny_H},
    {'I', 5, tiny_I},
    {'J', 5, tiny_J},
    {'K', 6, tiny_K},
    {'L', 6, tiny_L},
    {'M', 9, tiny_M},
    {'N', 6, tiny_N},
    {'O', 7, tiny_O},
    {'P', 6, tiny_P},
    {'Q', 7, tiny_Q},
    {'R', 6, tiny_R},
    {'S', 5, tiny_S},
    {'T', 7, tiny_T},
    {'U', 6, tiny_U},
    {'V', 7, tiny_V},
    {'W', 10, tiny_W},
    {'X', 6, tiny_X},
    {'Y', 7, tiny_Y},
    {'Z', 5, tiny_Z},

    // Lowercase
    {'a', 5, tiny_a},
    {'b', 6, tiny_b},
    {'c', 3, tiny_c},
    {'d', 6, tiny_d},
    {'e', 5, tiny_e},
    {'f', 5, tiny_f},
    {'g', 6, tiny_g},
    {'h', 5, tiny_h},
    {'i', 3, tiny_i},
    {'j', 4, tiny_j},
    {'k', 5, tiny_k},
    {'l', 3, tiny_l},
    {'m', 7, tiny_m},
    {'n', 6, tiny_n},
    {'o', 5, tiny_o},
    {'p', 6, tiny_p},
    {'q', 6, tiny_q},
    {'r', 5, tiny_r},
    {'s', 4, tiny_s},
    {'t', 5, tiny_t},
    {'u', 6, tiny_u},
    {'v', 6, tiny_v},
    {'w', 9, tiny_w},
    {'x', 6, tiny_x},
    {'y', 6, tiny_y},
    {'z', 4, tiny_z},

    // Digits
    {'0', 5, tiny_0},
    {'1', 3, tiny_1},
    {'2', 5, tiny_2},
    {'3', 5, tiny_3},
    {'4', 5, tiny_4},
    {'5', 5, tiny_5},
    {'6', 5, tiny_6},
    {'7', 5, tiny_7},
    {'8', 5, tiny_8},
    {'9', 5, tiny_9},

    // Punctuation
    {' ', 3, tiny_space},
    {'!', 3, tiny_exclaim},
    {'?', 5, tiny_question},
    {'.', 1, tiny_period},
    {',', 1, tiny_comma},
    {'-', 3, tiny_dash},
    {':', 1, tiny_colon}};

static const FigFontDef tiny_font = {.name = "Tiny",
                                     .height = 5,
                                     .chars = tiny_chars,
                                     .char_count = sizeof(tiny_chars) / sizeof(tiny_chars[0])};

// ============================================================================
// Destructive Overlap Functions (for Tiny font)
// ============================================================================

// Character priority for destructive overlap
static int char_priority(char c) {
    if (c == ' ') return 0;                          // Space - lowest priority (always loses)
    if (c == '|' || c == '/' || c == '\\') return 1; // Structural characters
    if (c == '_' || c == '-' || c == '(' || c == ')') return 2; // Horizontal/curves - aggressive
    return 3; // Letters, digits - highest priority
}

// Destructive merge: new character can overwrite old character based on priority
static void merge_lines_destructive(char *dest, const char *new_char, int overlap) {
    if (!new_char || !*new_char) return;

    size_t dest_len = strlen(dest);

    if (overlap > 0 && dest_len >= (size_t)overlap) {
        // For each overlapping column, pick the "winner"
        for (int i = 0; i < overlap && new_char[i]; i++) {
            int dest_idx = dest_len - overlap + i;
            char old_c = dest[dest_idx];
            char new_c = new_char[i];

            // Winner is character with higher priority
            // Equal priority: new character wins (allows cutting in)
            if (char_priority(new_c) >= char_priority(old_c)) {
                dest[dest_idx] = new_c; // New char OVERWRITES old char!
            }
            // If old has higher priority, keep old (do nothing)
        }

        // Append the rest of new_char (after overlap region)
        if (overlap < (int)strlen(new_char)) {
            strcat(dest, new_char + overlap);
        }
    } else {
        // No overlap, just concatenate
        strcat(dest, new_char);
    }
}

// ============================================================================
// Font Registry
// ============================================================================

static const FigFontDef *fonts[] = {
    [FIGLET_STANDARD] = &standard_font,
    [FIGLET_SMALL] = &small_font,
    [FIGLET_TINY] = &tiny_font,
};

// ============================================================================
// Rendering Engine
// ============================================================================

static const FigChar *find_char(const FigFontDef *font, int c) {
    // For fonts that don't support lowercase, convert to uppercase
    // Tiny font supports mixed case, so check if lowercase exists first
    const FigChar *exact_match = NULL;
    const FigChar *upper_match = NULL;

    for (int i = 0; i < font->char_count; i++) {
        if (font->chars[i].c == c) {
            exact_match = &font->chars[i];
            break;
        }
        // Track uppercase version as fallback
        if (font->chars[i].c == toupper(c)) {
            upper_match = &font->chars[i];
        }
    }

    // Return exact match if found, otherwise uppercase fallback
    return exact_match ? exact_match : upper_match;
}

int figlet_font_height(FigletFont font) {
    if (font < 0 || font >= FIGLET_COUNT || !fonts[font]) {
        return 0;
    }
    return fonts[font]->height;
}

int figlet_is_supported(int c, FigletFont font) {
    if (font < 0 || font >= FIGLET_COUNT || !fonts[font]) {
        return 0;
    }
    return find_char(fonts[font], c) != NULL;
}

char *figlet_render(const char *text, FigletFont font) {
    if (!text || font < 0 || font >= FIGLET_COUNT || !fonts[font]) {
        return NULL;
    }

    const FigFontDef *f = fonts[font];
    size_t text_len = strlen(text);

    if (text_len == 0) {
        return strdup("");
    }

    // Calculate total width needed
    int total_width = 0;
    for (size_t i = 0; i < text_len; i++) {
        const FigChar *fc = find_char(f, text[i]);
        if (!fc) {
            // Unsupported character - use space
            fc = find_char(f, ' ');
        }
        if (fc) {
            total_width += fc->width;
        }
    }

    // Allocate line buffers
    char **lines = malloc(f->height * sizeof(char *));
    if (!lines) return NULL;

    for (int row = 0; row < f->height; row++) {
        lines[row] = malloc(total_width + 1);
        if (!lines[row]) {
            // Cleanup on error
            for (int j = 0; j < row; j++) {
                free(lines[j]);
            }
            free(lines);
            return NULL;
        }
        lines[row][0] = '\0';
    }

    // Build each line by concatenating character lines
    for (size_t i = 0; i < text_len; i++) {
        const FigChar *fc = find_char(f, text[i]);
        if (!fc) {
            fc = find_char(f, ' ');
        }

        if (fc) {
            for (int row = 0; row < f->height; row++) {
                // Check if we should use destructive overlap (Tiny font only)
                int overlap = 0;

                // Tiny font uses aggressive 1-column destructive overlap
                if (strcmp(f->name, "Tiny") == 0 && i > 0) {
                    overlap = 1;
                }

                // Use destructive merge for Tiny font, simple concat for others
                if (overlap > 0) {
                    merge_lines_destructive(lines[row], fc->lines[row], overlap);
                } else {
                    strcat(lines[row], fc->lines[row]);
                }
            }
        }
    }

    // Calculate total output size
    size_t output_size = 0;
    for (int row = 0; row < f->height; row++) {
        output_size += strlen(lines[row]) + 1; // +1 for newline
    }
    output_size++; // +1 for null terminator

    // Assemble output
    char *output = malloc(output_size);
    if (!output) {
        for (int row = 0; row < f->height; row++) {
            free(lines[row]);
        }
        free(lines);
        return NULL;
    }

    output[0] = '\0';
    for (int row = 0; row < f->height; row++) {
        strcat(output, lines[row]);
        if (row < f->height - 1) {
            strcat(output, "\n");
        }
        free(lines[row]);
    }
    free(lines);

    return output;
}
