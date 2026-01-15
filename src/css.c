
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "css.h"
#include "parser_common.h"
#include "app_logging.h"

//
// STRUCTS
//

// Dynamic Buffer for string building from the array of at-rules and rule sets.
typedef struct DynBuf
{
    char* data;
    size_t len;
    size_t cap;
} DynBuf;

// A single CSS Rule Set: "div.container { background: green; color: red; }".
typedef struct CssRule
{
    char* selector; // "div.container"
    char* declarationBlock;     // "background: green; color: red;"
} CssRule;

// A Group of Rule Sets (e.g., inside @media screen {...} or in global space).
typedef struct CssAtRuleGroup
{
    char* querySignature; // "@media screen and (min-width: 900px)" or "" for global.
    CssRule* rules;
    size_t ruleCount;
    size_t ruleCap;
} CssAtRuleGroup;

// The main context holding multiple at-rule groups.
typedef struct CssContext
{
    CssAtRuleGroup* groups;
    size_t groupCount;
    size_t groupCap;
    SetOfClassesAndIDs* pCritSet;
    CssArena arena; // The unified memory store for all rules in this context.
    bool mangle;
} CssContext;

//
// CONFIGURATION CONSTANTS
//

static constexpr size_t ARENA_BLOCK_SIZE = 8192; // 8KB chunks to allocate at a time for IDs, classes, declaration blocks...
static constexpr size_t INVALID_INDEX = (size_t)-1;

//
// FUNCTIONS
//

static void internalArenaInit(_Inout_ CssArena* arena)
{
    arena->head = nullptr;
    arena->curr = nullptr;
}

static char* internalArenaAlloc(_Inout_ CssArena* arena, _In_ size_t size)
{
    // 1. Check if current block has space.
    if (arena->curr != nullptr)
    {
        if (arena->curr->used + size <= arena->curr->cap)
        {
            char* ptr = arena->curr->data + arena->curr->used;
            arena->curr->used += size;
            return ptr;
        }
    }

    // 2. Allocation needed. Ensure new block is large enough.
    size_t allocSize = (size > ARENA_BLOCK_SIZE) ? size : ARENA_BLOCK_SIZE;
    
    CssArenaBlock* node = malloc(sizeof(CssArenaBlock));
    if (node == nullptr)
    {
        appLogError("Failed to allocate memory in CSS arena allocation.");
        return nullptr;
    }

    node->data = malloc(allocSize);
    if (node->data == nullptr)
    {
        free(node);
        appLogError("Failed to allocate memory in CSS arena allocation.");
        return nullptr;
    }

    node->used = size;
    node->cap = allocSize;
    node->next = nullptr;

    // 3. Link.
    if (arena->curr != nullptr)
    {
        arena->curr->next = node;
    }
    else
    {
        arena->head = node;
    }
    arena->curr = node;

    return node->data;
}

static void internalArenaFree(_Inout_ CssArena* arena)
{
    CssArenaBlock* node = arena->head;
    while (node != nullptr)
    {
        CssArenaBlock* next = node->next;
        if (node->data) free(node->data);
        free(node);
        node = next;
    }
    arena->head = nullptr;
    arena->curr = nullptr;
}

// Transfers ownership of Src blocks to Dest. Src becomes empty.
static void internalArenaMerge(_Inout_ CssArena* dest, _Inout_ CssArena* src)
{
    if (src->head == nullptr) return; // Nothing to merge.

    if (dest->curr == nullptr)
    {
        // Dest is empty, just take src.
        dest->head = src->head;
        dest->curr = src->curr;
    }
    else
    {
        // Append src chain to end of dest chain.
        dest->curr->next = src->head;
        dest->curr = src->curr;
    }

    // Zero out src.
    src->head = nullptr;
    src->curr = nullptr;
}

CssContext* cssCreateContext(bool mangle)
{
    CssContext* ctx = calloc(1, sizeof(CssContext));
    if (ctx)
    {
        internalArenaInit(&ctx->arena);
        ctx->mangle = mangle; // The mangle argument allows CssContext to stay as an opaque pointer to the html module.
    }
    else appLogError("Error allocating CSS context stucture.");
    return ctx;
}

void cssDestroyContext(CssContext* ctx)
{
    if (ctx == nullptr) return;

    // Check if the groups array exists.
    if (ctx->groups != nullptr)
    {
        for (size_t i = 0; i < ctx->groupCount; i++)
        {            
            // The rules array itself is the only thing separate from the arena
            // (the array of structs, not the strings inside them).
            if (ctx->groups[i].rules != nullptr)
            {
                free(ctx->groups[i].rules);
            }
        }

        // Free the groups array itself.
        free(ctx->groups);
    }

    // Free the monolithic arena which holds:
    // 1. All rule selectors (possibly mangled).
    // 2. All rule declaration blocks.
    // 3. All group query signatures (@media...).
    internalArenaFree(&ctx->arena);

    // Free the context struct.
    free(ctx);
}

// Finds an existing at-rule group or creates a new one
static CssAtRuleGroup* internalGetGroup(_Inout_ CssContext* ctx, _In_z_ const char* sig)
{
    // 1. Search existing.
    for (size_t i = 0; i < ctx->groupCount; i++)
    {
        if (strcmp(ctx->groups[i].querySignature, sig) == 0)
        {
            return &ctx->groups[i];
        }
    }

    // 2. Create new.
    if (ctx->groupCount == ctx->groupCap)
    {
        size_t newCap = (ctx->groupCap == 0) ? 4 : ctx->groupCap * 2;
        CssAtRuleGroup* tmp = realloc(ctx->groups, newCap * sizeof(CssAtRuleGroup));
        if (!tmp) return nullptr;
        ctx->groups = tmp;
        ctx->groupCap = newCap;
    }
    
    CssAtRuleGroup* grp = &ctx->groups[ctx->groupCount];
    
    // Store the signature in the Arena.
    size_t sigLen = strlen(sig);
    char* storedSig = internalArenaAlloc(&ctx->arena, sigLen + 1);
    
    if (storedSig)
    {
        memcpy(storedSig, sig, sigLen + 1);
    }
    else
    {
        appLogError("Arena allocation failed for at-rule signature.");
        static char empty[] = ""; // TODO: Handle crash gracefully.
        storedSig = empty;
    }

    grp->querySignature = storedSig;
    grp->rules = nullptr;
    grp->ruleCount = 0;
    grp->ruleCap = 0;

    ctx->groupCount++; 
    return grp;
}

// Forward declarations for helpers.
static inline bool internalSpaceIsOptional(_In_ char c);
static void internalRuleSetParse(_In_ const char* src, _In_ size_t len, _Out_ char* dest);
static inline bool internalIsIdentifierChar(char c);
static bool internalFindInList(_In_ const SelectorList* list, _In_ const char* str, _In_ size_t len, _Out_ size_t* outIdx);

// Helper: Growing the array without setting data.
static CssRule* internalGetNextRuleSlot(_Inout_ CssAtRuleGroup* grp)
{
    if (grp->ruleCount == grp->ruleCap)
    {
        size_t newCap = (grp->ruleCap == 0) ? 16 : grp->ruleCap * 2;
        CssRule* tmp = realloc(grp->rules, newCap * sizeof(CssRule));
        if (tmp == nullptr) return nullptr;
        grp->rules = tmp;
        grp->ruleCap = newCap;
    }
    return &grp->rules[grp->ruleCount++];
}

// Helper to see if space is required inside a selector, declaration block or atomic at-rule signature.
static inline bool internalSpaceIsOptional(_In_ char c)
{
    if (parserCommonIsSpace(c)) return true;
    
    switch (c)
    {
        case '(': case ')':
        case '<': case '>':
        case '[': case ']':
        case '=':
        case ',':
        case ':': 
        case ';':
        case '@': // Atomic at-rules are treated as selectors.
            return true;
        default: 
            return false;
    }
}

// Takes a normalized selector string (in src) and writes the mangled version to dest.
// Dest must be large enough (max selector length).
static void internalMangleBuffer(_In_ CssContext* ctx, _In_ const char* src, _Out_ char* dest)
{
    if (!ctx->mangle || !ctx->pCritSet)
    {
        strcpy(dest, src);
        return;
    }

    size_t r = 0; 
    size_t w = 0;
    
    while (src[r] != '\0')
    {
        // Detect Class (.) or ID (#)
        if (src[r] == '.' || src[r] == '#')
        {
            char type = src[r];
            size_t start = r + 1;
            size_t end = start;

            // Find end of identifier
            while (internalIsIdentifierChar(src[end])) end++;

            size_t idLen = end - start;
            if (idLen > 0)
            {
                bool found = false;
                size_t foundIdx = 0;
                size_t mangleIdx = 0;

                SelectorList* listAbove = (type == '.') ? &ctx->pCritSet->classesAbove : &ctx->pCritSet->idsAbove;
                SelectorList* listUnder = (type == '.') ? &ctx->pCritSet->classesUnder : &ctx->pCritSet->idsUnder;

                if (internalFindInList(listAbove, &src[start], idLen, &foundIdx))
                {
                    mangleIdx = foundIdx;
                    found = true;
                }
                else if (internalFindInList(listUnder, &src[start], idLen, &foundIdx))
                {
                    mangleIdx = listAbove->count + foundIdx;
                    found = true;
                }

                if (found)
                {
                    char mangledBuf[16] = {0}; // Base54 results are short
                    parserCommonGetMangled((int)mangleIdx, mangledBuf);
                    size_t mangledLen = strlen(mangledBuf);

                    dest[w++] = type; // Keep prefix
                    memcpy(&dest[w], mangledBuf, mangledLen);
                    w += mangledLen;
                    r = end;
                    continue;
                }
            }
        }
        
        // Copy char as is
        dest[w++] = src[r++];
    }
    dest[w] = '\0';
}

static void internalRuleSetParse(_In_ const char* src, _In_ size_t len, _Out_ char* dest)
{
    size_t i = 0;
    size_t o = 0;
    for (; i < len; i++)
    {
        if (src[i] == '/' && i + 1 < len && src[i + 1] == '*') {
            for (; i < len; ++i) if (src[i] == '/' && src[i - 1] == '*') break;
            continue;
        }
        if (!parserCommonIsSpace(src[i])) {
            dest[o++] = src[i];
            continue;
        }
        if (i + 1 < len) {
            if (internalSpaceIsOptional(src[i + 1])) continue;
        }
        else if (src[i] != '/') continue; 
        else dest[o++] = src[i]; 

        if (o) {
            if (internalSpaceIsOptional(dest[o - 1])) continue;
        }
        else continue; 
        dest[o++] = src[i];
    }
    dest[o] = '\0';
}

// Save CSS selector and declaration block.
static void internalStoreRuleSet(_In_ CssContext* ctx, _Inout_ CssAtRuleGroup* grp, 
                                 _In_ const char* sel, _In_ size_t selLen, 
                                 _In_ const char* declarationBlock, _In_ size_t declarationBlockLen)
{
    CssRule* r = internalGetNextRuleSlot(grp);
    if (r == nullptr) return;

    // 1. Process Selector
    // We use a stack buffer. CSS selectors are rarely huge, but if they exceed this,
    // we could fallback to malloc, but for minification tools 8KB is plenty for a single selector group.
    char tempSel[8192]; 
    char mangledSel[8192];
    
    // Normalize whitespace (src -> tempSel)
    size_t safeSelLen = (selLen < 8191) ? selLen : 8191;
    internalRuleSetParse(sel, safeSelLen, tempSel);
    
    // Mangle (tempSel -> mangledSel)
    // If mangling is off, this just copies tempSel to mangledSel.
    internalMangleBuffer(ctx, tempSel, mangledSel);
    
    size_t finalSelLen = strlen(mangledSel);
    
    // Allocate exactly what we need from Arena
    r->selector = internalArenaAlloc(&ctx->arena, finalSelLen + 1);
    if (r->selector)
    {
        memcpy(r->selector, mangledSel, finalSelLen + 1);
    }
    else
    {
        appLogError("Arena allocation failed for selector.");
        return;
    }

    // 2. Process Declaration Block
    // Similar strategy: Normalize -> Store
    // Declaration blocks can be large (data URIs etc), so we use a heap temp buffer if needed
    // or just assume ruleSetParse reduces size. 
    // Let's alloc a temp heap buffer to be safe, normalize, then store permanently in arena.
    
    char* tempDecl = malloc(declarationBlockLen + 1);
    if (!tempDecl) return;

    internalRuleSetParse(declarationBlock, declarationBlockLen, tempDecl);
    size_t finalDeclLen = strlen(tempDecl);

    r->declarationBlock = internalArenaAlloc(&ctx->arena, finalDeclLen + 1);
    if (r->declarationBlock)
    {
        memcpy(r->declarationBlock, tempDecl, finalDeclLen + 1);
    }
    else
    {
        appLogError("Arena allocation failed for decl block.");
    }
    
    free(tempDecl);
}

// To merge the results of parsing different CSS files.
void cssMergeContexts(CssContext* dest, CssContext* src)
{
    if (dest == nullptr || src == nullptr) return;

    // 1. Merge At-Rules groups.
    for (size_t i = 0; i < src->groupCount; i++)
    {
        CssAtRuleGroup* srcGrp = &src->groups[i];
        CssAtRuleGroup* destGrp = internalGetGroup(dest, srcGrp->querySignature);
        if (destGrp == nullptr) continue;
        
        for (size_t j = 0; j < srcGrp->ruleCount; j++)
        {
            CssRule* srcRule = &srcGrp->rules[j];
            CssRule* destRule = internalGetNextRuleSlot(destGrp);
            if (destRule == nullptr) break;

            // Simple pointer copy. 
            // The actual data resides in src->arena (which we are about to move to dest).
            destRule->selector = srcRule->selector;
            destRule->declarationBlock = srcRule->declarationBlock;
        }
    }

    // 2. Merge Arenas.
    // Transfer ownership of all memory blocks from src to dest.
    internalArenaMerge(&dest->arena, &src->arena);
}

// Check if an at-rule implies selectors and descriptor blocks inside to be sorted above or under the fold.
static bool internalIsSplittableGroup(_In_ const char* start, _In_ size_t len)
{
    if (len < 2 || *start != '@') return false;
    const char* str = start + 1; // Skip '@'

    if (strncmp(str, "media", 5) == 0     && (parserCommonIsSpace(str[5]) || str[5] == '{')) return true;
    if (strncmp(str, "supports", 8) == 0  && (parserCommonIsSpace(str[8]) || str[8] == '{')) return true;
    if (strncmp(str, "layer", 5) == 0     && (parserCommonIsSpace(str[5]) || str[5] == '{')) return true;
    if (strncmp(str, "container", 9) == 0 && (parserCommonIsSpace(str[9]) || str[9] == '{')) return true;
    
    return false;
}

// Forward declaration needed for recursion.
static void internalParseRawCSS(_Inout_ CssContext* ctx, _In_ const char* currentSig, _In_ char* data, _In_ size_t len);

// Helper to see if space is required inside a non-atomic at-rule signature.
static inline bool internalSpaceIsOptionalAtRule(_In_ char c)
{
    if (parserCommonIsSpace(c)) return true;
    
    switch (c)
    {
        case ')':
        case '<': case '>':
        case '[': case ']':
        case '=':
        case ',':
        case ':': 
        case ';':
        case '@': // Atomic at-rules are treated as selectors.
            return true;
        default: 
            return false;
    }
}

static void internalAtRuleSignatureParse(_In_ const char* src, _In_ size_t len, _Out_ char* dest)
{
    // TODO: Make it so that at-rules meaning the same but spelled differently get merged.
    //       A brute force way could be enforcing a max-width before min width etc order.
    size_t i = 0;
    size_t o = 0;
    for (; i < len; i++)
    {
        if (src[i] == '/' && i + 1 < len && src[i + 1] == '*') // Comment detected.
        {
            for (; i < len; ++i) if (src[i] == '/' && src[i - 1] == '*') break;
            continue;
        }

        if (!parserCommonIsSpace(src[i]))
        {
            dest[o++] = src[i];
            continue;
        }

        if (i + 1 < len)
        {
            if (internalSpaceIsOptionalAtRule(src[i + 1])) continue; // Skip adjacent spaces.
                                                                     // The space before '(' of "and (max-something..." is preserved.
        }
        else if (src[i] != '/') continue; // Skip trailing space.
        else dest[o++] = src[i]; // A needed '/'.

        if (o)
        {
            if (internalSpaceIsOptional(dest[o - 1])) continue;
        }
        else continue; // Skip leading space

        dest[o++] = src[i];
    }

    dest[o] = '\0';
}

// Handles @media, @supports, etc, that might be nested one inside the other.
// Extracts signature, combines with parent, and recurses.
static void internalParseRecursiveGroup(
                        _Inout_ CssContext* ctx,
                        _In_ const char* parentSig,
                        _In_ char* data,
                        _In_ size_t len,
                        _Inout_ size_t* ioIdx,
                        _In_ size_t preludeEnd )
{
    // TODO: Implement max recursion depth.
    size_t i = *ioIdx;

    // 1. Extract Local Signature.
    size_t sigLen = preludeEnd - i;
    while (sigLen > 0 && parserCommonIsSpace((unsigned char)data[i + sigLen - 1])) sigLen--;
    
    char* localSig = malloc(sigLen + 1);
    if (!localSig)
    {
        appLogError("Failed to alloc memory for at-rule signature.");
        return;
    }

    // Copy, optimize and null terminate.
    internalAtRuleSignatureParse(data + i, sigLen, localSig);

    // 2. Combine with Parent Signature. // TODO: Implement a way to nest media queries instead of "duplicating" parent signatures in the output.
    char* combinedSig = nullptr;
    if (parentSig && *parentSig)
    {
        // Format: "ParentSig { LocalSig".
        size_t parentLen = strlen(parentSig);
        size_t combinedLen = parentLen + 3 + sigLen + 1; // " { " + null
        combinedSig = malloc(combinedLen);
        if (combinedSig)
        {
            sprintf_s(combinedSig, combinedLen, "%s { %s", parentSig, localSig);
        }
        else
        {
            appLogError("Failed to alloc memory for nested at-rule signature.");
            return;
        }
    }
    else
    {
        combinedSig = strdup(localSig);
    }
    free(localSig);

    // 3. Find Block Content.
    i = preludeEnd + 1; // Skip '{'.
    size_t contentStart = i;
    int depth = 1;

    while (i < len && depth > 0)
    {
        if (data[i] == '{') depth++;
        else if (data[i] == '}') depth--;
        i++;
    }

    // 4. Recurse if valid block found.
    if (depth == 0 && combinedSig) internalParseRawCSS(ctx, combinedSig, data + contentStart, (i - 1) - contentStart);

    if (combinedSig) free(combinedSig);

    *ioIdx = i; // Update main cursor.
}

// Handles both (Selector + Descriptor Block) or (non-splittable at-rules ("@keyframes", "@font-face")).
static void internalParseAtomicBlock(
                        _Inout_ CssContext* ctx,
                        _In_ const char* currentSig,
                        _In_ char* data,
                        _In_ size_t len,
                        _Inout_ size_t* ioIdx)
{
    size_t i = *ioIdx;
    size_t selStart = i;

    // 1. Find start of block
    while (i < len && data[i] != '{' && data[i] != ';') i++;

    // 2. Handle block found
    if (i < len && data[i] == '{')
    {
        size_t selEnd = i;
        // Trim trailing space from selector/prelude
        while (selEnd > selStart && parserCommonIsSpace((unsigned char)data[selEnd - 1])) selEnd--;

        i++; // Skip '{'
        size_t blockStart = i;
        
        // Find block end (respecting internal nesting like keyframes 0% {})
        int depth = 1;
        while (i < len && depth > 0)
        {
            if (data[i] == '{') depth++;
            else if (data[i] == '}') depth--;
            i++;
        }

        if (depth == 0) {
            CssAtRuleGroup* grp = internalGetGroup(ctx, currentSig ? currentSig : "");
            if (grp)
            {
                internalStoreRuleSet(ctx, grp, 
                    data + selStart, selEnd - selStart, 
                    data + blockStart, (i - 1) - blockStart);
            }
        }
    }
    // 3. Handle semicolon (e.g. @import "foo.css";)
    else if (i < len && data[i] == ';') 
    {
        size_t stmtEnd = i + 1; // Include the semicolon in the capture.
        
        CssAtRuleGroup* grp = internalGetGroup(ctx, currentSig ? currentSig : "");

        // Store the entire statement (e.g., '@import "foo.css";') as the selector.
        // Pass "" and 0 as the declaration block.
        if (grp) internalStoreRuleSet(ctx, grp, data + selStart, stmtEnd - selStart, "", 0);
        i++; // Advance past the semicolon
    }
    
    *ioIdx = i; // Update main cursor
}


static void internalParseRawCSS(_Inout_ CssContext* ctx, _In_ const char* currentSig, _In_ char* data, size_t len)
{
    size_t i = 0;
    
    while (i < len)
    {
        // Skip Whitespace.
        if (parserCommonIsSpace((unsigned char)data[i])) { i++; continue; }

        // Skip Comments.
        if (i + 1 < len && data[i] == '/' && data[i+1] == '*')
        {
            i += 2;
            while (i + 1 < len && !(data[i] == '*' && data[i+1] == '/')) i++;
            i += 2;
            continue;
        }

        bool handled = false;

        // Parse as a non-atomic at-rule.
        if (data[i] == '@') 
        {
            // Look ahead to see if it's a "SplittableGroup" containing selectors and descriptor blocks to
            // split between above and under the fold.
            size_t preludeEnd = i;
            while (preludeEnd < len && data[preludeEnd] != '{' && data[preludeEnd] != ';')
            {
                preludeEnd++;
            }

            if (preludeEnd < len && data[preludeEnd] == '{')
            {
                if (internalIsSplittableGroup(data + i, preludeEnd - i)) {
                    internalParseRecursiveGroup(ctx, currentSig, data, len, &i, preludeEnd);
                    handled = true;
                }
            }
        }

        // Parse as Atomic (Selector + Descriptor Block) or (non-splittable at-rules ("@keyframes", "@font-face")).
        if (!handled) internalParseAtomicBlock(ctx, currentSig, data, len, &i);
    }
}

static bool internalEnsurePointerArraySpace(_Inout_ SelectorList* list)
{
    if (list->count == list->cap)
    {
        size_t newCap = (list->cap == 0) ? 64 : list->cap * 2;
        char** tmp = realloc(list->items, newCap * sizeof(char*));
        if (tmp == nullptr) return false;
        list->items = tmp;
        list->cap = newCap;
    }

    return true;
}

static size_t internalListAppend(_Inout_ SelectorList* list, _In_ const char* str)
{
    size_t strLen = strlen(str);
    size_t required = strLen + 1;

    // 1. Ensure Pointer Array space
    if (!internalEnsurePointerArraySpace(list)) return INVALID_INDEX;

    // 2. Alloc from Arena
    char* dest = internalArenaAlloc(&list->arena, required);
    if (!dest) return INVALID_INDEX;

    memcpy(dest, str, strLen);
    dest[strLen] = 0;

    // 3. Store Pointer
    list->items[list->count++] = dest;
    return list->count - 1;
}

// Checks if a class or id is in the forwarded list.
// If the name to be checked is mangled and the list to be checked against is "under",
// offset must then be the "above" list count. Otherwise must be 0.
static size_t internalListContains(_In_ SelectorList* list, _In_ const char* name, _In_ size_t len, _In_ size_t offset, _In_ bool mangled)
{
    if (!list || !list->items) return INVALID_INDEX;
    if (mangled)
    {
        char item[10];
        for (size_t i = 0; i < list->count; i++)
        {
            parserCommonGetMangled(offset + i, item);
            if (strncmp(item, name, len) == 0 && item[len] == '\0') return i;
        }
    }
    else
    {
        for (size_t i = 0; i < list->count; i++)
        {
            const char* item = list->items[i];
            if (strncmp(item, name, len) == 0 && item[len] == '\0') return i;
        }
    }
    return INVALID_INDEX;
}

// Helper for finding exact match in list.
static bool internalFindInList(_In_ const SelectorList* list, _In_ const char* str, _In_ size_t len, _Out_ size_t* outIdx)
{
    for (size_t i = 0; i < list->count; i++)
    {
        const char* item = list->items[i];
        if (item && strlen(item) == len && memcmp(item, str, len) == 0)
        {
            *outIdx = i;
            return true;
        }
    }
    return false;
}

size_t cssRecordSelector(SetOfClassesAndIDs* set, const char* name, bool isId, bool isAbove)
{
    size_t nameLen = strlen(name);
    size_t index = INVALID_INDEX;

    if (isAbove)
    {
        SelectorList* target = isId ? &set->idsAbove : &set->classesAbove;
        // internalListContains is being called with an unmangled name, so 4th argument is irrelevant and last must be false.
        index = internalListContains(target, name, nameLen, 0, false);
        if (index == INVALID_INDEX) index = internalListAppend(target, name);
        return index;
    }
    else
    {
        SelectorList* aboveList = isId ? &set->idsAbove : &set->classesAbove;
        // internalListContains is being called with an unmangled name, so 4th argument is irrelevant and last must be false.
        index = internalListContains(aboveList, name, nameLen, 0, false);
        if (index != INVALID_INDEX) return index;

        SelectorList* underList = isId ? &set->idsUnder : &set->classesUnder;
        // internalListContains is being called with an unmangled name, so 4th argument is irrelevant and last must be false.
        index = internalListContains(underList, name, nameLen, 0, false);
        if (index == INVALID_INDEX) index = internalListAppend(underList, name);
        return index + aboveList->count;
    }
}

static void internalListFree(_Inout_ SelectorList* list)
{
    if (list->items)
    {
        free(list->items);
        list->items = nullptr;
    }
    // Free the arena.
    internalArenaFree(&list->arena);
    
    list->count = 0;
    list->cap = 0;
}

void cssFreeCriticalSet(SetOfClassesAndIDs* set)
{
    internalListFree(&set->classesAbove);
    internalListFree(&set->idsAbove);
    internalListFree(&set->classesUnder);
    internalListFree(&set->idsUnder);
}

static bool internalIsCritical(_In_ const char* selector, _In_ SetOfClassesAndIDs* set, _In_ bool mangled)
{
    if (!set) return true; // Default to critical if no data.

    const char* cursor = selector;
    
    // Iterate through comma-separated segments (e.g., "div.a, div.b").
    while (*cursor)
    {
        // Skip leading whitespaces.
        while (*cursor && parserCommonIsSpace((unsigned char)*cursor)) cursor++;
        if (*cursor == '\0') break;

        bool segmentIsCritical = true; // Assume it's critical until proved otherwise.
        const char* segmentStart = cursor;

        // Scan the current segment.
        while (*cursor && *cursor != ',')
        {
            // Look for Class (.) or ID (#) start.
            if (*cursor == '.' || *cursor == '#')
            {
                bool isId = (*cursor == '#');
                
                // Check if this is inside a :not(...) pseudo-class looking backwards from current position.
                // 1. We need at least 5 chars back: ":not(".
                bool isInsideNot = false;
                if (cursor - segmentStart >= 5)
                {
                    // Simple check: looking for ":not(" immediately preceding.
                    // TODO: Catch ":not( div " (with whitespaces).
                    if (strncmp(cursor - 5, ":not(", 5) == 0)
                    {
                        isInsideNot = true;
                    }
                }

                cursor++; // Move past '.' or '#'
                
                // Extract the name.
                const char* nameStart = cursor;
                size_t nameLen = 0;
                while (*cursor && (isalnum((unsigned char)*cursor) || *cursor == '-' || *cursor == '_'))
                {
                    cursor++;
                    nameLen++;
                }

                if (nameLen > 0 && !isInsideNot)
                {
                    // Is it in the "Under" list?
                    SelectorList* underList = isId ? &set->idsUnder : &set->classesUnder;
                    size_t countAbove = isId ? set->idsAbove.count : set->classesAbove.count;
                    // Checking an "under" list with internalListContains mandates the 4th argument "offset" to be countAbove.
                    if (internalListContains(underList, nameStart, nameLen, countAbove, mangled) != INVALID_INDEX)
                    {
                        segmentIsCritical = false;
                        
                        // Fast-forward to next comma
                        while (*cursor && *cursor != ',') cursor++;
                        break;
                    }
                }
                // Don't increment cursor here, the inner while loop did it.
                continue; 
            }
            
            cursor++;
        }

        // We finished one segment (or broke out because it was poisoned).
        // If this segment is still marked critical, the whole rule is valid/critical.
        if (segmentIsCritical) return true;

        // If we hit a comma, skip it and continue to the next segment
        if (*cursor == ',') cursor++;
    }

    // If we checked all segments and none were critical (all were poisoned), return false.
    return false;
}

static void internalDbInit(_Inout_ DynBuf* db)
{
    memset(db, 0, sizeof(DynBuf));
    db->data = malloc(256);

    if (db->data) 
    {
        db->cap = 256;
        db->data[0] = 0;
    }
    else
    {
        db->cap = 0;
        db->len = 0;
    }
}

static void internalDbAppend(DynBuf* db, const char* str, size_t n)
{
    if (!str || !n) return;

    // A. Check if buffer needs initialization or growth.
    if (!db->data || db->len + n + 1 >= db->cap)
    {
        size_t newCap;

        // 1. Determine starting capacity.
        if (!db->data)
        {
            // If pointer is NULL, ignore current db->cap and start fresh.
            newCap = 256;
        }
        else
        {
            // Existing data, double the current capacity
            newCap = db->cap; 
        }

        // 2. Grow until it fits the new string.
        // Check for overflow to prevent infinite loops.
        while (newCap > 0 && db->len + n + 1 >= newCap) 
        {
            newCap *= 2;
        }
        
        // If overflow wrapped newCap to 0 or small number, abort.
        if (newCap <= db->len + n + 1)
        {
            appLogError("Can't append more string CSS data. Buffer overflow.");
            return;
        }

        // 3. Reallocate.
        // Note: realloc(NULL, size) acts like malloc(size).
        char* t = realloc(db->data, newCap);
        if (!t)
        {
            appLogError("Can't append more string CSS data, realloc() failed.");
            return;
        }

        db->data = t;
        db->cap = newCap;
    }

    // B. Copy data
    memcpy(db->data + db->len, str, n);
    db->len += n;
    db->data[db->len] = 0;
}

static void internalDbFree(_Inout_ DynBuf* db)
{
    if(db->data) free(db->data);
    db->data = nullptr;
}

CssOutputs cssGenerateSplitOutput(CssContext* ctx, SetOfClassesAndIDs* set)
{
    DynBuf bufAbove, bufUnder;
    internalDbInit(&bufAbove); internalDbInit(&bufUnder);
    
    // Generate output by iterating groups.
    for (size_t i = 0; i < ctx->groupCount; i++)
    {
        CssAtRuleGroup* grp = &ctx->groups[i];
        
        // Temporary buffers for this specific at-rule group.
        DynBuf grpAbove, grpUnder;
        internalDbInit(&grpAbove); internalDbInit(&grpUnder);

        // Process rule sets.
        for (size_t j = 0; j < grp->ruleCount; j++)
        {
            CssRule* r = &grp->rules[j];
            
            DynBuf* target = internalIsCritical(r->selector, set, ctx->mangle) ? &grpAbove : &grpUnder;
            
            internalDbAppend(target, r->selector, strlen(r->selector));
            
            if (r->declarationBlock[0]) 
            {
                internalDbAppend(target, "{", 1);
                internalDbAppend(target, r->declarationBlock, strlen(r->declarationBlock));
                internalDbAppend(target, "}", 1);
            }
        }

        // Wrap in an at-rule signature if not in the global.
        bool isGlobal = (grp->querySignature[0] == 0);
        
        if (grpAbove.len > 0)
        {
            if (!isGlobal)
            {
                internalDbAppend(&bufAbove, grp->querySignature, strlen(grp->querySignature));
                internalDbAppend(&bufAbove, "{", 1);
            }
            internalDbAppend(&bufAbove, grpAbove.data, grpAbove.len);
            if (!isGlobal) internalDbAppend(&bufAbove, "}", 1);
        }

        if (grpUnder.len > 0) {
            if (!isGlobal) {
                internalDbAppend(&bufUnder, grp->querySignature, strlen(grp->querySignature));
                internalDbAppend(&bufUnder, "{", 1);
            }
            internalDbAppend(&bufUnder, grpUnder.data, grpUnder.len);
            if (!isGlobal) internalDbAppend(&bufUnder, "}", 1);
        }
        
        internalDbFree(&grpAbove);
        internalDbFree(&grpUnder);
    }

    CssOutputs out;
    out.aboveCSS = bufAbove.data;
    out.aboveLen = bufAbove.len;
    out.underCSS = bufUnder.data;
    out.underLen = bufUnder.len;
    return out;
}

void cssOutFree(CssOutputs* out)
{
    if (out == nullptr) return;
    if (out->aboveCSS != nullptr) free(out->aboveCSS);
    if (out->underCSS != nullptr) free(out->underCSS);
}

static inline bool internalIsIdentifierChar(char c)
{
    return isalnum((unsigned char)c) || c == '-' || c == '_';
}

DWORD WINAPI cssSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    char* pD = parserCommonGetPointerToUTF8(args->data, &args->len, args->isPath);
    if (!pD)
    {
        if (args->mainParsingThread) parserCommonFinished(args->pStateGUI, NULL, 0);
        free(args);
        return 0;
    }

    // Instead of generating a string immediately, we build the Context
    CssContext* ctx = cssCreateContext(args->mangle);
    if (!ctx)
    {
        free(pD);
        if (args->mainParsingThread) parserCommonFinished(args->pStateGUI, NULL, 0);
        free(args);
        return 0;
    }

    ctx->pCritSet = args->pCritSet;

    internalParseRawCSS(ctx, "", pD, args->len);

    free(pD);

    // If we are the MAIN parsing thread (Standalone mode), we output everything to Above.
    if (args->mainParsingThread)
    {
        CssOutputs out = cssGenerateSplitOutput(ctx, NULL); 
        // For standalone, logic dictates we just dump everything.
        // We combine above and under (if any generated).
        
        size_t total = out.aboveLen + out.underLen;
        char* finalBuf = malloc(total + 1);
        if (finalBuf)
        {
            char* ptr = finalBuf;
            if(out.aboveLen) { memcpy(ptr, out.aboveCSS, out.aboveLen); ptr += out.aboveLen; }
            if(out.underLen) { memcpy(ptr, out.underCSS, out.underLen); ptr += out.underLen; }
            *ptr = 0;
        }

        parserCommonFinished(args->pStateGUI, finalBuf, total);
        
        // Cleanup generated strings
        free(out.aboveCSS); free(out.underCSS);
        cssDestroyContext(ctx);
        free(args);
    } 
    else
    {
        // Helper mode: Return the CONTEXT struct, not a string.
        args->data = ctx;
    }

    return 0;
}