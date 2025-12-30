
//
// DEPENDENCIES
//

#include <windows.h>
#include <stdint.h>
#include <wchar.h>
#include "file_utils.h"
#include "app_logging.h"
#include <shlobj.h> // To get AppData PATH.

//
// FUNCTIONS
//

bool fileUtilsGetAppDataPath(PWSTR path)
{
    PWSTR tmpPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath( &FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &tmpPath );

    if (hr != S_OK)
    {
    	appLogErrorPop(L"Error getting AppData PATH.");
    	return false;
    }
    
    swprintf_s(path, MAX_PATH, L"%s\\Minifier4", tmpPath);

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
		appLogErrorPop(L"ERROR: A file named Minifier4 is in AppData\\Local. Can't create Minifier4 folder.");
		return false;
	}

	// Existing folder found.
    return  true;
}

bool fileUtilsSaveToFile(LPCWSTR filePath, LPCWSTR filename, LPCVOID buffer, DWORD len)
{
	wchar_t fullPath[MAX_PATH] = { };

	swprintf_s(fullPath, MAX_PATH, L"%s\\%s", filePath, filename);

	HANDLE hFileToWrite = NULL;
	hFileToWrite = CreateFileW(
		fullPath,
		GENERIC_WRITE,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, // Share with any other app concurrently.
		NULL, // SECURITY_ATTRIBUTES pointer.
		CREATE_ALWAYS, // Recreate the file as a whole instead of modifying it.
		FILE_FLAG_WRITE_THROUGH, // Set readonly, hidden etc properties. // TODO: Try using FILE_FLAG_NO_BUFFERING to speed hdd access.
		NULL // Extended file attributes
	);

	// Chequea el éxito de la función anterior.
	if (hFileToWrite == INVALID_HANDLE_VALUE)
	{
		appLogErrorPop(L"Error creating file with CreateFileW().");
		return false;
	}

	// Speed up next write using FSCTL_SET_SPARSE.
	// DeviceIoControl(hFileToWrite, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytesWritten, NULL);

	
	DWORD writtenCount = 0; // Bytes effectively written.
	if (!WriteFile( hFileToWrite, buffer, len, &writtenCount, NULL ))
	{
		appLogErrorPop(L"Error writing file with WriteFile().");
	}
 
	if (!CloseHandle(hFileToWrite)) appLogErrorPop(L"Error closing file handle with CloseHandle().");

	if (writtenCount != len)
	{
		appLogErrorPop(L"Bytes written to the desired file were less than specified.");
		return false;
	}

	return true;
}

bool fileUtilsReadFromFile(LPCWSTR filePath, LPCWSTR filename, void** outBuffer, size_t* outLen)
{
	wchar_t fullPath[MAX_PATH] = { };

	swprintf_s(fullPath, MAX_PATH, L"%s\\%s", filePath, filename);

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
		appLogErrorPop(L"Error getting content size in bytes using GetFileSizeEx().");
		CloseHandle(hFileToRead);
		return false;
	}

	if(!fileSize.QuadPart)
	{
		appLogErrorPop(L"Error: Content size returned from GetFileSizeEx() is 0 bytes.");
		CloseHandle(hFileToRead);
		return false;
	}
    
    // Allocate Memory.
    if (sizeof(size_t) < 8 && fileSize.HighPart != 0)
    {
    	appLogErrorPop(L"Error: 32-bit system. Can't allocate enough memory with malloc for the file to be read.");
        CloseHandle(hFileToRead);
        return false;
    } 
    void* buffer = malloc(fileSize.QuadPart);
    if (!buffer)
    {
    	appLogErrorPop(L"Error: Couldn't allocate memory with malloc for the file to be read.");
        CloseHandle(hFileToRead);
        return false;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFileToRead, buffer, (DWORD)fileSize.QuadPart, &bytesRead, nullptr))
    {
    	appLogErrorPop(L"Error: Couldn't read file with ReadFile().");
        free(buffer);
        CloseHandle(hFileToRead);
        return false;
    }

    CloseHandle(hFileToRead);

    *outBuffer = buffer;
    *outLen = (size_t)bytesRead;
    return true;
}