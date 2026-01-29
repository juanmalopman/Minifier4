
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // sprintf_s, strlen, etc. TODO: Needed here?
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

//
// FUNCTIONS
//

static void jsDbInit(JsDynBuf* db)
{
    db->cap = 4096;
    db->len = 0;
    db->data = malloc(db->cap);
    if (db->data) db->data[0] = 0;
}

static void jsDbAppend(JsDynBuf* db, const char* str, size_t n)
{
    if (!db->data) return;
    if (db->len + n + 1 >= db->cap)
    {
        while (db->len + n + 1 >= db->cap) db->cap *= 2;
        char* tmp = realloc(db->data, db->cap);
        if (!tmp) return;
        db->data = tmp;
    }
    memcpy(db->data + db->len, str, n);
    db->len += n;
    db->data[db->len] = 0;
}

static void jsDbAppendChar(JsDynBuf* db, char c)
{
    char tmp[2] = { c, 0 };
    jsDbAppend(db, tmp, 1);
}

static inline bool jsIsIdentifierChar(char c)
{
    return isalnum((unsigned char)c) || c == '-' || c == '_';
}

// Helper to look backwards in source for /*ID-NEXT*/ or /*CLASS-NEXT*/.
// Returns 1 for ID, 0 for Class, -1 for None.
static int jsSniffDirective(const char* src, size_t i)
{
    if (i == 0) return -1;
    size_t end = i - 1;

    // 1. Skip whitespace backwards from the current quote position.
    while (end > 0 && parserCommonIsSpace(src[end])) end--;

    // 2. We expect the comment to end with '*/'.
    if (end < 1 || src[end] != '/' || src[end - 1] != '*') return -1;

    // 3. Check for /*ID-NEXT*/ (Total length 11: 2 for /*, 7 for ID-NEXT, 2 for */).
    // We are at the index of the final '/'. Start of content is at end - 8.
    if (end >= 10 && src[end - 10] == '/' && src[end - 9] == '*')
    {
        const char* content = &src[end - 8];
        const char* target = "id-next";
        bool match = true;
        for (int k = 0; k < 7; k++) {
            if (tolower((unsigned char)content[k]) != target[k]) { match = false; break; }
        }
        if (match) return 1;
    }

    // 4. Check for /*CLASS-NEXT*/ (Total length 14: 2 for /*, 10 for CLASS-NEXT, 2 for */).
    if (end >= 13 && src[end - 13] == '/' && src[end - 12] == '*')
    {
        const char* content = &src[end - 11];
        const char* target = "class-next";
        bool match = true;
        for (int k = 0; k < 10; k++) {
            if (tolower((unsigned char)content[k]) != target[k]) { match = false; break; }
        }
        if (match) return 0;
    }

    return -1;
}

static int jsSniffContext(const char* buf, size_t len)
{
    if (len < 5) return -1;
    size_t i = len - 1;
    while (i > 0 && (parserCommonIsSpace(buf[i]) || buf[i] == '(' || buf[i] == '=' || buf[i] == ',')) i--;
    
    if (i >= 13 && !strncmp(&buf[i - 13], "getElementById", 14)) return 1; // ID.
    if (i >= 19 && !strncmp(&buf[i - 19], "getElementsByTagName", 20)) return -1;
    if (i >= 21 && !strncmp(&buf[i - 21], "getElementsByClassName", 22)) return 0; // Class.
    
    // classList checks
    if (i >= 3 && !strncmp(&buf[i - 3], ".add", 4)) return 0;
    if (i >= 6 && !strncmp(&buf[i - 6], ".remove", 7)) return 0;
    if (i >= 6 && !strncmp(&buf[i - 6], ".toggle", 7)) return 0;
    if (i >= 8 && !strncmp(&buf[i - 8], ".contains", 9)) return 0;

    return -1;
}

// Returns true if comment was handled and index updated.
static bool jsConsumeComment(const char* src, size_t len, size_t* i, JsDynBuf* out)
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
        jsDbAppendChar(out, ' '); // Safety space. TODO: See if the space is necessary.
        return true; 
    }
    return false;
}

// Returns true if string was handled and index updated.
static bool jsConsumeString(const char* src, size_t len, size_t* i, JsDynBuf* out, bool mangle, SetOfClassesAndIDs* set)
{
    size_t startQuoteIdx = *i; // Store start index for backtracking.
    char c = src[*i];
    if (c != '\'' && c != '\"' && c != '`') return false;

    char quote = c;
    (*i)++; // Skip quote.

    // 1. Capture content.
    JsDynBuf strContent;
    jsDbInit(&strContent);
    bool escaped = false;
    
    while (*i < len)
    {
        if (src[*i] == '\\' && !escaped)
        {
            escaped = true;
            jsDbAppendChar(&strContent, src[*i]);
            (*i)++;
            continue;
        }
        if (src[*i] == quote && !escaped) break;
        jsDbAppendChar(&strContent, src[*i]);
        escaped = false;
        (*i)++;
    }

    // 2. Analyze & Mangle.
    char* rawStr = strContent.data;
    size_t rawLen = strContent.len;
    bool isSelector = false;
    bool isId = false;
    char* cleanName = rawStr;

    // Check prefix.
    if (rawLen > 1)
    {
        if (rawStr[0] == '.') { isSelector = true; isId = false; cleanName++; }
        else if (rawStr[0] == '#') { isSelector = true; isId = true; cleanName++; }
    }
    // Check context.
    if (!isSelector && set)
    {
        // Priority 1: Check directive backwards from the opening quote.
        int ctx = jsSniffDirective(src, startQuoteIdx);
        
        // Priority 2: If no directive, check JS syntax context (getElementById, etc).
        if (ctx == -1) {
            ctx = jsSniffContext(out->data, out->len);
        }

        if (ctx != -1)
        {
            bool valid = (rawLen > 0);
            for(size_t k=0; k<rawLen; k++) if(!jsIsIdentifierChar(rawStr[k])) valid = false;
            if (valid)
            {
                isSelector = true;
                isId = (ctx == 1);
            }
        }
    }

    // 3. Write Output.
    jsDbAppendChar(out, quote);
    if (mangle && isSelector && set)
    {
        size_t idx = cssRecordSelector(set, cleanName, isId, false);
        if (idx != (size_t)-1) {
            char mangled[16];
            parserCommonGetMangled((int)idx, mangled);
            if (cleanName > rawStr) jsDbAppendChar(out, rawStr[0]); // Prefix
            jsDbAppend(out, mangled, strlen(mangled));
        } else {
            jsDbAppend(out, rawStr, rawLen);
        }
    }
    else
    {
        jsDbAppend(out, rawStr, rawLen);
    }
    jsDbAppendChar(out, quote);

    if (strContent.data) free(strContent.data);
    if (*i < len) (*i)++; // Skip closing quote in source.
    return true;
}
static void jsConsumeWhitespace(const char* src, size_t len, size_t* i, JsDynBuf* out)
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
                if (isalnum((unsigned char)next) || next == '_' || next == '$') jsDbAppendChar(out, ' ');
            }
        }
    }

    // Skip remaining spaces.
    while (*i + 1 < len && parserCommonIsSpace(src[*i + 1])) (*i)++;
    (*i)++;
}

static void jsMinifyStream(const char* src, size_t len, JsDynBuf* out, bool mangle, SetOfClassesAndIDs* set)
{
    size_t i = 0;
    while (i < len)
    {
        if (jsConsumeComment(src, len, &i, out)) continue;
        if (jsConsumeString(src, len, &i, out, mangle, set)) continue;
        
        if (parserCommonIsSpace(src[i]))
        {
            jsConsumeWhitespace(src, len, &i, out);
            continue;
        }

        jsDbAppendChar(out, src[i]);
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
    jsDbInit(&out);

    // 2. Perform Parsing.
    jsMinifyStream(src, args->len, &out, args->mangle, args->pCritSet);

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