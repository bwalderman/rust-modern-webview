#include "common.hpp"
#include "webview.hpp"
#include <string>
#include <deque>
#include <winrt/Windows.Web.UI.Interop.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Security.Cryptography.h>

extern "C"
{
    extern bool webview_get_content(void* webview, const char* path, const uint8_t** content, size_t* length);
    extern void webview_dispatch_callback(void* webview, void* callback);
}

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Windows::Security::Cryptography;
using namespace Windows::Web::UI::Interop;
using namespace Windows::Web::UI;
using namespace Windows::Web;

#define WM_APP_DISPATCH WM_APP + 1

// UriToStreamResolver class
namespace
{
    class UriToStreamResolver : public winrt::implements<UriToStreamResolver, IUriToStreamResolver>
    {
    public:
        UriToStreamResolver(void* webview) : m_webview(webview)
        {
        }

        IAsyncOperation<IInputStream> UriToStreamAsync(Uri uri) const
        {
            size_t length = 0;
            const uint8_t* content = nullptr;
            bool result = webview_get_content(m_webview, winrt::to_string(uri.Path()).c_str(), &content, &length);
            if (!result)
            {
                throw_hresult(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            }

            auto buffer = CryptographicBuffer::CreateFromByteArray(winrt::array_view<const uint8_t>(content, content + length - 1));
            auto stream = InMemoryRandomAccessStream();
            co_await stream.WriteAsync(buffer);
            co_return stream.GetInputStreamAt(0);
        }

    private:
        void* const m_webview;
    };
}

// Helper functions for WebView Window class
namespace
{
    struct EventInfo
    {
        EventType type;
        std::string data;
    };

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

    template <typename T>
    T AwaitAsyncOperation(IAsyncOperation<T>& operation)
    {
        winrt::handle ready(CreateEvent(nullptr, FALSE, FALSE, nullptr));
        winrt::check_bool(bool{ready});

        operation.Completed([ready = ready.get()](auto operation, auto status)
        {
            winrt::check_bool(::SetEvent(ready));
        });

        DWORD index = 0;
        HANDLE handles[] = { ready.get() };
        winrt::check_hresult(CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES | COWAIT_DISPATCH_CALLS | COWAIT_INPUTAVAILABLE, INFINITE, _countof(handles), handles, &index));

        return operation.GetResults();
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
            HINSTANCE hInstance = winrt::check_pointer(GetModuleHandle(nullptr));

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

            winrt::check_bool(::RegisterClassExW(&wcex));

            const auto titleWide = WideStringFromString(title);

            HWND hwnd = winrt::check_pointer(::CreateWindowW(
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
                reinterpret_cast<LPVOID>(this)));

            m_process = WebViewControlProcess();
            
            const Rect bounds = _GetBounds();
            auto op = m_process.CreateWebViewControlAsync(reinterpret_cast<int64_t>(hwnd), bounds);

            m_control = AwaitAsyncOperation(op);
            m_control.Settings().IsScriptNotifyAllowed(true);
            m_control.IsVisible(true);

            m_domContentLoadedRevoker = m_control.DOMContentLoaded(winrt::auto_revoke, { this, &Window::OnDOMContentLoaded });
            m_scriptNotifyRevoker = m_control.ScriptNotify(winrt::auto_revoke, { this, &Window::OnScriptNotify });

            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);
        }

    public:
        ~Window()
        {
            m_process.Terminate();
        }

        void Navigate(void* webview, const std::string& content, const ContentType contentType)
        {
            m_owner = webview;

            if (contentType == ContentType::Url)
            {
                Uri uri(winrt::to_hstring(content));
                m_control.Navigate(uri);
            }
            else if (contentType == ContentType::Html)
            {
                m_control.NavigateToString(winrt::to_hstring(content));
            }
        }

        void NavigateWithStreamResolver(void* webview, const std::string& path)
        {
            m_owner = webview;

            auto source = m_control.BuildLocalStreamUri(winrt::to_hstring("WebView"), winrt::to_hstring(path));
            auto resolver = winrt::make_self<UriToStreamResolver>(webview);
            m_control.NavigateToLocalStreamUri(source, resolver.as<IUriToStreamResolver>());
        }

        std::string EvaluateScript(const std::string& script)
        {
            auto op = (m_control.InvokeScriptAsync(winrt::to_hstring("eval"), { winrt::to_hstring(script) }));
            auto value = AwaitAsyncOperation(op);
            return winrt::to_string(value);
        }

        void InjectCss(const std::string& css)
        {
            if (!m_injectCssFunctionInitialized)
            {
                static constexpr char s_injectCss[] =
                    "window.__webview_injectCss = function __webview_injectCss(css) {"
                    "    const style = document.createElement('style');"
                    "    style.type = 'text/css';"
                    "    style.innerHTML = css;"
                    "    document.head.appendChild(style);"
                    "};";

                auto op = (m_control.InvokeScriptAsync(winrt::to_hstring("eval"), { winrt::to_hstring(s_injectCss) }));
                AwaitAsyncOperation(op);
                m_injectCssFunctionInitialized = true;
            }
            
            auto op = (m_control.InvokeScriptAsync(winrt::to_hstring("__webview_injectCss"), { winrt::to_hstring(css) }));
            AwaitAsyncOperation(op);
        }

        EventInfo Loop(const bool blocking)
        {
            if (m_events.size() > 0)
            {
                auto event = m_events.front();
                m_events.pop_front();
                return event;
            }

            if (blocking)
            {
                MSG msg;
                while (::GetMessage(&msg, nullptr, 0, 0))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);

                    if (m_events.size() > 0)
                    {
                        auto event = m_events.front();
                        m_events.pop_front();
                        return event;
                    }
                }

                // WM_QUIT
                return { EventType::Quit, "" };
            }
            else
            {
                MSG msg;
                if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);

                    if (m_events.size() > 0)
                    {
                        auto event = m_events.front();
                        m_events.pop_front();
                        return event;
                    }
                    else if (msg.message == WM_QUIT)
                    {
                        return { EventType::Quit, "" };
                    }

                    return { EventType::None, "" };
                }

                return { EventType::None, "" };
            }
        }

        void Dispatch(void* webview, void* callback)
        {
            auto info = std::make_unique<DispatchInfo>(DispatchInfo { webview, callback });
            PostMessage(m_hwnd, WM_APP_DISPATCH, 0, reinterpret_cast<LPARAM>(info.release()));
        }

    private:
        struct DispatchInfo
        {
            void* webview;
            void* callback;
        };

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

        void _HandleDispatch(LPARAM lParam)
        {
            std::unique_ptr<DispatchInfo> info(reinterpret_cast<DispatchInfo*>(lParam));
            webview_dispatch_callback(info->webview, info->callback);
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
            case WM_APP_DISPATCH:
                _HandleDispatch(lParam);
                break;
            default:
                return DefWindowProc(m_hwnd, msg, wParam, lParam);
            }

            return 0;
        }

        void OnDOMContentLoaded(const IWebViewControl&, const WebViewControlDOMContentLoadedEventArgs&)
        {
            m_events.push_back({ EventType::DOMContentLoaded, "" });
        }

        void OnScriptNotify(const IWebViewControl&, const WebViewControlScriptNotifyEventArgs& args)
        {
            m_events.push_back({ EventType::ScriptNotify, winrt::to_string(args.Value()) });
        }

        HWND m_hwnd = nullptr;
        void* m_owner = nullptr;
        winrt::Windows::Web::UI::Interop::WebViewControlProcess m_process;
        winrt::Windows::Web::UI::Interop::WebViewControl m_control = nullptr;
        bool m_injectCssFunctionInitialized = false;
        std::deque<EventInfo> m_events;
        winrt::event_revoker<IWebViewControl> m_domContentLoadedRevoker;
        winrt::event_revoker<IWebViewControl> m_scriptNotifyRevoker;
    };
}

// Helper functions for public API
namespace
{
    INIT_ONCE s_initOnce = INIT_ONCE_STATIC_INIT;
    static thread_local std::optional<winrt::hresult_error> s_error;

    bool IsValidContentType(const ContentType type)
    {
        return type == ContentType::Url || type == ContentType::Html;
    }

    void EnsureInitialized()
    {
        winrt::check_bool(InitOnceExecuteOnce(&s_initOnce, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

            return TRUE;
        }, nullptr /*Parameter*/, nullptr /*Context*/));
    }

    HRESULT MapException(void* window, const std::function<void(Window&)>& func) noexcept
    {
        s_error.reset();

        try
        {
            winrt::check_pointer(window);
            Window& internalWindow = *reinterpret_cast<Window*>(window);
            func(internalWindow);
        }
        catch (const winrt::hresult_error& err)
        {
            s_error = err;
            return err.code();
        }
        catch (...)
        {
            auto hr = to_hresult();
            s_error = winrt::hresult_error(hr);
            return hr;
        }

        return S_OK;
    }
}

// Public API

HRESULT webview_new(
    const char* const title,
    const int32_t width,
    const int32_t height,
    const bool resizable,
    void** window) noexcept
{
    s_error.reset();
    *window = nullptr;
    
    try
    {
        if (title == nullptr || width <= 0 || height <= 0 || window == nullptr)
        {
            winrt::throw_hresult(E_INVALIDARG);
        }

        EnsureInitialized();
        std::unique_ptr<Window> localWindow(new Window(title, { width, height }, resizable));
        *window = localWindow.release();
    }
    catch (const winrt::hresult_error& err)
    {
        s_error = err;
        return err.code();
    }
    catch (...)
    {
        auto hr = to_hresult();
        s_error = winrt::hresult_error(hr);
        return hr;
    }

    return S_OK;
}

void webview_free(void* window) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    delete internalWindow;
}

char* webview_string_new(const std::string& source)
{
    char* dest = new char[source.length() + 1];
    strncpy(dest, source.c_str(), source.length() + 1);
    return dest;
}

void webview_string_free(const char* str) noexcept
{
    if (str != nullptr)
    {
        delete [] str;
    }
}

HRESULT webview_navigate(void* window, void* webview, const char* content, ContentType contentType) noexcept
{
    return MapException(window, [webview, content, contentType](Window& window)
    {
        if (webview == nullptr || content == nullptr || !IsValidContentType(contentType))
        {
            winrt::throw_hresult(E_INVALIDARG);
        }

        window.Navigate(webview, content, contentType);
    });
}

HRESULT webview_navigate_with_streamresolver(void* window, void* webview, const char* source) noexcept
{
    return MapException(window, [webview, source](Window& window)
    {
        if (webview == nullptr || source == nullptr)
        {
            winrt::throw_hresult(E_INVALIDARG);
        }

        window.NavigateWithStreamResolver(webview, source);
    });
}

HRESULT webview_loop(void* window, bool blocking, EventType* event, char** data) noexcept
{
    *event = EventType::None;
    *data = nullptr;

    return MapException(window, [blocking, event, data](Window& window)
    {
        auto info = window.Loop(blocking);

        *event = info.type;
        *data = !info.data.empty() ? webview_string_new(info.data) : nullptr;
    });
}

HRESULT webview_dispatch(void* window, void* webview, void* callback) noexcept
{
    return MapException(window, [webview, callback](Window& window)
    {
        window.Dispatch(webview, callback);
    });
}

HRESULT webview_eval_script(void* window, const char* script, char** value) noexcept
{
    *value = nullptr;

    return MapException(window, [script, value](Window& window)
    {
        if (script == nullptr || value == nullptr)
        {
            winrt::throw_hresult(E_INVALIDARG);
        }
    
        auto ret = window.EvaluateScript(script);
        *value = webview_string_new(ret);
    });
}

HRESULT webview_inject_css(void* window, const char* css) noexcept
{
    return MapException(window, [css](Window& window)
    {
        if (css == nullptr)
        {
            winrt::throw_hresult(E_INVALIDARG);
        }

        window.InjectCss(css);
    });
}

HRESULT webview_get_error_message(char** message) noexcept
{
    *message = nullptr;
    HRESULT hr = S_OK;

    if (s_error)
    {
        auto err = *s_error;
        hr = err.code();

        *message = webview_string_new(winrt::to_string(err.message()));
    }

    return hr;
}