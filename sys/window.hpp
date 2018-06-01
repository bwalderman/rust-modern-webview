#pragma once

#include <string>
#include <winrt/Windows.Web.UI.Interop.h>

namespace WebView
{

class Window final
{
public:
    Window(
        const std::string& title,
        const std::string& url,
        SIZE size,
        bool resizable);
    ~Window();

    int Run() noexcept;

private:
    static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    winrt::Windows::Foundation::Rect _GetBounds();
    void _UpdateBounds();
    LRESULT _HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    winrt::Windows::Web::UI::Interop::WebViewControlProcess m_process;
    winrt::Windows::Web::UI::Interop::WebViewControl m_control = nullptr;
};

}