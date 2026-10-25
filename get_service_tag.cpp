#include <windows.h>
#include <wbemidl.h>
#include <iostream>
#include <comdef.h>

#pragma comment(lib, "wbemuuid.lib")

void GetDellServiceTag() {
    HRESULT hres;

    // 1. Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cout << "Failed to initialize COM. Error: " << hres << std::endl;
        return;
    }

    // 2. Set general COM security levels
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    // 3. Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        std::cout << "Failed to create IWbemLocator. Error: " << hres << std::endl;
        CoUninitialize();
        return;
    }

    // 4. Connect to root\cimv2
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL, NULL, NULL, 0, NULL, NULL, &pSvc
    );
    if (FAILED(hres)) {
        std::cout << "Could not connect to WMI. Error: " << hres << std::endl;
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // 5. Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE
    );

    // 6. Use the pointer to make a WMI query
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT SerialNumber FROM Win32_BIOS"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator
    );

    if (FAILED(hres)) {
        std::cout << "Query failed. Error: " << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // 7. Retrieve the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }
        VARIANT vtProp;
        VariantInit(&vtProp);
        hres = pclsObj->Get(L"SerialNumber", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
            std::wcout << L"Dell Service Tag (Serial Number): " << vtProp.bstrVal << std::endl;
        }
	VariantClear(&vtProp);
        pclsObj->Release();
    }

    // 8. Cleanup
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

void GetDellAssetTag() {
    HRESULT hres;

    // 1. Initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cout << "Failed to initialize COM. Error: " << hres << std::endl;
        return;
    }

    // 2. Set general COM security levels
    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    // 3. Obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, NULL,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        std::cout << "Failed to create IWbemLocator. Error: " << hres << std::endl;
        CoUninitialize();
        return;
    }

    // 4. Connect to root\cimv2
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL, NULL, NULL, 0, NULL, NULL, &pSvc
    );
    if (FAILED(hres)) {
        std::cout << "Could not connect to WMI. Error: " << hres << std::endl;
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // 5. Set security levels on the proxy
    hres = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE
    );

    // 6. Use the pointer to make a WMI query
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT SMBIOSAssetTag FROM Win32_SystemEnclosure"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator
    );

    if (FAILED(hres)) {
        std::cout << "Query failed. Error: " << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // 7. Retrieve the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (0 == uReturn) {
            break;
        }
        VARIANT vtProp;
        VariantInit(&vtProp);
        hres = pclsObj->Get(L"SMBIOSAssetTag", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
            std::wcout << L"Dell Asset Tag (Barcode): " << vtProp.bstrVal << std::endl;
        }
	VariantClear(&vtProp);
        pclsObj->Release();
    }

    // 8. Cleanup
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();
}

int main() {
    GetDellServiceTag();
    GetDellAssetTag();
    return 0;
}

