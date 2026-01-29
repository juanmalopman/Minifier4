
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "parser_common.h"
#include "main_window.h"
#include "minify_config.h"
#include "app_logging.h"
#include "css.h"

//
// STRUCTS
//

typedef struct JsDynBuf
{
    char* data;
    size_t len;
    size_t cap;
} JsDynBuf;

// Internal context types for mangling decision.
typedef enum JsContextType
{
    CTX_NONE = 0,
    CTX_ID,         // Force mangle as ID.
    CTX_CLASS,      // Force mangle as Class.
    CTX_SELECTOR    // Detect based on '#' or '.' prefix.
} JsContextType;

//
// FUNCTIONS
//

static void internalDbInit(JsDynBuf* db)
{
    db->cap = 4096;
    db->len = 0;
    db->data = malloc(db->cap);
    if (db->data) db->data[0] = 0;
}

static void internalDbAppend(JsDynBuf* db, const char* str, size_t n)
{
    if (!db->data) return;
    if (db->len + n + 1 >= db->cap)
    {
        size_t newCap = db->cap;
        while (db->len + n + 1 >= newCap)
        {
            if (newCap > SIZE_MAX / 2) // Overflow protection
            {
                if (db->len + n + 1 >= SIZE_MAX)
                {
                    appLogError("Can't allocate more memory for JS output.");
                    return;
                }
                newCap = SIZE_MAX;
            }
            else
            {
                newCap *= 2;
            }
        }
        char* tmp = realloc(db->data, newCap);
        if (!tmp) return;
        db->cap = newCap;
        db->data = tmp;
    }
    memcpy(db->data + db->len, str, n);
    db->len += n;
    db->data[db->len] = 0;
}

static void internalDbAppendChar(JsDynBuf* db, char c)
{
    char tmp[2] = { c, 0 };
    internalDbAppend(db, tmp, 1);
}

static inline bool internalIsIdentifierChar(char c)
{
    return isalnum((unsigned char)c) || c == '-' || c == '_';
}

// Skips whitespace backwards. Returns new index.
static size_t internalSkipSpaceBack(const char* src, size_t i)
{
    while (i > 0 && parserCommonIsSpace(src[i])) i--;
    return i;
}

// Checks if the word ending at index 'i' matches 'target'.
// E.g. "...classList" at i (pointing to 't') matches "classList".
// Also ensures the character BEFORE the match is not alphanumeric (boundary check).
// Updates 'i' to point before the word on success.
static bool internalMatchWordBack(const char* src, size_t* i, const char* target)
{
    size_t tLen = strlen(target);
    if (*i < tLen - 1) return false;

    size_t startIdx = *i - (tLen - 1);
    
    // Check string match.
    if (strncmp(&src[startIdx], target, tLen) != 0) return false;

    // Check boundary (ensure we didn't match suffix of another word like 'notclassList').
    if (startIdx > 0 && internalIsIdentifierChar(src[startIdx - 1])) return false;

    *i = (startIdx > 0) ? startIdx - 1 : 0;
    return true;
}

static JsContextType internalSniffDirective(const char* src, size_t i)
{
    if (i == 0) return CTX_NONE;
    size_t end = i - 1;

    // Skip whitespace backwards.
    // This allows the directive to be placed on a previous line or separated by spaces.
    end = internalSkipSpaceBack(src, end);

    // Check for ID Directive.
    // sizeof includes the null terminator for string literals, so subtract 1.
    const size_t kIdLen = sizeof(idNextComment) - 1;

    if (end >= kIdLen - 1)
    {
        // Calculate start position of the potential comment.
        size_t start = end - kIdLen + 1;
        
        // Compare against the global configuration constant (case-insensitive).
        if (_strnicmp(&src[start], idNextComment, kIdLen) == 0)
        {
            return CTX_ID;
        }
    }

    // Check for Class Directive.
    const size_t kClassLen = sizeof(classNextComment) - 1;

    if (end >= kClassLen - 1)
    {
        size_t start = end - kClassLen + 1;
        
        if (_strnicmp(&src[start], classNextComment, kClassLen) == 0)
        {
            return CTX_CLASS;
        }
    }

    return CTX_NONE;
}

// This function analyzes the JS code preceding a string literal to determine.
// if that string contains an ID, a Class, or a CSS Selector.
static JsContextType internalSniffContext(const char* buf, size_t len)
{
    if (len == 0) return CTX_NONE;
    size_t i = len - 1;

    // 1. Skip whitespace before the string
    i = internalSkipSpaceBack(buf, i);

    // 2. Handle setAttribute("id|class", ...)
    // Pattern: setAttribute ( ... , 
    // We are currently at the comma (or whitespace before it).
    if (buf[i] == ',')
    {
        size_t tempI = (i > 0) ? i - 1 : 0;
        tempI = internalSkipSpaceBack(buf, tempI);
        
        // We expect the closing quote of the FIRST argument
        char quote = buf[tempI];
        if (quote == '\'' || quote == '"' || quote == '`')
        {
            // Scan back strictly for the start quote. 
            // We assume "id" or "class" are short and don't have escaped quotes for optimization.
            size_t endQuoteIdx = tempI;
            bool foundStart = false;
            size_t limit = (tempI > 20) ? tempI - 20 : 0; // Look back max 20 chars

            while (tempI > limit)
            {
                tempI--;
                if (buf[tempI] == quote) { foundStart = true; break; }
            }

            if (foundStart)
            {
                // Check the content of the first argument
                size_t keyLen = endQuoteIdx - tempI - 1;
                const char* key = &buf[tempI + 1];
                
                JsContextType potentialCtx = CTX_NONE;
                if (keyLen == 2 && strncmp(key, "id", 2) == 0) potentialCtx = CTX_ID;
                else if (keyLen == 5 && strncmp(key, "class", 5) == 0) potentialCtx = CTX_CLASS;

                if (potentialCtx != CTX_NONE)
                {
                    // Now verify the function name is setAttribute
                    tempI = (tempI > 0) ? tempI - 1 : 0;
                    tempI = internalSkipSpaceBack(buf, tempI);
                    if (buf[tempI] == '(')
                    {
                        tempI = (tempI > 0) ? tempI - 1 : 0;
                        tempI = internalSkipSpaceBack(buf, tempI);
                        if (internalMatchWordBack(buf, &tempI, "setAttribute")) return potentialCtx;
                    }
                }
            }
        }
        // If we hit comma but didn't match setAttribute logic, fall through to normal logic
        // (Unlikely to be valid JS syntax for other cases we care about, but safe to continue).
    }

    // 3. Normal Function/Method call: func( or method(
    if (buf[i] != '(' && buf[i] != '=') return CTX_NONE; // Must start with ( or = (assignment)
    
    if (buf[i] == '=') {
        // Simple assignment check could go here if needed (e.g. className = "...")
        // For now, focusing on function calls as requested.
        return CTX_NONE; 
    }

    // Move past '('
    i = (i > 0) ? i - 1 : 0;
    i = internalSkipSpaceBack(buf, i);

    // --- CHECK FOR SPECIFIC METHODS ---

    // ID Specific
    if (internalMatchWordBack(buf, &i, "getElementById")) return CTX_ID;

    // Class Specific
    if (internalMatchWordBack(buf, &i, "getElementsByClassName")) return CTX_CLASS;
    
    // Selectors (querySelector, closest, matches, jQuery's $)
    // We check these before checking the generic ".add" to avoid overlap issues
    size_t selectorCheckI = i; // Save state
    if (internalMatchWordBack(buf, &selectorCheckI, "querySelector")) return CTX_SELECTOR;
    selectorCheckI = i;
    if (internalMatchWordBack(buf, &selectorCheckI, "querySelectorAll")) return CTX_SELECTOR;
    selectorCheckI = i;
    if (internalMatchWordBack(buf, &selectorCheckI, "closest")) return CTX_SELECTOR;
    selectorCheckI = i;
    if (internalMatchWordBack(buf, &selectorCheckI, "matches")) return CTX_SELECTOR;
    selectorCheckI = i;
    if (internalMatchWordBack(buf, &selectorCheckI, "jQuery")) return CTX_SELECTOR;
    selectorCheckI = i;
    if (buf[selectorCheckI] == '$') { 
        // jQuery alias $, ensure previous char isn't identifier
        if (selectorCheckI == 0 || !internalIsIdentifierChar(buf[selectorCheckI - 1])) return CTX_SELECTOR; 
    }

    // jQuery Methods (.addClass, .removeClass, .hasClass)
    // i points to the last char of the function name
    size_t jqI = i;
    if (internalMatchWordBack(buf, &jqI, "addClass") || 
        internalMatchWordBack(buf, &jqI, "removeClass") ||
        internalMatchWordBack(buf, &jqI, "hasClass") || 
        internalMatchWordBack(buf, &jqI, "toggleClass"))
    {
        // These are almost exclusively class operations.
        // We could check for preceding '.' but it's safe to assume these specific names imply classes.
        return CTX_CLASS;
    }

    // --- CHECK FOR classList.* ---
    // Methods: add, remove, toggle, contains
    size_t clI = i;

    if (internalMatchWordBack(buf, &clI, "add") || 
        internalMatchWordBack(buf, &clI, "remove") || 
        internalMatchWordBack(buf, &clI, "toggle") || 
        internalMatchWordBack(buf, &clI, "contains"))
    {
        // Now we must confirm it is applied to 'classList'
        // clI points to char before "add".
        clI = internalSkipSpaceBack(buf, clI);
        
        if (buf[clI] == '.') 
        {
            clI = (clI > 0) ? clI - 1 : 0;
            clI = internalSkipSpaceBack(buf, clI);
            
            if (internalMatchWordBack(buf, &clI, "classList")) {
                return CTX_CLASS;
            }
        }
    }

    return CTX_NONE;
}


// Returns true if comment was handled and index updated.
static bool internalConsumeComment(const char* src, size_t len, size_t* i, JsDynBuf* out)
{
    if (src[*i] != '/' || *i + 1 >= len) return false;

    if (src[*i + 1] == '/') // Line comment.
    {
        *i += 2;
        while (*i < len && src[*i] != '\n' && src[*i] != '\r') (*i)++;
        return true;
    }
    else if (src[*i + 1] == '*') // Block comment.
    {
        *i += 2;
        while (*i + 1 < len && !(src[*i] == '*' && src[*i + 1] == '/')) (*i)++;
        *i += 2;
        internalDbAppendChar(out, ' '); // Safety space. TODO: See if the space is necessary.
        return true; 
    }
    return false;
}

// Returns true if string was handled and index updated.
static bool internalConsumeString(const char* src, size_t len, size_t* i, JsDynBuf* out, bool mangle, SetOfClassesAndIDs* set)
{
    size_t startQuoteIdx = *i;
    char quote = src[*i];
    
    if (quote != '\'' && quote != '\"' && quote != '`') return false;

    (*i)++; // Skip open quote

    // 1. Capture content
    JsDynBuf strContent;
    internalDbInit(&strContent);
    bool escaped = false;
    
    while (*i < len)
    {
        if (src[*i] == '\\' && !escaped)
        {
            escaped = true;
            internalDbAppendChar(&strContent, src[*i]); // Keep escape in raw content
            (*i)++;
            continue;
        }
        if (src[*i] == quote && !escaped) break;
        internalDbAppendChar(&strContent, src[*i]);
        escaped = false;
        (*i)++;
    }

    // 2. Analyze & Mangle
    char* rawStr = strContent.data;
    size_t rawLen = strContent.len;
    
    // Determine context
    JsContextType ctx = CTX_NONE;

    if (set)
    {
        // Priority 1: Directives (/*ID-NEXT*/)
        ctx = internalSniffDirective(src, startQuoteIdx);
        
        // Priority 2: Syntax Context (classList.add, querySelector, etc)
        if (ctx == CTX_NONE) {
            ctx = internalSniffContext(out->data, out->len);
        }
    }

    // Apply Mangle Logic
    bool performMangle = false;
    bool isId = false;
    char* tokenToMangle = rawStr;

    // CASE A: Explicit ID or Class context (from directive or specific function like getElementById)
    if (ctx == CTX_ID || ctx == CTX_CLASS)
    {
        performMangle = true;
        isId = (ctx == CTX_ID);
        
        // Strip '.' or '#' if user accidentally provided it in a context that doesn't need it
        // (e.g. getElementById("#foo") - technically wrong JS but we can handle it, 
        // or classList.add(".foo")).
        if (rawLen > 1 && (rawStr[0] == '#' || rawStr[0] == '.')) {
            tokenToMangle++;
        }
    }
    // CASE B: Selector context (querySelector, $, etc.)
    else if (ctx == CTX_SELECTOR && rawLen > 1)
    {
        // Only mangle if it explicitly looks like a class or ID selector
        if (rawStr[0] == '.') {
            performMangle = true;
            isId = false;
            tokenToMangle++; // Skip '.'
        }
        else if (rawStr[0] == '#') {
            performMangle = true;
            isId = true;
            tokenToMangle++; // Skip '#'
        }
    }
    // CASE C: No context, but string starts with . or #
    // (Existing behavior: blind mangling if it looks like a selector? 
    //  Refined: Only if we are fairly sure. The prompt implies specific context support.
    //  However, keeping original behavior for standalone strings is risky. 
    //  Let's stick to Context-based mangling for safety, plus Directive.)
    
    // 3. Write Output
    internalDbAppendChar(out, quote);

    if (mangle && performMangle && set)
    {
        // Check validity of identifier
        bool valid = (strlen(tokenToMangle) > 0);
        for(size_t k=0; tokenToMangle[k]; k++) {
            if(!internalIsIdentifierChar(tokenToMangle[k])) { valid = false; break; }
        }

        if (valid)
        {
            size_t idx = cssRecordSelector(set, tokenToMangle, isId, false);
            if (idx != (size_t)-1) {
                char mangled[16];
                parserCommonGetMangled((int)idx, mangled);
                
                // Re-add prefix if it was a selector context or we stripped it
                if (tokenToMangle > rawStr) internalDbAppendChar(out, rawStr[0]); 
                
                internalDbAppend(out, mangled, strlen(mangled));
            } else {
                internalDbAppend(out, rawStr, rawLen);
            }
        }
        else
        {
            // Invalid chars for a selector, leave alone
            internalDbAppend(out, rawStr, rawLen);
        }
    }
    else
    {
        internalDbAppend(out, rawStr, rawLen);
    }
    internalDbAppendChar(out, quote);

    if (strContent.data) free(strContent.data);
    if (*i < len) (*i)++; // Skip closing quote in source
    return true;
}

static void internalConsumeWhitespace(const char* src, size_t len, size_t* i, JsDynBuf* out)
{
    // Safety check: do we need a space?
    if (out->len > 0 && !parserCommonIsSpace(out->data[out->len - 1]))
    {
        char prev = out->data[out->len - 1];
        if (isalnum((unsigned char)prev) || prev == '_' || prev == '$')
        {
            size_t j = *i + 1;
            while (j < len && parserCommonIsSpace(src[j])) j++;
            if (j < len)
            {
                char next = src[j];
                if (isalnum((unsigned char)next) || next == '_' || next == '$') internalDbAppendChar(out, ' ');
            }
        }
    }

    // Skip remaining spaces.
    while (*i + 1 < len && parserCommonIsSpace(src[*i + 1])) (*i)++;
    (*i)++;
}

static void internalMinifyStream(const char* src, size_t len, JsDynBuf* out, bool mangle, SetOfClassesAndIDs* set)
{
    size_t i = 0;
    while (i < len)
    {
        if (internalConsumeComment(src, len, &i, out)) continue;
        if (internalConsumeString(src, len, &i, out, mangle, set)) continue;
        
        if (parserCommonIsSpace(src[i]))
        {
            internalConsumeWhitespace(src, len, &i, out);
            continue;
        }

        internalDbAppendChar(out, src[i]);
        i++;
    }
}

DWORD WINAPI jsSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs* args = (ParsingThreadArgs*)lpParam;
    char* src = parserCommonGetPointerToUTF8(args->data, &args->len, args->isPath);
    
    if (!src)
    {
        if (args->mainParsingThread) parserCommonFinished(args->pStateGUI, NULL, 0);
        free(args);
        return 0;
    }

    // 1. Prepare Output Buffer.
    JsDynBuf out;
    internalDbInit(&out);

    // 2. Perform Parsing.
    internalMinifyStream(src, args->len, &out, args->mangle, args->pCritSet);

    // 3. Finalize.
    free(src); // Free original source.
    
    if (!out.data) { out.data = calloc(1, 1); out.len = 0; }

    if (args->mainParsingThread)
    {
        parserCommonFinished(args->pStateGUI, out.data, out.len);
        free(args);
    }
    else
    {
        // TODO: Check that parent is freeing the memory.
        args->data = out.data;
        args->len = out.len;
    }

    return 0;
}