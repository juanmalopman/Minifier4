
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h>
#include <wchar.h>
#include "file_utils.h"
#include "app_logging.h"
#include <shlobj.h> // To get AppData PATH.
#include <shlwapi.h> // PathCombineW()

//
// FUNCTIONS
//

bool fileUtilsGetAppDataPath(PWSTR path)
{
    PWSTR tmpPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath( &FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &tmpPath );

    if (hr != S_OK)
    {
    	appLogError(L"Error getting AppData PATH.");
    	return false;
    }

    PathCombineW(path, tmpPath, L"Minifier4");

    // WinAPI requires us to free the memory allocated by the Shell.
    CoTaskMemFree(tmpPath);

    // Check Minifier4 exists and is a folder.
    DWORD dwAttrib = GetFileAttributesW(path);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES)
    {
    	// Create "Minifier4" folder.
	 	if (CreateDirectoryW(path, nullptr)) return true;
    }

	if (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
	{
		appLogError(L"ERROR: A file named Minifier4 is in AppData\\Local. Can't create Minifier4 folder.");
		return false;
	}

	// Existing folder found.
    return  true;
}

bool fileUtilsSaveToFile(LPCWSTR fullPath, LPCVOID buffer, DWORD len)
{
	// TODO: Create all intermediate directories if they don't exist whenever saving a file.

	HANDLE hFileToWrite = NULL;
	hFileToWrite = CreateFileW(
		fullPath,
		GENERIC_WRITE,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // Share with any other app concurrently.
		NULL, // SECURITY_ATTRIBUTES pointer.
		CREATE_ALWAYS, // Recreate the file as a whole instead of modifying it.
		FILE_FLAG_WRITE_THROUGH, 	// Write directly to the disk, bypassing the lazy-write cache. Takes the same time but is a synchoronous write.
									// TODO: See if you want this synchronous write or not. Useful for uploading files after parsing, but not yet implemented.
									// TODO: Try using FILE_FLAG_NO_BUFFERING to speed hdd access.
		NULL // Extended file attributes
	);

	// Chequea el éxito de la función anterior.
	if (hFileToWrite == INVALID_HANDLE_VALUE)
	{
		appLogError(L"Error creating file with CreateFileW().");
		return false;
	}

	// Speed up next write using FSCTL_SET_SPARSE.
	// DeviceIoControl(hFileToWrite, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytesWritten, NULL);

	DWORD writtenCount = 0; // Bytes effectively written.
	if (!WriteFile( hFileToWrite, buffer, len, &writtenCount, NULL ))
	{
		appLogError(L"Error writing file with WriteFile().");
	}
 
	if (!CloseHandle(hFileToWrite)) appLogError(L"Error closing file handle with CloseHandle().");

	if (writtenCount != len)
	{
		appLogError(L"Bytes written to the desired file were less than specified.");
		return false;
	}

	return true;
}

// Helper to check  Byte Order Mark (BOM) at the start of files in search for the code page.
void internalDetectCodePage(_In_ void* buffer, _In_ size_t length, _Out_ UINT* outCodePage)
{
    const uint8_t* bytes = (const uint8_t*)buffer;

    // 1. Definitive BOM Checks
    if (length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        *outCodePage = CP_UTF8; return;
    }
    if (length >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        *outCodePage = CP_UTF16LE; return; // Standard Windows Unicode
    }
    if (length >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        *outCodePage = CP_UTF16BE; return;
    }

    // 2. Prepare Heuristic Chunk.
    static constexpr int DETECT_CHUNK_SIZE = 1024;
    int testLen = (int)((length > DETECT_CHUNK_SIZE) ? DETECT_CHUNK_SIZE : length);

    // Safety adjustment (avoid cutting UTF-8 multi-byte sequences).
    if (length > DETECT_CHUNK_SIZE)
    {
        while (testLen > 0) {
            uint8_t b = bytes[testLen - 1];
            if ((b & 0x80) == 0) break;       // ASCII
            if ((b & 0xC0) == 0xC0) { testLen--; break; } // Lead byte
            testLen--; // Continuation byte.
        }
    }

    if (testLen <= 0) { *outCodePage = CP_ACP; return; }

    // 3. UTF-16 LE Heuristic.
    int utf16TestLen = (testLen % 2 == 0) ? testLen : testLen - 1;
    int tests = IS_TEXT_UNICODE_STATISTICS | IS_TEXT_UNICODE_CONTROLS;
    
    if (utf16TestLen > 0 && IsTextUnicode(buffer, utf16TestLen, &tests))
    {
        *outCodePage = CP_UTF16LE;
        return;
    }

    // 4. UTF-8 Strict Heuristic with Null Check.
    int res = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCSTR)buffer, testLen, NULL, 0);
    
    if (res > 0)
    {
        // It passed the structure test, but it might be UTF-16LE masquerading as UTF-8 with Nulls.
        // Web source files (HTML/JS/CSS) should not contain binary NULLs.
        bool containsNull = false;
        for (int i = 0; i < testLen; i++)
        {
            if (bytes[i] == 0)
            {
                containsNull = true;
                break;
            }
        }

        if (containsNull)
        {
            // Valid UTF-8 structure BUT contains NULLs -> Almost certainly UTF-16LE without BOM.
            *outCodePage = CP_UTF16LE;
        }
        else
        {
            *outCodePage = CP_UTF8;
        }
    }
    else
    {
        // 5. Fallback
        *outCodePage = CP_ACP;
    }
}

bool fileUtilsReadFromFile(LPCWSTR fullPath, UINT* codePage, void** outBuffer, size_t* outLen)
{

	HANDLE hFileToRead = nullptr;
	hFileToRead = CreateFileW(
		fullPath,
		GENERIC_READ, // Read only access.
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // Share with any other app concurrently.
		NULL,
		OPEN_EXISTING, // Don't recreate the file if it doesn't exist.
		0,
		NULL
	);

	if (hFileToRead == INVALID_HANDLE_VALUE)
	{
		return false; // Don't spam an error message, the might not be settings saved.
	}

	// Get the content size in bytes.
	LARGE_INTEGER fileSize = { };
	if (!GetFileSizeEx(hFileToRead, &fileSize))
	{
		appLogError(L"Error getting content size in bytes using GetFileSizeEx().");
		CloseHandle(hFileToRead);
		return false;
	}

	if(!fileSize.QuadPart)
	{
		appLogError(L"Error: Content size returned from GetFileSizeEx() is 0 bytes.");
		CloseHandle(hFileToRead);
		return false;
	}
    
    // Allocate Memory.
    if (sizeof(size_t) < 8 && fileSize.HighPart != 0)
    {
    	appLogError(L"Error: 32-bit system. Can't allocate enough memory with malloc for the file to be read.");
        CloseHandle(hFileToRead);
        return false;
    } 
    void* buffer = malloc(fileSize.QuadPart);
    if (!buffer)
    {
    	appLogError(L"Error: Couldn't allocate memory with malloc for the file to be read.");
        CloseHandle(hFileToRead);
        return false;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFileToRead, buffer, (DWORD)fileSize.QuadPart, &bytesRead, nullptr))
    {
    	appLogError(L"Error: Couldn't read file with ReadFile().");
        free(buffer);
        CloseHandle(hFileToRead);
        return false;
    }

    CloseHandle(hFileToRead);

    if (codePage != nullptr)
    {
        internalDetectCodePage(buffer, (size_t)bytesRead, codePage);
    }

    *outBuffer = buffer;
    *outLen = (size_t)bytesRead;
    return true;
}