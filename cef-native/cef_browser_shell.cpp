#define CEF_ENABLE_SANDBOX 0

#include "cef_app.h"
#include "cef_client.h"
#include "cef_browser.h"
#include "cef_command_line.h"
#include "cef_life_span_handler.h"
#include "wrapper/cef_helpers.h"
#include <filesystem>

#include <windows.h>
#include <iostream>

class SimpleHandler : public CefClient,
                      public CefLifeSpanHandler,
                      public CefDisplayHandler {
public:
    SimpleHandler() {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        #if defined(OS_WIN)
        SetWindowText(browser->GetHost()->GetWindowHandle(), std::wstring(title).c_str());
        #endif
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleHandler);
};

class SimpleApp : public CefApp,
                  public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                   CefRefPtr<CefCommandLine> command_line) override {
    std::wcout << L"OnBeforeCommandLineProcessing for type: " << std::wstring(process_type) << std::endl;

    if (!command_line->HasSwitch("lang")) {
        std::wcout << L"Appending --lang=en-US" << std::endl;
        command_line->AppendSwitchWithValue("lang", "en-US");
    } else {
        std::wcout << L"--lang already present" << std::endl;
    }

    // Disable GPU and related features
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
    command_line->AppendSwitch("disable-gpu-process");
    command_line->AppendSwitch("disable-software-rasterizer");
    command_line->AppendSwitch("disable-gpu-shader-disk-cache");
    command_line->AppendSwitch("enable-begin-frame-scheduling");
    command_line->AppendSwitch("headless");

    // Optionally force software rendering via CPU
    command_line->AppendSwitchWithValue("use-angle", "none");
}

    void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();

        // Register native window class
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"BitcoinBrowserWndClass";
        RegisterClass(&wc);

        // Create main native Win32 window
        HWND hwnd = CreateWindowEx(
            0,
            wc.lpszClassName,
            L"Bitcoin Browser",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!hwnd) {
            std::cerr << "Failed to create native window." << std::endl;
            return;
        }

        // Embed Chromium browser into that window
        CefWindowInfo window_info;
        window_info.SetAsChild(hwnd, CefRect(0, 0, 1200, 800));

        CefBrowserSettings browser_settings;
        CefRefPtr<SimpleHandler> handler = new SimpleHandler();

        std::string url = "http://localhost:5137";
        CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleApp);
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    std::cout << "Shell starting..." << std::endl;

    CefMainArgs main_args(hInstance);
    CefRefPtr<SimpleApp> app(new SimpleApp());

    // ✅ Reconstruct command line and inject --lang if needed
    LPWSTR* argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::wstring final_cmd;
    bool has_lang = false;

    for (int i = 0; i < argc; ++i) {
        final_cmd += argv[i];
        final_cmd += L" ";

        if (std::wstring(argv[i]).find(L"--lang=") != std::wstring::npos) {
            has_lang = true;
        }
    }

    if (!has_lang) {
        final_cmd += L"--lang=en-US";
    }

    CefCommandLine::CreateCommandLine()->InitFromString(final_cmd);

    // ✅ Subprocess handling: ALWAYS include this block
    std::cout << "Running CefExecuteProcess..." << std::endl;
    // MessageBox(nullptr, L"About to run CefExecuteProcess", L"Debug", MB_OK);
    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    // MessageBox(nullptr, L"CefExecuteProcess finished", L"Debug", MB_OK);
    std::cout << "CefExecuteProcess returned: " << exit_code << std::endl;
    if (exit_code >= 0) {
        std::cout << "Exiting because this is a subprocess." << std::endl;
        return exit_code;
    }

    // ✅ Configure CEF settings
    CefSettings settings;
    settings.no_sandbox = true;
    settings.command_line_args_disabled = false;
    CefString(&settings.log_file).FromASCII("debug.log");
    settings.log_severity = LOGSEVERITY_INFO;
    std::wstring absPath = std::filesystem::absolute(".").wstring();
    CefString(&settings.resources_dir_path) = absPath;


    std::cout << "Initializing CEF..." << std::endl;
    bool success = CefInitialize(main_args, settings, app, nullptr);
    std::cout << "CefInitialize success: " << (success ? "true" : "false") << std::endl;

    // ✅ Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CefShutdown();
    return 0;
}
