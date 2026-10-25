#include <windows.h>
#include <winevt.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "wevtapi.lib")

void ReadDefenderLogs() {
    // Path to the Windows Defender Operational Log Channel
    LPCWSTR logChannel = L"Microsoft-Windows-Windows Defender/Operational";
    
    // Query for all events in reverse chronological order (newest first)
    EVT_HANDLE hQuery = EvtQuery(NULL, logChannel, NULL, EvtQueryChannelPath | EvtQueryReverseDirection);
    if (hQuery == NULL) {
        std::cerr << "Failed to query event log. Error: " << GetLastError() << std::endl;
        return;
    }

    EVT_HANDLE hEvents[10];
    DWORD dwReturned = 0;

    // Fetch event records in chunks
    while (EvtNext(hQuery, 10, hEvents, INFINITE, 0, &dwReturned)) {
        for (DWORD i = 0; i < dwReturned; i++) {
            DWORD dwBufferSize = 0;
            DWORD dwBufferUsed = 0;
            DWORD dwPropertyCount = 0;

            // First call to determine required buffer size for the XML output
            EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, NULL, &dwBufferUsed, &dwPropertyCount);
            
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                dwBufferSize = dwBufferUsed;
                std::vector<wchar_t> buffer(dwBufferSize / sizeof(wchar_t));

                // Render the complete event structure into XML format
                if (EvtRender(NULL, hEvents[i], EvtRenderEventXml, dwBufferSize, &buffer[0], &dwBufferUsed, &dwPropertyCount)) {
                    std::wcout << std::endl;
                    std::wcout << &buffer[0] << std::endl;
                }
            }
            EvtClose(hEvents[i]);
        }
    }

    EvtClose(hQuery);
}

int main() {
    // Set locale to handle wide-character console output properly
    std::wcout.imbue(std::locale(""));
    ReadDefenderLogs();
    return 0;
}
