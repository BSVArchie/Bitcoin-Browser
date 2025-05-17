// cef_browser_shell.cpp
#define CEF_ENABLE_SANDBOX 0

#include "cef_app.h"
#include "cef_client.h"
#include "cef_browser.h"
#include "cef_command_line.h"
#include "cef_life_span_handler.h"
#include "wrapper/cef_helpers.h"

#include <iostream>

class SimpleHandler : public CefClient,
                      public CefLifeSpanHandler,
                      public CefDisplayHandler {
public:
    SimpleHandler() {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        std::cout << "SimpleHandler::GetLifeSpanHandler" << std::endl;
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        std::cout << "SimpleHandler::GetDisplayHandler" << std::endl;
        return this;
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
        std::wcout << L"OnTitleChange: " << std::wstring(title) << std::endl;

        #if defined(OS_WIN)
        SetWindowText(hwnd, std::wstring(title).c_str());
        #endif
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleHandler);
};

class SimpleApp : public CefApp,
                  public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        std::cout << "SimpleApp::GetBrowserProcessHandler" << std::endl;
        return this;
    }

    void OnContextInitialized() override {
        std::cout << "OnContextInitialized called" << std::endl;
        CEF_REQUIRE_UI_THREAD();

        CefWindowInfo window_info;
        #if defined(OS_WIN)
        window_info.SetAsPopup(NULL, "Bitcoin Browser");
        #endif

        CefBrowserSettings browser_settings;
        CefRefPtr<SimpleHandler> handler = new SimpleHandler();

        std::string url = "http://localhost:5137";
        std::cout << "Attempting to load URL: " << url << std::endl;

        bool result = CefBrowserHost::CreateBrowser(
            window_info, handler, url, browser_settings, nullptr, nullptr);

        std::cout << "CreateBrowser returned: " << (result ? "true" : "false") << std::endl;
    }

private:
    IMPLEMENT_REFCOUNTING(SimpleApp);
};

int main(int argc, char* argv[]) {
    std::cout << "Shell starting..." << std::endl;

    CefMainArgs main_args(GetModuleHandle(nullptr));
    CefRefPtr<SimpleApp> app(new SimpleApp());

    // Initialize global command line
    CefCommandLine::CreateCommandLine()->InitFromString(::GetCommandLineW());

    // Add fallback switches for subprocesses
    CefCommandLine::GetGlobalCommandLine()->AppendSwitchWithValue("lang", "en-US");
    CefCommandLine::GetGlobalCommandLine()->AppendSwitch("disable-gpu");
    CefCommandLine::GetGlobalCommandLine()->AppendSwitch("disable-gpu-compositing");
    CefCommandLine::GetGlobalCommandLine()->AppendSwitch("enable-begin-frame-scheduling");
    CefCommandLine::GetGlobalCommandLine()->AppendSwitch("disable-gpu-shader-disk-cache");

    // 🧪 Print the raw switch flags
    std::cout << "Global command line string: "
            << CefCommandLine::GetGlobalCommandLine()->GetCommandLineString().ToString()
            << std::endl;

    if (CefCommandLine::GetGlobalCommandLine()->HasSwitch("type")) {
        std::cout << "Process type: "
                << CefCommandLine::GetGlobalCommandLine()->GetSwitchValue("type").ToString()
                << std::endl;
    } else {
        std::cout << "This is the browser process (no --type switch)." << std::endl;
    }

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    std::cout << "CefExecuteProcess returned: " << exit_code << std::endl;

    if (exit_code >= 0) {
        std::cout << "Subprocess exited early with code: " << exit_code << std::endl;
        std::cin.get();  // Pause so we can read it
        return exit_code;
    }

    CefSettings settings;
    settings.no_sandbox = true;
    CefString(&settings.log_file).FromASCII("debug.log");
    settings.log_severity = LOGSEVERITY_INFO;

    bool success = CefInitialize(main_args, settings, app, nullptr);
    std::cout << "CefInitialize success: " << (success ? "true" : "false") << std::endl;

    if (success) {
        CefRunMessageLoop();
        std::cout << "Exited CefRunMessageLoop." << std::endl;
        CefShutdown();
    } else {
        std::cerr << "CefInitialize failed." << std::endl;
        std::cin.get();  // Pause to read the error
    }

    return 0;
}
