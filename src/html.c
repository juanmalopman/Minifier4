// ============================================================================== 
// FILE: src\html.c 
// ============================================================================== 

//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h> // int64_t, uint32_t, etc.
#include <stdio.h>
#include <string.h> // sprintf_s, strlen, etc.
#include "html.h"
#include "parser_common.h"
#include "main_window.h"
#include "minify_config.h"
#include "app_logging.h"
#include "css.h"
#include "js.h"

//
// ENUMS
//

typedef enum FoundAttribute : int
{
    ATTR_NONE = -1,
    ATTR_CLASS = 0,
    ATTR_ID,
    ATTR_HREF
} FoundAttribute;

//
// STRUCTS
//

typedef struct ChildThreads
{
    ParsingThreadArgs parsingThreadArgs;
    bool isCSS;
    HANDLE hThread;
} ChildThreads;


typedef struct ContextHTML
{
    int iChildThread;
    ChildThreads* pChildThreads;
    
    // Position markers.
    size_t iCSS;     // Where <style> goes in head.
    size_t iFold;    // Where the fold comment was found.
    size_t iJS;      // Where <script> goes (usually body end).
    size_t iError[2];
    size_t o;        // Output write cursor index.
    
    // CSS Logic.
    SetOfClassesAndIDs critSet; // Stores found classes and id separated by above or under the fold location.
    bool isUnderFold;           // State flag during parsing.

    bool mangle; // Current config.

} ContextHTML;

//
// CONFIGURATION CONSTANTS
//

static constexpr int nThreadBlock = 99; // How many thread stucts to allocate in memory at a time.
static constexpr char styleTag[] = "<style>";
static constexpr char scriptTag[] = "<script>";
static constexpr char styleEndTag[] = "</style>";
static constexpr char scriptEndTag[] = "</script>";
static constexpr char headEndTag[] = "</head>";
static constexpr char bodyEndTag[] = "</body>";
static constexpr int maxLenClassOrID = 255; // There's no official spec for length of classes and ID's, but this app will cap them.

//
// FUNCTIONS
//

static bool internalAllocateChildThreadStructMem(_In_ int iChildThread, _Inout_ ChildThreads** pChildThreads, _In_ int nBlock)
{
    if (iChildThread % (nBlock + 1) == nBlock)
    {
        // 1. Calculate the number of elements we want.
        size_t numBlocks = (size_t)(iChildThread / nBlock) + 1;
        
        // 2. Check for overflow.
        if (SIZE_MAX / nBlock < numBlocks || SIZE_MAX / sizeof(ChildThreads) < (numBlocks * nBlock))
        {
            appLogError("Size_t overflow allocating memory for helper CSS or JS thread.");
            return false;
        }

        size_t newSize = numBlocks * (size_t)nBlock * sizeof(ChildThreads);

        ChildThreads* temp = realloc(*pChildThreads, newSize);
        if (!temp)
        {
            appLogError("Failed to allocate memory for helper CSS or JS thread.");
            return false;
        }
        *pChildThreads = temp;
    }
    return true;
}

static void internalQueueHelperThread(_Inout_ ContextHTML* ctx, _In_ bool isPath, _In_ bool isCSS, _In_ char* buffer, _In_ size_t bufferLen)
{
    if (!internalAllocateChildThreadStructMem(ctx->iChildThread, &ctx->pChildThreads, nThreadBlock))
    {
        free(buffer);
        return;
    }

    ChildThreads* thread = &ctx->pChildThreads[ctx->iChildThread];
    thread->parsingThreadArgs.pStateGUI = nullptr;
    thread->parsingThreadArgs.mainParsingThread = false;
    thread->parsingThreadArgs.data = buffer;
    thread->parsingThreadArgs.len = bufferLen;
    thread->parsingThreadArgs.isPath = isPath;
    thread->parsingThreadArgs.mangle = ctx->mangle;
    thread->parsingThreadArgs.pCritSet = &ctx->critSet;
    thread->isCSS = isCSS;
    
    ctx->iChildThread++;
}

static bool internalSpawnHelperThread(_In_ int index, _In_ ChildThreads* pChildThreads)
{
    DWORD (WINAPI *selectedFunc)(LPVOID) = pChildThreads[index].isCSS ? cssSpawnThread : jsSpawnThread;
    
    pChildThreads[index].hThread = CreateThread(
        NULL, 0, selectedFunc,
        &(pChildThreads[index].parsingThreadArgs),
        0, NULL
    );

    if (!pChildThreads[index].hThread)
    {
        appLogError("Failed to spawn helper thread.");
        return false;
    }
    return true;
}

static void internalHandleComment(_Inout_ ContextHTML* ctx, _In_ char* pD, _In_ size_t len, _Inout_ size_t* idx, _Inout_ size_t* lastNewline)
{
    // Start of comment detected at pD[*idx] == '!' (after < and - and -)
    // Check previous chars to confirm <!--
    if (*idx < 1 || pD[*idx - 1] != '<' || *idx + 2 >= len || pD[*idx + 1] != '-' || pD[*idx + 2] != '-')
    {
        pD[ctx->o++] = pD[*idx];
        return;
    }

    ctx->o--; // Erase '<' written in previous iteration
    size_t commentStart = *idx - 1;

    // Scan forward for -->
    for (size_t k = *idx; k < len; k++)
    {
        if (pD[k] == '>' && pD[k - 1] == '-' && pD[k - 2] == '-')
        {
            // Check Special Tags
            if (!_strnicmp(&pD[commentStart], htmlTagFold, sizeof(htmlTagFold) - 1))
            {
                ctx->iFold = ctx->o;
                ctx->isUnderFold = true;
            }
            else if (!_strnicmp(&pD[commentStart], htmlTagError, sizeof(htmlTagError) - 1))
            {
                if (!ctx->iError[0]) ctx->iError[0] = ctx->o;
                else ctx->iError[1] = ctx->o;
            }
            
            *lastNewline = k; // Update newline tracker as comments effectively reset context
            *idx = k;         // Move main loop index
            return;
        }
    }
    
    // If we reached here, comment wasn't closed properly or logic fell through
    pD[ctx->o++] = '!'; 
}

static FoundAttribute identifyAttribute(_In_ char* pD, _In_ size_t idx)
{
    static constexpr size_t LOOKBACK_LIMIT = 200;
    static constexpr size_t MAX_ATTR_LEN   = 15;
    
    size_t searchLimitIdx = (idx >= LOOKBACK_LIMIT) ? (idx - LOOKBACK_LIMIT) : 0;
    size_t attrStartIdx   = 0;
    size_t attrLength     = 0;
    size_t tagStartRelPos = 0;
    size_t scanIdx = idx - 1;

    for (; scanIdx > 0; scanIdx--)
    {
        char c = pD[scanIdx];
        if (parserCommonIsBlank(c))
        {
            if (attrLength != 0 && attrStartIdx == 0) attrStartIdx = scanIdx + 1;
        }
        else if (c == '<')
        {
            tagStartRelPos = (searchLimitIdx + LOOKBACK_LIMIT) - scanIdx;
            break; 
        }
        else
        {
            if (attrStartIdx == 0) attrLength++;
        }
        
        if (tagStartRelPos != 0 || attrLength > MAX_ATTR_LEN || scanIdx == searchLimitIdx) break; 
    }

    if (tagStartRelPos == 0 || attrLength > MAX_ATTR_LEN || attrStartIdx == 0) return ATTR_NONE;

    if (attrLength == 5 && !_strnicmp(&pD[attrStartIdx], "class", 5)) return ATTR_CLASS;
    if (attrLength == 2 && !_strnicmp(&pD[attrStartIdx], "id", 2))    return ATTR_ID;
    if (attrLength == 4 && !_strnicmp(&pD[attrStartIdx], "href", 4))  return ATTR_HREF;
    if (attrLength == 3 && !_strnicmp(&pD[attrStartIdx], "src", 3))   return ATTR_HREF;

    return ATTR_NONE;
}

static void internalProcessClassOrId(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len, _Inout_ size_t* idx, _In_ bool isId)
{
    char tempName[maxLenClassOrID]; 
    size_t tempLen = 0;
    
    // *idx is currently at the opening quote.
    (*idx)++; 
    
    for (; *idx < len - 1; (*idx)++)
    {
        char c = pD[*idx];
        bool isSep = parserCommonIsSpace(c);
        bool isQuote = (c == '\"' || c == '\'');

        if (isSep || isQuote)
        {
            if (tempLen > 0)
            {
                tempName[tempLen] = 0;

                static constexpr size_t INVALID_INDEX = (size_t)-1;
                size_t mangleIndex = cssRecordSelector(&ctx->critSet, tempName, isId, !ctx->isUnderFold);

                // Write space if needed (multiple classes for one HTML element).
                if (ctx->o > 0 && pD[ctx->o-1] != '\"' && pD[ctx->o-1] != '\'')
                {
                    pD[ctx->o++] = ' ';
                }

                if (ctx->mangle && mangleIndex != INVALID_INDEX)
                {
                    parserCommonGetMangled(mangleIndex, tempName);
                    tempLen = strlen(tempName);
                }

                memcpy(&pD[ctx->o], tempName, tempLen);
                ctx->o += tempLen;
                tempLen = 0;
            }
            
            if (isQuote)
            {
                pD[ctx->o++] = c;
                return; // Done
            }
        }
        else
        {
            if (tempLen < maxLenClassOrID - 1) tempName[tempLen++] = c;
        }
    }
}

static void internalProcessExternalResource(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len, _Inout_ size_t* idx)
{
    size_t pathLen = 0;
    char path[MAX_PATH] = { };
    char fileExt[12] = { };
    int8_t extLen = -1;
    bool pathDone = false;
    
    size_t openingQuoteIdx = *idx;

    // Scan the HREF value
    for (; *idx < len - 1; (*idx)++)
    {
        char c = pD[*idx];
        if (pathLen > MAX_PATH - 10) break;

        // Check for Quote (Start or End).
        if (c == '\"' || c == '\'')
        {
            if (*idx == openingQuoteIdx) continue; // Skip opening
            else break; // Closing found.
        }

        // Handle Spaces inside URL
        if (parserCommonIsSpace(c))
        {
            if (pathLen == 0) continue; 
            pathDone = true;
            continue;
        }

        // Safety break
        if (c == '>') { (*idx)--; break; }

        if (!pathDone)
        {
            path[pathLen++] = c;
            if (c == '.') { memset(fileExt, 0, sizeof(fileExt)); extLen = 0; continue; }
            if (extLen > -1 && extLen < 11) fileExt[extLen++] = c;
        }
    }

    bool isJS  = (_strnicmp(fileExt, "js", 2) == 0);
    bool isCSS = (_strnicmp(fileExt, "css", 3) == 0);

    if ((isJS || isCSS) && pathLen > 0)
    {
        char* buffer = malloc(pathLen + 1);
        if (buffer)
        {
            memcpy(buffer, path, pathLen);
            buffer[pathLen] = 0;
            internalQueueHelperThread(ctx, true, isCSS, buffer, pathLen);

            // Erase the tag currently being written (<link... or <script...)
            // Backtrack ctx->o until '<' is found
            while (ctx->o > 0)
            {
                ctx->o--;
                char c = pD[ctx->o];
                pD[ctx->o] = 0;
                if (c == '<') break; 
            }

            // Consume rest of input tag in pD
            for (; *idx < len; (*idx)++) if (pD[*idx] == '>') break;
            
            // If JS script tag, also consume the closing </script>. ++(*idx) skips current '>' just found.
            if (isJS) for (; ++(*idx) < len; (*idx)++) if (pD[*idx] == '>') break;
            return;
        }
    }

    // Failed or not valid resource, reset logic to treat as normal text.
    *idx = openingQuoteIdx; 
}

static void internalHandleAttribute(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len, _Inout_ size_t* idx)
{
    FoundAttribute attrType = identifyAttribute(pD, *idx);

    if (attrType == ATTR_NONE)
    {
        pD[ctx->o++] = '=';
        return;
    }

    // Rewind output to remove spaces before '='
    while (ctx->o > 0 && parserCommonIsSpace(pD[ctx->o-1]))
    {
        ctx->o--; 
        pD[ctx->o] = 0;
    }
    pD[ctx->o++] = '=';

    // Find Opening Quote
    (*idx)++; 
    for (; *idx < len - 1; (*idx)++)
    {
        char c = pD[*idx];
        if (c == '\"' || c == '\'')
        {
            pD[ctx->o++] = c;
            // Don't increment idx here, passed to helpers at quote pos
            break;
        }
        // If we hit non-space non-quote, it's unquoted attr (not supported by this optimizer safely), abort
        if (!parserCommonIsSpace(c)) return; 
    }

    switch (attrType)
    {
        case ATTR_CLASS: internalProcessClassOrId(ctx, pD, len, idx, false); break;
        case ATTR_ID:    internalProcessClassOrId(ctx, pD, len, idx, true);  break;
        case ATTR_HREF:  internalProcessExternalResource(ctx, pD, len, idx); break;
        default: break;
    }
}

static void internalHandleOpenTag(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len, _Inout_ size_t* idx)
{
    // Look ahead to identify tag
    if (!(*idx + sizeof(scriptTag) - 1 < len))
    {
        pD[ctx->o++] = '<'; return;
    }

    size_t i = *idx;
    bool isInlineStyle = false;
    bool isInlineScript = false;

    if (!_strnicmp(&pD[i], styleTag, sizeof(styleTag) - 1))
    {
        i += sizeof(styleTag) - 1; isInlineStyle = true;
    }
    else if (!_strnicmp(&pD[i], scriptTag, sizeof(scriptTag) - 1))
    {
        i += sizeof(scriptTag) - 1; isInlineScript = true;
    }
    else if (!_strnicmp(&pD[i], headEndTag, sizeof(headEndTag) - 1))
    {
        ctx->iCSS = ctx->o; 
        pD[ctx->o++] = '<'; return;
    }
    else if (!_strnicmp(&pD[i], bodyEndTag, sizeof(bodyEndTag) - 1))
    {
        ctx->iJS = ctx->o; 
        pD[ctx->o++] = '<'; return;
    }
    else if (!_strnicmp(&pD[i + 1], footerTagText, sizeof(footerTagText) - 1))
    {
        // Set the footer as a non-critical class for it to be sent to the non-critical CSS unless it's before the <!--FOLD--> comment.
        cssRecordSelector(&ctx->critSet, footerTagText, true, !ctx->isUnderFold);
        pD[ctx->o++] = '<'; return;
    } 
    else
    {
        pD[ctx->o++] = '<'; return;
    }

    // If we are here, we found an inline <style> or <script> to extract
    size_t contentStart = i;
    size_t contentLen = 0;

    // Scan for end tag
    for (; i < len; i++)
    {
        if (pD[i] == '<')
        {
            if (isInlineStyle && !_strnicmp(&pD[i], styleEndTag, sizeof(styleEndTag)-1))
            {
                contentLen = i - contentStart; 
                i += sizeof(styleEndTag) - 2; 
                break;
            }
            if (isInlineScript && !_strnicmp(&pD[i], scriptEndTag, sizeof(scriptEndTag)-1))
            {
                contentLen = i - contentStart; 
                i += sizeof(scriptEndTag) - 2; 
                break;
            }
        }
    }
    
    char* buffer = malloc(contentLen + 1);
    if (buffer)
    {
        memcpy(buffer, &pD[contentStart], contentLen);
        buffer[contentLen] = 0;
        internalQueueHelperThread(ctx, false, isInlineStyle, buffer, contentLen);
        *idx = i; // Move main loop index to end of tag
    }
    else
    {
        // Allocation failure fallback: just output original char
        pD[ctx->o++] = '<';
    }
}

static void internalParseHTML(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _In_ size_t len)
{
    size_t lastWasNewline = 0;
    
    for (size_t i = 0; i < len; i++)
    {
        switch (pD[i])
        {
        case '\b':
        case '\a':
            if (i && i - 1 == lastWasNewline) lastWasNewline = i;
            break;

        case '\r':
        case '\n':
            lastWasNewline = i;
            break;

        case '\t':
        case ' ':
            if (i && i - 1 == lastWasNewline) {
                lastWasNewline = i;
                break;
            }
            pD[ctx->o++] = pD[i];
            break;

        case '!': 
            internalHandleComment(ctx, pD, len, &i, &lastWasNewline);
            break;

        case '=': 
            internalHandleAttribute(ctx, pD, len, &i);
            break;

        case '<': 
            internalHandleOpenTag(ctx, pD, len, &i);
            break;

        default:
            pD[ctx->o++] = pD[i];
            break;
        }
    } 
}

static void internalMangledNamesSetup(_In_ ContextHTML* ctx)
{
    if (ctx->mangle)
    {
        // Get ready the mangled classes and IDs. // TODO: First get the "extra" classes and IDs from JS.
                                                  // TODO: Evaluate just any call to parserCommonGetMangled iterating from the last index
                                                  // up to the requested one, in a thead safe way. Current way is faster.
        char dummyBuff[3];
        size_t maxIndexClasses = ctx->critSet.classesAbove.count + ctx->critSet.classesUnder.count;
        size_t maxIndexIds = ctx->critSet.idsAbove.count + ctx->critSet.idsUnder.count;
        size_t maxIndex = maxIndexClasses > maxIndexIds ? maxIndexClasses : maxIndexIds;

        for (size_t i = 0; i < maxIndex; i++)
        {
            parserCommonGetMangled(i, dummyBuff); // This ensures any future call is ordered as the function needs.
        }
    }
}

static bool internalThreadQueue(_Inout_ ContextHTML* ctx)
{
    if (ctx->iChildThread)
    {
        // TODO: Make these settings available to the user.
        static constexpr DWORD dwTotalTimeout = 3000; // 3 seconds total for all batches.
        static constexpr int BATCH_SIZE = 32; // Max number of threads to spawn at a time. Must be <= 64 (MAXIMUM_WAIT_OBJECTS).
        
        DWORD dwStartTime = GetTickCount();
        HANDLE batchHandles[BATCH_SIZE];
        int batchIndices[BATCH_SIZE]; // Map batch index back to pChildThreads index
        
        // Execute in two passes: 0 = JS, 1 = CSS.
        for (int pass = 0; pass < 2; pass++)
        {
            bool doingCSS = (pass == 1);

            // Execute mangled names setup after JS threads finish, but before CSS threads start.
            // This allows JS to potentially extract class names that CSS needs to be aware of.
            if (doingCSS)
            {
                internalMangledNamesSetup(ctx);
            }

            int currentBatchCount = 0;

            for (int i = 0; i < ctx->iChildThread; i++)
            {
                // Filter: If pass 0, process only JS (!isCSS). If pass 1, process only CSS.
                if (ctx->pChildThreads[i].isCSS != doingCSS) continue;

                // 1. Spawn.
                if (internalSpawnHelperThread(i, ctx->pChildThreads))
                {
                    batchHandles[currentBatchCount] = ctx->pChildThreads[i].hThread;
                }
                else
                {
                    batchHandles[currentBatchCount] = INVALID_HANDLE_VALUE;
                }
                
                // Track which main index this handle belongs to for cleanup.
                batchIndices[currentBatchCount] = i;
                currentBatchCount++;

                // 2. If Batch Full, Wait and Clean.
                if (currentBatchCount == BATCH_SIZE)
                {
                    DWORD dwElapsed = GetTickCount() - dwStartTime;
                    DWORD dwTimeLeft = (dwElapsed >= dwTotalTimeout) ? 0 : (dwTotalTimeout - dwElapsed);

                    DWORD dwWaitResult = WaitForMultipleObjects(
                        (DWORD)currentBatchCount, 
                        batchHandles, 
                        TRUE,
                        dwTimeLeft
                    );

                    // Cleanup Handles.
                    for (int j = 0; j < currentBatchCount; j++)
                    {
                        if (batchHandles[j] && batchHandles[j] != INVALID_HANDLE_VALUE)
                        {
                            CloseHandle(batchHandles[j]);
                            ctx->pChildThreads[batchIndices[j]].hThread = nullptr;
                        }
                    }

                    if (dwWaitResult == WAIT_TIMEOUT || dwWaitResult == WAIT_FAILED)
                    {
                        appLogError("Helper threads timed out or failed in batch processing.");
                        return false;
                    }

                    currentBatchCount = 0;
                }
            }

            // 3. Process remaining items in this pass.
            if (currentBatchCount > 0)
            {
                DWORD dwElapsed = GetTickCount() - dwStartTime;
                DWORD dwTimeLeft = (dwElapsed >= dwTotalTimeout) ? 0 : (dwTotalTimeout - dwElapsed);

                DWORD dwWaitResult = WaitForMultipleObjects(
                    (DWORD)currentBatchCount, 
                    batchHandles, 
                    TRUE,
                    dwTimeLeft
                );

                for (int j = 0; j < currentBatchCount; j++)
                {
                    if (batchHandles[j] && batchHandles[j] != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(batchHandles[j]);
                        ctx->pChildThreads[batchIndices[j]].hThread = nullptr;
                    }
                }

                if (dwWaitResult == WAIT_TIMEOUT || dwWaitResult == WAIT_FAILED)
                {
                    appLogError("Helper threads timed out or failed in batch processing.");
                    return false;
                }
            }
        }
    }

    return true;
}

static bool internalMergeContextsCSS(_In_ ContextHTML* ctx, _Out_ CssOutputs* outResult, _Out_ CssContext** outMaster)
{
    *outMaster = cssCreateContext(ctx->mangle);
    if (!*outMaster)
    {
        appLogError("Failed to allocate memory for outMaster context.");
        return false;
    }
    
    // Iterate threads to merge CSS.
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        if (ctx->pChildThreads[i].isCSS)
        {
            CssContext* threadCtx = (CssContext*)ctx->pChildThreads[i].parsingThreadArgs.data;
            if (threadCtx)
            {
                cssMergeContexts(*outMaster, threadCtx);
                cssDestroyContext(threadCtx);
            }
        }
    }

    // Generate Final Split CSS using the Critical Set we gathered parsing HTML.
    *outResult = cssGenerateSplitOutput(*outMaster, &ctx->critSet);

    return true;
}

static void internalStitching(_Inout_ ContextHTML* ctx, _Inout_ char* pD, _Out_ char** pO, _Out_ size_t* pTotalLen)
{
    bool success = false;

    // Execute any helper thread needed.
    if (!internalThreadQueue(ctx)) goto cleanup;

    // Merge CSS Contexts & Generate Output.
    CssContext* masterCss = nullptr;
    CssOutputs cssOut = { };
    if (!internalMergeContextsCSS(ctx, &cssOut, &masterCss)) goto cleanup;

    // Calculate Total Size.
    *pTotalLen = ctx->o;
    
    // Add CSS sizes (Above + Under).
    if (cssOut.aboveLen > 0) *pTotalLen += cssOut.aboveLen + strlen(styleTag) + strlen(styleEndTag);
    if (cssOut.underLen > 0) *pTotalLen += cssOut.underLen + strlen(styleTag) + strlen(styleEndTag);
    
    // Add JS sizes.
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        if (!ctx->pChildThreads[i].isCSS)
        {
            *pTotalLen += ctx->pChildThreads[i].parsingThreadArgs.len + strlen(scriptTag) + strlen(scriptEndTag);
        }
    }

    // Allocate.
    *pO = malloc(*pTotalLen + 1);
    if (!*pO)
    {
        cssDestroyContext(masterCss);
        free(pD);
        return;
    }
    char* pCursor = *pO;

    // Stitching.
    
    // A. Header (HTML up to <style> insertion point).
    memcpy(pCursor, pD, ctx->iCSS); 
    pCursor += ctx->iCSS;

    // B. Critical CSS (Above the Fold).
    if (cssOut.aboveLen > 0)
    {
        memcpy(pCursor, styleTag, sizeof(styleTag) - 1); pCursor += sizeof(styleTag) - 1;
        memcpy(pCursor, cssOut.aboveCSS, cssOut.aboveLen); pCursor += cssOut.aboveLen;
        memcpy(pCursor, styleEndTag, sizeof(styleEndTag) - 1); pCursor += sizeof(styleEndTag) - 1;
    }

    // C. Middle HTML (From <style> to <script>/Footer).
    size_t lenMiddle = ctx->iJS - ctx->iCSS;
    memcpy(pCursor, pD + ctx->iCSS, lenMiddle);
    pCursor += lenMiddle;

    // D. Javascript (All JS threads).
    for (int i = 0; i < ctx->iChildThread; i++)
    {
        if (!ctx->pChildThreads[i].isCSS)
        {
            char* jsData = ctx->pChildThreads[i].parsingThreadArgs.data;
            size_t jsLen = ctx->pChildThreads[i].parsingThreadArgs.len;
            if (jsData && jsLen)
            {
                memcpy(pCursor, scriptTag, sizeof(scriptTag) - 1); pCursor += sizeof(scriptTag) - 1;
                memcpy(pCursor, jsData, jsLen); pCursor += jsLen;
                memcpy(pCursor, scriptEndTag, sizeof(scriptEndTag) - 1); pCursor += sizeof(scriptEndTag) - 1;
            }
        }
    }

    // E. Non-Critical CSS (Under the Fold) - Lazy Loaded at bottom.
    if (cssOut.underLen > 0)
    {
        memcpy(pCursor, styleTag, sizeof(styleTag) - 1); pCursor += sizeof(styleTag) - 1;
        memcpy(pCursor, cssOut.underCSS, cssOut.underLen); pCursor += cssOut.underLen;
        memcpy(pCursor, styleEndTag, sizeof(styleEndTag) - 1); pCursor += sizeof(styleEndTag) - 1;
    }

    // F. Footer HTML.
    size_t lenFooter = ctx->o - ctx->iJS;
    memcpy(pCursor, pD + ctx->iJS, lenFooter);
    pCursor += lenFooter;
    *pCursor = '\0'; // Null terminate

    success = true;
    
    // Cleanup.
cleanup:
    if (masterCss) cssDestroyContext(masterCss);
    cssOutFree(&cssOut);
    free(pD);

    if (!success && *pO)
    {
        free(*pO);
        *pO = nullptr;
        *pTotalLen = 0;
    }
}

DWORD WINAPI htmlSpawnThread(LPVOID lpParam)
{
    ParsingThreadArgs argsStack;
    memcpy(&argsStack, lpParam, sizeof(ParsingThreadArgs));
    free(lpParam);
    StateGUI* pStateGUI = argsStack.pStateGUI;
    size_t len = argsStack.len;

    char* pD = parserCommonGetPointerToUTF8(argsStack.data, &len, argsStack.isPath);
    if (!pD) {
        parserCommonFinished(pStateGUI, 0, 0);
        return 0;
    }

    ContextHTML contextHTML = { };
    ContextHTML* ctx = &contextHTML;
    ctx->mangle = argsStack.mangle;
    ctx->pChildThreads = malloc(nThreadBlock * sizeof(ChildThreads));

    if (!ctx->pChildThreads)
    {
        appLogError("Failed to allocate initial memory for HTML child threads.");
        if (pD) free(pD);
        parserCommonFinished(pStateGUI, nullptr, 0); 
        return 0;
    }

    internalParseHTML(ctx, pD, len);

    if (ctx->o < len) pD[ctx->o] = '\0';

    if (!ctx->iChildThread)
    {
        parserCommonFinished(pStateGUI, pD, ctx->o);
    }
    else
    {
        size_t totalLen;
        char* pO;
        internalStitching(ctx, pD, &pO, &totalLen);
        parserCommonFinished(pStateGUI, pO, totalLen);
    }
    
    // Clean up class and ID stored names.
    cssFreeCriticalSet(&ctx->critSet);
    if(ctx->pChildThreads) free(ctx->pChildThreads);
    
    return 0;
}