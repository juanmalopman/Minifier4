#pragma once

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

//
// STRUCTS
//

// Arena allocations are used to store at-rules, selector lists and declaration blocks.
typedef struct CssArenaBlock
{
    char* data;
    size_t used;
    size_t cap;
    struct CssArenaBlock* next;
} CssArenaBlock;

// The manager struct for the arena
typedef struct CssArena
{
    CssArenaBlock* head;
    CssArenaBlock* curr; // The block we are currently writing to.
} CssArena;

typedef struct SelectorList
{
    char** items;        // Array of pointers to arena indexes.
    size_t count;        // Number of pointers.
    size_t cap;          // Capacity of pointer array.
    
    CssArena arena;      // Shared arena logic. First parsing CSS and then merging contexts.
} SelectorList;

typedef struct SetOfClassesAndIDs
{
    SelectorList classesAbove;
    SelectorList idsAbove;
    SelectorList classesUnder;
    SelectorList idsUnder;

    SRWLOCK lock;
} SetOfClassesAndIDs;

// Structure to hold generated strings for final stitching.
typedef struct CssOutputs
{
    char* aboveCSS;
    size_t aboveLen;
    char* underCSS;
    size_t underLen;
} CssOutputs;

// Opaque pointer. The main context holding multiple at-rule groups.
typedef struct CssContext CssContext;

//
// FUNCTION PROTOTYPES
//

DWORD WINAPI cssSpawnThread(_Inout_ LPVOID lpParam);
CssContext* cssCreateContext(_In_ bool mangle);
void cssDestroyContext(_In_ CssContext* ctx);
void cssMergeContexts(_Inout_ CssContext* dest, _Inout_ CssContext* src);
size_t cssRecordSelector(_Inout_ SetOfClassesAndIDs* set, _In_z_ const char* name, _In_ bool isId, _In_ bool isAbove);
void cssInitCriticalSet(_Inout_ SetOfClassesAndIDs* set);
void cssFreeCriticalSet(_Inout_ SetOfClassesAndIDs* set);
void cssOutFree(_Inout_ CssOutputs* out);
CssOutputs cssGenerateSplitOutput(_In_ CssContext* ctx, _In_ SetOfClassesAndIDs* criticalSet);