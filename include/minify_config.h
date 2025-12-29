#pragma once

//
// FORWARD DECLARATIONS
//

typedef struct StateGUI StateGUI;


//
// DEFINES
//


//
// LOCAL VARIABLES
//

static const wchar_t* const prohibitedNamesJS[] = {L"do", L"if", L"in", L"for" };
static constexpr wchar_t LETTERS[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static constexpr size_t LETTERS_CNT = _countof(LETTERS) - 1;
static constexpr wchar_t ALPHANUM[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
static constexpr size_t ALPHANUM_CNT = _countof(ALPHANUM) - 1;

//
// STRUCTS
//

typedef struct MiniCfg
{
	bool alreadyPresentGUI;
	bool flagNoGUI;
	wchar_t prevPath[MAX_PATH];
	wchar_t inPath[MAX_PATH];
    uint8_t inputType;
    bool defaultToPrevFile;
    bool mangle;
    uint8_t outFile;
    wchar_t stripSeg[MAX_PATH];
    wchar_t outPath[MAX_PATH];
} MiniCfg;

// Context structure to hold the shuffled alfabet to generate mangled names.
typedef struct NameGenerator
{
    wchar_t startWchars[52]; // a-z, A-Z
    wchar_t otherWchars[64];   // a-z, A-Z, 0-9, -, _
} NameGenerator;

//
// GLOBAL VARIABLES
//


//
// FUNCTIONS
//

void loadMinificationSettings(MiniCfg*, StateGUI*);
bool parseArgumentsCLI(MiniCfg*, StateGUI*, PWSTR);
void getMangledNameByIndex(uint64_t, wchar_t*);