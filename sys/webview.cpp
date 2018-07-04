#include "common.hpp"
#include "webview.hpp"
#include "window.hpp"

using WebView::Window;

namespace
{
    static bool IsValidContentType(const ContentType type)
    {
        return type == ContentType::Url || type == ContentType::Html;
    }
}

void* webview_new(
    const char* const title,
    const char* const content,
    const ContentType contentType,
    const int32_t width,
    const int32_t height,
    const bool resizable) noexcept
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    Window* window = nullptr;

    if (title != nullptr &&
        content != nullptr &&
        width >= 0 && height >= 0 &&
        IsValidContentType(contentType))
    {
        try
        {
            window = new Window(title, { width, height }, resizable);

            if (contentType == ContentType::Url)
            {
                window->NavigateToUrl(content);
            }
            else if (contentType == ContentType::Html)
            {
                window->NavigateToString(content);
            }
        }
        catch (...)
        {
        }
    }

    return window;
}

int webview_run(void* window, void* webview) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    return internalWindow->Run(webview);
}

void webview_free(void* window) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    delete internalWindow;
}

int webview_eval_script(void* window, const char* script) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);

    try
    {
        internalWindow->EvaluateScript(script);
    }
    catch (...)
    {
        return 1; // TODO: Error codes
    }
    
    return 0;
}