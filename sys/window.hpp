#pragma once

#include <string>
#include <winrt/Windows.Web.UI.Interop.h>

namespace WebView
{

class Window final
{
public:
    Window(const std::string& title, SIZE size, bool resizable);

public:
    ~Window();

    int Run(void* webview) noexcept;
    int NavigateToUrl(const std::string& url);
    int NavigateToString(const std::string& html);
    void EvaluateScript(const std::string& script);

private:
    static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void _WaitForControl() noexcept;
    winrt::Windows::Foundation::Rect _GetBounds();
    void _UpdateBounds();
    LRESULT _HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    void* m_owner;
    winrt::Windows::Web::UI::Interop::WebViewControlProcess m_process;
    winrt::Windows::Web::UI::Interop::WebViewControl m_control = nullptr;
};

}