
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

// A single CSS Rule Set: "div.container { background: green; color: red; }"
typedef struct CssRule
{
    char* selector; // "div.container"
    char* declarationBlock;     // "background: green; color: red;"
} CssRule;

// A Group of Rule Sets (e.g., inside @media screen {...}).
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
} CssContext;

//
// CONFIGURATION CONSTANTS
//

static constexpr size_t ARENA_BLOCK_SIZE = 4096; // 4KB chunks to allocate at a time for found IDs and classes names.

//
// FUNCTIONS
//

CssContext* cssCreateContext()
{
    CssContext* ctx = calloc(1, sizeof(CssContext));
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
            // Free the query signature string.
            if (ctx->groups[i].querySignature != nullptr) free(ctx->groups[i].querySignature);
            
            // Check if the rules array exists.
            if (ctx->groups[i].rules != nullptr)
            {
                for (size_t j = 0; j < ctx->groups[i].ruleCount; j++)
                {
                    // Free selector string.
                    if (ctx->groups[i].rules[j].selector != nullptr) free(ctx->groups[i].rules[j].selector);
                    
                    // Free declaration block string.
                    if (ctx->groups[i].rules[j].declarationBlock != nullptr) free(ctx->groups[i].rules[j].declarationBlock);
                }

                // Free the rules array itself.
                free(ctx->groups[i].rules);
            }
        }
        // Free the groups array itself.
        free(ctx->groups);
    }

    // Free the context struct.
    free(ctx);
}

// Finds an existing at-rule group or creates a new one
static CssAtRuleGroup* internalGetGroup(_Inout_ CssContext* ctx, _In_z_ const char* sig)
{
    // 1. Search existing
    for (size_t i = 0; i < ctx->groupCount; i++)
    {
        if (strcmp(ctx->groups[i].querySignature, sig) == 0)
        {
            return &ctx->groups[i];
        }
    }

    // 2. Create new
    if (ctx->groupCount == ctx->groupCap)
    {
        size_t newCap = (ctx->groupCap == 0) ? 4 : ctx->groupCap * 2;
        CssAtRuleGroup* tmp = realloc(ctx->groups, newCap * sizeof(CssAtRuleGroup));
        if (!tmp) return nullptr;
        ctx->groups = tmp;
        ctx->groupCap = newCap;
    }
    
    CssAtRuleGroup* grp = &ctx->groups[ctx->groupCount];
    
    char* dupSig = _strdup(sig);
    if (!dupSig) return nullptr; // Fail if string duplication fails

    grp->querySignature = dupSig;
    grp->rules = nullptr;
    grp->ruleCount = 0;
    grp->ruleCap = 0;

    ctx->groupCount++; 
    return grp;
}

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

// Helper to see if space is required inside a selector or declaration block.
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

static void internalRuleSetParse(_In_ const char* src, _In_ size_t len, _Out_ char* dest)
{
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
            if (internalSpaceIsOptional(src[i + 1])) continue; // Skip adjacent spaces.
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

// Save CSS selector and declaration block.
static void internalStoreRuleSet(_Inout_ CssAtRuleGroup* grp, _In_ const char* sel, _In_ size_t selLen, _In_ const char* declarationBlock, _In_ size_t declarationBlockLen)
{
    CssRule* r = internalGetNextRuleSlot(grp);
    if (r == nullptr) return;
    
    r->selector = malloc(selLen + 1);
    if (r->selector != nullptr)
    {
        internalRuleSetParse(sel, selLen, r->selector);
    }
    else
    {
        appLogError("Failed to allocate memory for a rule set selector.");
        return;
    }

    r->declarationBlock = malloc(declarationBlockLen + 1);
    if (r->declarationBlock != nullptr)
    {
        internalRuleSetParse(declarationBlock, declarationBlockLen, r->declarationBlock);
    }
    else
    {
        appLogError("Failed to allocate memory for a declaration block.");
        free(r->selector);
        r->selector = nullptr;
        return;
    }
}

// To merge the results of parsing different CSS files.
void cssMergeContexts(CssContext* dest, CssContext* src)
{
    if (dest == nullptr || src == nullptr) return;

    for (size_t i = 0; i < src->groupCount; i++)
    {
        CssAtRuleGroup* srcGrp = &src->groups[i];
        
        // Find corresponding bucket in destination (or create it).
        CssAtRuleGroup* destGrp = internalGetGroup(dest, srcGrp->querySignature);
        if (destGrp == nullptr) continue;
        
        // Move rules from src to dest.
        for (size_t j = 0; j < srcGrp->ruleCount; j++)
        {
            CssRule* srcRule = &srcGrp->rules[j];
            
            // Get a blank slot in the destination.
            CssRule* destRule = internalGetNextRuleSlot(destGrp);
            if (destRule == nullptr) break;

            // Transfer ownership.
            destRule->selector = srcRule->selector;
            destRule->declarationBlock = srcRule->declarationBlock;

            // Nullify source. When cssDestroyContext(src) is called, 
            // the memory pointed at by these pointers must not get free().
            srcRule->selector = nullptr;
            srcRule->declarationBlock = nullptr;
        }
    }
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
    internalRuleSetParse(data + i, sigLen, localSig); // TODO: Extract and match common at-rule specs to merge them even if they are spelled in a different way.

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
        combinedSig = _strdup(localSig);
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
                internalStoreRuleSet(grp, 
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
        if (grp) internalStoreRuleSet(grp, data + selStart, stmtEnd - selStart, "", 0);
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

static bool internalEnsureArenaSpace(_Inout_ SelectorList* list, _In_ size_t required)
{
    if (list->curr == nullptr || (list->curr->used + required > list->curr->cap))
    {
        // Allocate new node.
        CssArenaNode* node = malloc(sizeof(CssArenaNode));
        if (node == nullptr) return false;

        // Allocate the raw buffer.
        node->data = malloc(ARENA_BLOCK_SIZE);
        if (node->data == nullptr) { free(node); return false; }

        node->used = 0;
        node->cap = ARENA_BLOCK_SIZE;
        node->next = nullptr;

        // Link it.
        if (list->curr != nullptr)
        {
            list->curr->next = node;
        }
        else
        {
            list->head = node;
        }
        list->curr = node;
    }

    return true;
}

// Helper to append selector string to a specific list.
static void internalListAppend(_Inout_ SelectorList* list, _In_ const char* str)
{
    size_t strLen = strlen(str);
    size_t required = strLen + 1; // +1 for null terminator

    // 1. Ensure we have an arena block with space.
    if (!internalEnsureArenaSpace(list, required)) return;

    // 2. Ensure Pointer Array has space.
    if (!internalEnsurePointerArraySpace(list)) return;

    // 3. Copy String into Arena
    char* dest = list->curr->data + list->curr->used;
    memcpy(dest, str, strLen);
    dest[strLen] = 0;
    list->curr->used += required;

    // 4. Store Pointer
    list->items[list->count++] = dest;
}

// Helper to check if a specific class or id name exists in a list.
static bool internalListContains(_In_ SelectorList* list, _In_ const char* name, _In_ size_t len)
{
    if (!list || !list->items) return false;
    for (size_t i = 0; i < list->count; i++)
    {
        // We use check length first to avoid full strcmp if not needed
        const char* item = list->items[i];
        if (strncmp(item, name, len) == 0 && item[len] == '\0')
        {
            return true;
        }
    }
    return false;
}

void cssRecordSelector(SetOfClassesAndIDs* set, const char* name, bool isId, bool isAbove)
{
    // Calculate length once for all checks.
    size_t nameLen = strlen(name);

    if (isAbove)
    {
        // We only check the specific "Above" list.
        SelectorList* target = isId ? &set->idsAbove : &set->classesAbove;

        // Only add if it doesn't already exist.
        if (!internalListContains(target, name, nameLen)) internalListAppend(target, name);
    }
    else
    {
        SelectorList* aboveList = isId ? &set->idsAbove : &set->classesAbove;
        if (internalListContains(aboveList, name, nameLen)) return;

        SelectorList* underList = isId ? &set->idsUnder : &set->classesUnder;
        if (!internalListContains(underList, name, nameLen)) internalListAppend(underList, name);
    }
}

static void internalListFree(_Inout_ SelectorList* list)
{
    // Free the pointer array.
    if (list->items)
    {
        free(list->items);
        list->items = nullptr;
    }

    // Free the memory blocks
    CssArenaNode* node = list->head;
    while (node != nullptr)
    {
        CssArenaNode* next = node->next;
        if (node->data) free(node->data);
        free(node);
        node = next;
    }
    
    list->head = nullptr;
    list->curr = nullptr;
    list->count = 0;
    list->cap = 0;
}

// Called from html.c cleanup.
void cssFreeCriticalSet(SetOfClassesAndIDs* set)
{
    internalListFree(&set->classesAbove);
    internalListFree(&set->idsAbove);
    internalListFree(&set->classesUnder);
    internalListFree(&set->idsUnder);
}

static bool internalIsCritical(_In_ const char* selector, _In_ SetOfClassesAndIDs* set)
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

        // Scan the current segment
        while (*cursor && *cursor != ',')
        {
            // Look for Class (.) or ID (#) start
            if (*cursor == '.' || *cursor == '#')
            {
                bool isId = (*cursor == '#');
                
                // Check if this is inside a :not(...) pseudo-class
                // We look backwards from current position.
                // 1. We need at least 5 chars back: ":not("
                bool isInsideNot = false;
                if (cursor - segmentStart >= 5)
                {
                    // Simple check: looking for ":not(" immediately preceding
                    // Note: This is a basic check. It won't catch ":not( div " (spaces).
                    if (strncmp(cursor - 5, ":not(", 5) == 0)
                    {
                        isInsideNot = true;
                    }
                }

                cursor++; // Move past . or #
                
                // Extract the name
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
                    if (internalListContains(underList, nameStart, nameLen))
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
            
            DynBuf* target = internalIsCritical(r->selector, set) ? &grpAbove : &grpUnder;
            
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
    CssContext* ctx = cssCreateContext();
    if (!ctx)
    {
        free(pD);
        if (args->mainParsingThread) parserCommonFinished(args->pStateGUI, NULL, 0);
        free(args);
        return 0;
    }
    internalParseRawCSS(ctx, "", pD, args->len);
    free(pD);

    // If we are the MAIN parsing thread (Standalone mode), we output everything to Above.
    if (args->mainParsingThread)
    {
        CssOutputs out = cssGenerateSplitOutput(ctx, NULL); 
        // For standalone, logic dictates we just dump everything.
        // We combine above and under (if any generated)
        
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