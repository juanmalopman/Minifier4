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

// Structures to track classes and IDs found above and under the fold in HTML.
typedef struct CssArenaNode
{
    char* data; // A single block of memory for strings of all classes and IDs.
    size_t used;
    size_t cap;
    struct CssArenaNode* next;
} CssArenaNode;
typedef struct SelectorList
{
    char** items;        // Array of pointers for fast iteration.
    size_t count;        // Number of pointers.
    size_t cap;          // Capacity of pointer array.
    
    CssArenaNode* head;  // Start of memory blocks.
    CssArenaNode* curr;  // Current block we are writing to.
} SelectorList;
typedef struct SetOfClassesAndIDs
{
    SelectorList classesAbove;
    SelectorList idsAbove;
    SelectorList classesUnder;
    SelectorList idsUnder;
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
CssContext* cssCreateContext();
void cssDestroyContext(_In_ CssContext* ctx);
void cssMergeContexts(_Inout_ CssContext* dest, _Inout_ CssContext* src);
void cssRecordSelector(_Inout_ SetOfClassesAndIDs* set, _In_z_ const char* name, _In_ bool isId, _In_ bool isAbove);
void cssFreeCriticalSet(_Inout_ SetOfClassesAndIDs* set);
void cssOutFree(_Inout_ CssOutputs* out);
CssOutputs cssGenerateSplitOutput(_In_ CssContext* ctx, _In_ SetOfClassesAndIDs* criticalSet);