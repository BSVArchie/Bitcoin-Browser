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
                      public CefDisplayHandler,
                      public CefLoadHandler {
public:
    SimpleHandler() {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        #if defined(OS_WIN)
        SetWindowText(browser->GetHost()->GetWindowHandle(), std::wstring(title).c_str());
        #endif
    }

    void OnLoadError(CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefFrame> frame,
                    ErrorCode errorCode,
                    const CefString& errorText,
                    const CefString& failedUrl) override {
        std::wcout << L"❌ Load error: " << std::wstring(failedUrl)
                << L" - " << std::wstring(errorText) << std::endl;

        // Only handle top-level frame
        if (frame->IsMain()) {
            std::string html = "<html><body><h1>Failed to load</h1><p>URL: " +
                            failedUrl.ToString() + "</p><p>Error: " +
                            errorText.ToString() + "</p></body></html>";

            // Basic URL-encode (you can replace with a proper encoder if needed)
            std::string encoded_html;
            for (char c : html) {
                if (isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '.' || c == '-' || c == '_' || c == ':')
                    encoded_html += c;
                else {
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
                    encoded_html += buf;
                }
            }

            std::string data_url = "data:text/html," + encoded_html;
            frame->LoadURL(data_url);
        }
    }

    void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
        std::cout << "📡 Loading state: " << (isLoading ? "loading..." : "done") << std::endl;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        std::cout << "🛠️ Browser created, opening DevTools..." << std::endl;

        // // Open DevTools window
        // CefWindowInfo window_info;
        // #if defined(OS_WIN)
        //     window_info.SetAsPopup(nullptr, "DevTools");
        // #endif

        //     browser->GetHost()->ShowDevTools(window_info, nullptr, CefBrowserSettings(), CefPoint());
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
        command_line->AppendSwitch("disable-gpu-shader-disk-cache");
        command_line->AppendSwitchWithValue("use-gl", "disabled");
        command_line->AppendSwitchWithValue("use-angle", "none");

    }

    void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();

        // Register window class
        HINSTANCE instance = GetModuleHandle(nullptr);

        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"BitcoinBrowserWndClass";
        RegisterClass(&wc);

        // Create native Win32 window
        RECT rect;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

        HWND hwnd = CreateWindow(
            L"BitcoinBrowserWndClass",    // Class name
            L"Bitcoin Browser",           // Window title
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,                      // No parent
            nullptr,                      // No menu
            instance,                     // ✅ Use instance here
            nullptr
        );

        // Embed Chromium browser into that window
        CefWindowInfo window_info;
        // window_info.SetAsPopup(nullptr, "Bitcoin Browser");
        window_info.SetAsChild(hwnd, CefRect(0, 0, rect.right - rect.left, rect.bottom - rect.top));

        CefBrowserSettings browser_settings;
        CefRefPtr<SimpleHandler> handler = new SimpleHandler();

        std::string url = "http://localhost:5137";
        // std::string url = "data:text/html,<html><body><h1>Hello from CEF!</h1></body></html>";
        // std::string url = "https://www.google.com";
        std::cout << "Attempting to load URL: " << url << std::endl;
        bool result = CefBrowserHost::CreateBrowser(
            window_info, handler, url, browser_settings, nullptr, nullptr);
        std::cout << "CreateBrowser returned: " << (result ? "true" : "false") << std::endl;
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
    settings.remote_debugging_port = 9222;

    CefString(&settings.resources_dir_path).FromWString(L"D:\\BSVProjects\\Browser-Project\\Bitcoin-Browser\\cef-native\\build\\bin\\Release");
    CefString(&settings.locales_dir_path).FromWString(L"D:\\BSVProjects\\Browser-Project\\Bitcoin-Browser\\cef-native\\build\\bin\\Release\\locales");
    CefString(&settings.browser_subprocess_path).FromWString(L"D:\\BSVProjects\\Browser-Project\\Bitcoin-Browser\\cef-native\\build\\bin\\Release\\BitcoinBrowserShell.exe");


    std::cout << "Initializing CEF..." << std::endl;
    bool success = CefInitialize(main_args, settings, app, nullptr);
    std::cout << "CefInitialize success: " << (success ? "true" : "false") << std::endl;

    // ✅ Message loop
    CefRunMessageLoop();
    // MSG msg;
    // while (GetMessage(&msg, nullptr, 0, 0)) {
    //     TranslateMessage(&msg);
    //     DispatchMessage(&msg);
    // }

    CefShutdown();
    return 0;
}
