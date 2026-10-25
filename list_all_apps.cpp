#include <windows.h>
#include <iostream>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "user32.lib")

void DumpUninstallKey(HKEY hRootKey, const wchar_t* subKeyPath, REGSAM samDesired) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(hRootKey, subKeyPath, 0, KEY_READ | samDesired, &hKey) != ERROR_SUCCESS) {
        return;
    }

    WCHAR subKeyName[256];
    DWORD subKeyNameSize;
    DWORD index = 0;

    while (true) {
        subKeyNameSize = ARRAYSIZE(subKeyName);
        LONG result = RegEnumKeyExW(hKey, index, subKeyName, &subKeyNameSize, NULL, NULL, NULL, NULL);
        if (result == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (result != ERROR_SUCCESS) {
            index++;
            continue;
        }

        HKEY hSubKey = NULL;
        WCHAR fullPath[512];
        wsprintfW(fullPath, L"%s\\%s", subKeyPath, subKeyName);

        if (RegOpenKeyExW(hRootKey, fullPath, 0, KEY_READ | samDesired, &hSubKey) == ERROR_SUCCESS) {
            WCHAR displayName[512] = { 0 };
            WCHAR displayVersion[128] = { 0 };
            DWORD dataSize = sizeof(displayName);

            // Get Display Name
            if (RegQueryValueExW(hSubKey, L"DisplayName", NULL, NULL, (LPBYTE)displayName, &dataSize) == ERROR_SUCCESS) {
                dataSize = sizeof(displayVersion);
                RegQueryValueExW(hSubKey, L"DisplayVersion", NULL, NULL, (LPBYTE)displayVersion, &dataSize);

                std::wcout << L"Name: " << displayName;
                if (displayVersion[0] != L'\0') {
                    std::wcout << L" | Version: " << displayVersion;
                }
                std::wcout << std::endl;
            }
            RegCloseKey(hSubKey);
        }
        index++;
    }
    RegCloseKey(hKey);
}

int main() {
    // Enable Unicode output for console
    _setmode(_fileno(stdout), _O_U16TEXT);

    std::wcout << L"--- 64-bit / Default Hive ---" << std::endl;
    DumpUninstallKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_64KEY);

    std::wcout << L"\n--- 32-bit Wow6432Node Hive ---" << std::endl;
    DumpUninstallKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_32KEY);

    std::wcout << L"\n--- Per-User Applications ---\n";
    DumpUninstallKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_64KEY);

    std::wcout << L"\n--- Per-User Applications 32-bit Applications on 64-bit Windows (WOW64) ---\n";
    DumpUninstallKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", KEY_WOW64_32KEY);

    return 0;
}
