#include "common.hpp"
#include "webview.hpp"
#include "window.hpp"

using WebView::Window;

void* webview_new(
    const char* const title,
    const char* const url,
    const char* html,
    const int32_t width,
    const int32_t height,
    const bool resizable) noexcept
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    Window* window = nullptr;

    try
    {
        if (url != nullptr)
        {
            window = new Window(title, url, { width, height }, resizable);
        }
        // else if (html != nullptr)
        // {
        // 	window = new Window(title, html, { width, height }, resizable);
        // }
    }
    catch (...)
    {
    }

    return window;
}

int webview_run(void* window) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    return internalWindow->Run();
}

void webview_free(void* window) noexcept
{
    auto internalWindow = reinterpret_cast<Window*>(window);
    delete internalWindow;
}