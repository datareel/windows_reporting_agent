// Windows Update Agent (WUA) API

#include <windows.h>
#include <wuapi.h>
#include <iostream>
#include <comdef.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

int main() {
    // Initialize the COM library
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize COM library. Error code: " << hr << std::endl;
        return 1;
    }

    std::wcout << L"Retrieving installed Windows patches...\n\n";

    // Create an instance of the UpdateSession
    IUpdateSession* pSession = nullptr;
    hr = CoCreateInstance(CLSID_UpdateSession, NULL, CLSCTX_INPROC_SERVER, IID_IUpdateSession, (LPVOID*)&pSession);
    
    if (SUCCEEDED(hr)) {
        // Create an UpdateSearcher
        IUpdateSearcher* pSearcher = nullptr;
        hr = pSession->CreateUpdateSearcher(&pSearcher);

        if (SUCCEEDED(hr)) {
            // Query the total number of history events available
            LONG totalHistoryCount = 0;
            hr = pSearcher->GetTotalHistoryCount(&totalHistoryCount);

            if (SUCCEEDED(hr) && totalHistoryCount > 0) {
                // Retrieve the history entries (from index 0 to totalHistoryCount)
                IUpdateHistoryEntryCollection* pHistoryCollection = nullptr;
                hr = pSearcher->QueryHistory(0, totalHistoryCount, &pHistoryCollection);

                if (SUCCEEDED(hr)) {
                    LONG historyCount = 0;
                    pHistoryCollection->get_Count(&historyCount);

                    for (LONG i = 0; i < historyCount; ++i) {
                        IUpdateHistoryEntry* pEntry = nullptr;
                        hr = pHistoryCollection->get_Item(i, &pEntry);

                        if (SUCCEEDED(hr)) {
                            BSTR title;
                            BSTR supportUrl;
                            DATE date;
                            UpdateOperation operation;

                            // Fetch patch details
                            pEntry->get_Title(&title);
                            pEntry->get_Date(&date);
                            pEntry->get_Operation(&operation);

                            // We only want to display actual installation events
                            if (operation == uoInstallation) {
                                // Convert COM DATE variant to readable system time
                                SYSTEMTIME st;
                                VariantTimeToSystemTime(date, &st);

                                std::wcout << L"Date:  " << st.wYear << L"-" << st.wMonth << L"-" << st.wDay << L"\n";
                                std::wcout << L"Patch: " << title << L"\n";
                                std::wcout << L"\n";
                            }

                            // Free COM strings and objects
                            SysFreeString(title);
                            pEntry->Release();
                        }
                    }
                    pHistoryCollection->Release();
                }
            } else {
                std::wcout << L"No patch history found.\n";
            }
            pSearcher->Release();
        }
        pSession->Release();
    } else {
        std::cerr << "Failed to create IUpdateSession. Error code: " << hr << std::endl;
    }

    // Uninitialize COM library
    CoUninitialize();
    return 0;
}
