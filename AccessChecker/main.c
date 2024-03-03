/* An example of checking access rights using the Windows API
 *
 * Made to work with both Unicode ans ANSI settings
 *
 * With Unicode:
 * cl /D_UNICODE /DUNICODE /link Advapi32.lib main.c
 *
 * Without:
 * cl /link Advapi32.lib main.c
 */
#include <stdio.h>

#include <Windows.h>
#include <aclapi.h>
#include <tchar.h>
#include <stdlib.h>

BOOL GetErrorMessage(DWORD dwErrorCode, LPTSTR pBuffer, DWORD cchBufferLength) {
    if (cchBufferLength == 0) {
        return FALSE;
    }

    DWORD cchMsg = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
        dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        pBuffer,
        cchBufferLength,
        NULL
    );
    return (cchMsg > 0);
}


void PrintErrorMessage(const DWORD errorCode) {
    TCHAR msg[255] = {'\0'};
     if (GetErrorMessage(errorCode, msg, 254) != 0) {
        _tprintf_s(_T("Error: %Ts\n"), msg);
    } else {
        _tprintf_s(_T("Error: %lu\n"), errorCode);
    }
}

BOOL GetFolderRights(LPCTSTR folderName, DWORD genericAccessRights, DWORD* grantedRights) {
    DWORD length = 0;

    const SECURITY_INFORMATION requestedInformation = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;

    /* Get the needed length */
    GetFileSecurity(
        folderName,
        requestedInformation,
        NULL,
        NULL,
        &length
    );

    if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
    {
        return FALSE;
    }

    PSECURITY_DESCRIPTOR security = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, length);
    if (GetFileSecurity(folderName, requestedInformation, security, length, &length) == FALSE) {
        LocalFree(security);
        return FALSE;
    }


    DWORD desiredAccess = TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_DUPLICATE | STANDARD_RIGHTS_READ;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), desiredAccess, &hToken) == FALSE) {
        LocalFree(security);
        return FALSE;
    }


    HANDLE hImpersonatedToken = NULL;
    if (DuplicateToken(hToken, SecurityImpersonation, &hImpersonatedToken) == FALSE) {
        CloseHandle(hToken);
        LocalFree(security);
        return FALSE;
    }

    GENERIC_MAPPING mapping = { 0xFFFFFFFF };
    PRIVILEGE_SET privileges = { 0 };
    DWORD grantedAccess = 0, privilegesLength = sizeof(privileges);
    BOOL result = FALSE;

    mapping.GenericRead = FILE_GENERIC_READ;
    mapping.GenericWrite = FILE_GENERIC_WRITE;
    mapping.GenericExecute = FILE_GENERIC_EXECUTE;
    mapping.GenericAll = FILE_ALL_ACCESS;

    MapGenericMask(&genericAccessRights, &mapping);
    if (AccessCheck(
            security,
            hImpersonatedToken,
            genericAccessRights,
            &mapping,
            &privileges,
            &privilegesLength,
            &grantedAccess,
            &result
        ) == FALSE) {
        CloseHandle(hImpersonatedToken);
        CloseHandle(hToken);
        LocalFree(security);
        return FALSE;
    }

    *grantedRights = grantedAccess;

    CloseHandle(hImpersonatedToken);
    CloseHandle(hToken);
    LocalFree(security);

    return TRUE;
}

void PrintMasks(const DWORD Mask) {
     if (((Mask & GENERIC_READ) == GENERIC_READ) || ((Mask & FILE_GENERIC_READ) == FILE_GENERIC_READ))
        wprintf_s(L"\tRead\n");
     if (((Mask & GENERIC_WRITE) == GENERIC_WRITE) || ((Mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE))
        wprintf_s(L"\tWrite\n");
     if (((Mask & GENERIC_EXECUTE) == GENERIC_EXECUTE) || ((Mask & FILE_GENERIC_EXECUTE) == FILE_GENERIC_EXECUTE))
        wprintf_s(L"\tExecute\n");
 }


int _tmain(int argc, TCHAR *argv[]) {
    if (argc < 2) {
        printf_s("No path(s) to check were provided\n");
        return EXIT_FAILURE;
    }


    const DWORD access_mask = MAXIMUM_ALLOWED;
    DWORD grant;


    for (int i = 1; i < argc; ++i) {
        grant = 0;
        const TCHAR* folderPath = argv[i];

        _tprintf_s(_T("Folder %Ts\n"), folderPath);
        if (GetFolderRights(folderPath, access_mask, &grant) == TRUE) {
           PrintMasks(grant);
        } else {
            PrintErrorMessage(GetLastError());
        }
    }

    return EXIT_SUCCESS;
}