#include <windows.h>
#include <winevt.h>
#include <iostream>
#include <vector>
#include <string>

#pragma comment(lib, "wevtapi.lib")

// Helper function to convert system time to ISO 8601 string for XPath
std::string GetTime30DaysAgoISO() {
    SYSTEMTIME st;
    GetSystemTime(&st);

    // Subtract 30 days
    FILETIME ft, localFt;
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // 30 days * 24 hours * 60 min * 60 sec * 10^7 (nanoseconds in intervals of 100ns)
    uli.QuadPart -= (ULONGLONG)30 * 24 * 60 * 60 * 10000000;
    
    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;
    FileTimeToSystemTime(&ft, &st);

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return std::string(buffer);
}

void QueryRecentEvents() {
    std::string timeFilter = GetTime30DaysAgoISO();

    std::wstring channel = L"Microsoft-Windows-Windows Defender/Operational";

    // Level 1: Critical
    // Level 2: Error
    // Level 3: Warning
    // Level 4: Information
    // Level 5: Verbose
    // XPath query to get events after the calculated time
    std::wstring query = L"Event[System[Level=1 or Level=2 or Level=3 and TimeCreated[@SystemTime > '" + 
                         std::wstring(timeFilter.begin(), timeFilter.end()) + L"']]]";
    
    // Querying the System log channel
    EVT_HANDLE hResults = EvtQuery(NULL, channel.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
    
    if (hResults == NULL) {
        std::cerr << "EvtQuery failed with error: " << GetLastError() << std::endl;
        return;
    }

    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;
    bool bStatus = true;

    while (bStatus) {
        bStatus = EvtNext(hResults, 10, hEvents, INFINITE, 0, &dwReturned);
        if (!bStatus) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            std::cerr << "EvtNext failed with error: " << GetLastError() << std::endl;
            break;
        }

        for (DWORD i = 0; i < dwReturned; i++) {
            // Render the event into XML
            DWORD dwBufferSize = 0;
            DWORD dwBufferUsed = 0;
            DWORD dwPropertyCount = 0;

            EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);
            
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<wchar_t> buffer(dwBufferUsed / sizeof(wchar_t) + 1);
                if (EvtRender(NULL, hEvents[i], EvtRenderEventXml, (DWORD)buffer.size() * sizeof(wchar_t), &buffer[0], &dwBufferUsed, &dwPropertyCount)) {
                    std::wcout << &buffer[0] << std::endl << std::endl;
                }
            }
            EvtClose(hEvents[i]);
        }
    }
    EvtClose(hResults);
}

int main() {
    QueryRecentEvents();
    return 0;
}
