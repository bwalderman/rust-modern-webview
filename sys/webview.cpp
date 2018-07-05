#include "common.hpp"
#include "webview.hpp"
#include <string>
#include <winrt/Windows.Web.UI.Interop.h>

extern "C"
{
    extern void webview_generic_callback(void* webview, uint32_t event);
    extern void webview_script_notify_callback(void* webview, const char* value);
}

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI::Interop;
using namespace Windows::Web::UI;

// Helper functions for WebView Window class
namespace
{
    constexpr uint32_t DOMCONTENTLOADED = 1;

    std::wstring WideStringFromString(const std::string& narrow)
    {
        std::wstring wide;

        const int charCount = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, nullptr, 0);
        if (charCount > 0)
        {
            std::unique_ptr<wchar_t[]> buffer(new wchar_t[charCount]);
            if (MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, buffer.get(), charCount) == 0)
            {
                winrt::throw_last_error();
            }

            wide = buffer.get();
        }

        return wide;
    }

    template <typename T> T default() {
        return nullptr;
    }

    template <> winrt::hstring default<winrt::hstring>() {
        return winrt::hstring {};
    } 

    template <typename T>
    T AwaitAsyncOperation(IAsyncOperation<T>& operation)
    {
        using unique_handle = std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)>;

        T result = default<T>();

        unique_handle ready(CreateEvent(nullptr, FALSE, FALSE, nullptr), &::CloseHandle);
        if (ready == nullptr)
        {
            winrt::throw_last_error();
        }

        operation.Completed([ready = ready.get(), &result](auto operation, auto /*status*/)
        {
            result = operation.GetResults();
            SetEvent(ready);
        });

        DWORD index = 0;
        HANDLE handles[] = { ready.get() };
        winrt::check_hresult(CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES | COWAIT_DISPATCH_CALLS | COWAIT_INPUTAVAILABLE, INFINITE, _countof(handles), handles, &index));

        return result;
    }
}

// Window class
namespace
{
    class Window final
    {
    public:
        Window(const std::string& title, SIZE size, bool resizable)
        {
            HINSTANCE hInstance = GetModuleHandle(nullptr);
            if (hInstance == nullptr)
            {
                winrt::throw_last_error();
            }

            WNDCLASSEXW wcex;
            wcex.cbSize = sizeof(WNDCLASSEX);
            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = Window::s_WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = hInstance;
            wcex.hIcon = nullptr;
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = L"WebViewControlWindow";
            wcex.hIconSm = nullptr;

            if (::RegisterClassExW(&wcex) == 0)
            {
                winrt::throw_last_error();
            }

            const auto titleWide = WideStringFromString(title);

            HWND hwnd = ::CreateWindowW(
                L"WebViewControlWindow", 
                titleWide.c_str(),
                resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME),
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                size.cx,
                size.cy,
                nullptr,
                nullptr,
                hInstance,
                reinterpret_cast<LPVOID>(this));

            if (hwnd == nullptr)
            {
                winrt::throw_last_error();
            }

            m_process = WebViewControlProcess();
            
            const Rect bounds = _GetBounds();
            auto op = m_process.CreateWebViewControlAsync(reinterpret_cast<int64_t>(hwnd), bounds);

            m_control = AwaitAsyncOperation(op);
            m_control.IsVisible(true);

            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
        }

    public:
        ~Window()
        {
            m_process.Terminate();
        }

        int Run(void* webview) noexcept
        {
            m_owner = webview;

            auto domContentLoadedRevoker = m_control.DOMContentLoaded(winrt::auto_revoke, { this, &Window::OnDOMContentLoaded });
            auto scriptNotifyRevoker = m_control.ScriptNotify(winrt::auto_revoke, { this, &Window::OnScriptNotify });

            MSG msg;
            while (::GetMessage(&msg, nullptr, 0, 0))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }

            return (int)msg.wParam;
        }

        void NavigateToUrl(const std::string& url)
        {
            Uri uri(winrt::to_hstring(url.c_str()));
            m_control.Navigate(uri);
        }

        void NavigateToString(const std::string& html)
        {
            m_control.NavigateToString(winrt::to_hstring(html));
        }

        std::string EvaluateScript(const std::string& script)
        {
            auto op = (m_control.InvokeScriptAsync(winrt::to_hstring("eval"), { winrt::to_hstring(script) }));
            auto value = AwaitAsyncOperation(op);
            return winrt::to_string(value);
        }

    private:
        static LRESULT CALLBACK s_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            Window* window = nullptr;

            if (msg == WM_NCCREATE)
            {
                CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
                window = reinterpret_cast<Window*>(create->lpCreateParams);
                ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));

                window->m_hwnd = hwnd;
            }
            else
            {
                window = reinterpret_cast<Window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }

            if (window != nullptr)
            {
                return window->_HandleMessage(msg, wParam, lParam);
            }

            return ::DefWindowProc(hwnd, msg, wParam, lParam);
        }
        
        winrt::Windows::Foundation::Rect _GetBounds()
        {
            RECT clientRect;
            ::GetClientRect(m_hwnd, &clientRect);

            Rect bounds;
            bounds.X = static_cast<float>(clientRect.left);
            bounds.Y = static_cast<float>(clientRect.top);
            bounds.Width = static_cast<float>(clientRect.right - clientRect.left);
            bounds.Height = static_cast<float>(clientRect.bottom - clientRect.top);
            return bounds;
        }
        
        void _UpdateBounds()
        {
            if (m_control)
            {
                m_control.Bounds(_GetBounds());
            }
        }

        LRESULT _HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
        {
            switch (msg)
            {
            case WM_DESTROY:
                ::PostQuitMessage(0);
                break;
            case WM_SIZE:
                _UpdateBounds();
                break;
            default:
                return DefWindowProc(m_hwnd, msg, wParam, lParam);
            }

            return 0;
        }

        void OnDOMContentLoaded(const IWebViewControl&, const WebViewControlDOMContentLoadedEventArgs&)
        {
            webview_generic_callback(m_owner, DOMCONTENTLOADED);
        }

        void OnScriptNotify(const IWebViewControl&, const WebViewControlScriptNotifyEventArgs& args)
        {
            webview_script_notify_callback(m_owner, winrt::to_string(args.Value()).c_str());
        }

        HWND m_hwnd = nullptr;
        void* m_owner = nullptr;
        winrt::Windows::Web::UI::Interop::WebViewControlProcess m_process;
        winrt::Windows::Web::UI::Interop::WebViewControl m_control = nullptr;
    };
}

// Helper functions for public API
namespace
{
    INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;

    bool IsValidContentType(const ContentType type)
    {
        return type == ContentType::Url || type == ContentType::Html;
    }

    void EnsureInitialized()
    {
        BOOL success = InitOnceExecuteOnce(&s_initOnce, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

            return TRUE;
        }, nullptr /*Parameter*/, nullptr /*Context*/);
        
        if (!success)
        {
            winrt::throw_last_error();
        }
    }

    WebViewResult MapException(void* window, const std::function<void(Window&)>& func) noexcept
    {
        try
        {
            Window& internalWindow = *reinterpret_cast<Window*>(window);
            func(internalWindow);
        }
        catch (...)
        {
            return WebViewResult::InternalError;
        }

        return WebViewResult::Success;
    }
}

// Public API

WebViewResult webview_new(
    const char* const title,
    const char* const content,
    const ContentType contentType,
    const int32_t width,
    const int32_t height,
    const bool resizable,
    void** window) noexcept
{
    *window = nullptr;

    if (title == nullptr || content == nullptr || width <= 0 || height <= 0 || !IsValidContentType(contentType) || window == nullptr)
    {
        return WebViewResult::InvalidArgument;
    }

    try
    {
        EnsureInitialized();

        std::unique_ptr<Window> localWindow(new Window(title, { width, height }, resizable));

        if (contentType == ContentType::Url)
        {
            localWindow->NavigateToUrl(content);
        }
        else if (contentType == ContentType::Html)
        {
            localWindow->NavigateToString(content);
        }

        *window = localWindow.release();
    }
    catch (...)
    {
        return WebViewResult::InternalError;
    }
    
    return WebViewResult::Success;
}

void webview_free(void* window) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    delete internalWindow;
}

WebViewResult webview_run(void* window, void* webview) noexcept
{
    if (window == nullptr || webview == nullptr)
    {
        return WebViewResult::InvalidArgument;
    }

    return MapException(window, [webview](Window& window)
    {
        window.Run(webview);
    });
}

WebViewResult webview_eval_script(void* window, const char* script, char** value) noexcept
{
    if (window == nullptr || script == nullptr || value == nullptr)
    {
        return WebViewResult::InvalidArgument;
    }

    return MapException(window, [script, value](Window& window)
    {
        *value = nullptr;
        auto ret = window.EvaluateScript(script);
        *value = new char[ret.length() + 1];
        ret.copy(*value, ret.length(), 0);
        (*value)[ret.length()] = '\0';
    });
}

void webview_string_free(const char* str) noexcept
{
    if (str != nullptr)
    {
        delete [] str;
    }
}