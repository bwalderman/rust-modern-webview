#pragma once

#include <cstdint>

extern "C"
{
    enum class ContentType : uint32_t
    {
        Url = 1,
        Html = 2
    };

    enum class WebViewResult : uint32_t
    {
        Success = 0,
        InvalidArgument = 1,
        InternalError = 2
    };

    WebViewResult webview_new(
        const char* title,
        const char* const content,
        const ContentType contentType,
        int32_t width,
        int32_t height,
        bool resizable,
        void** window) noexcept;
    void webview_free(void* window) noexcept;
    void webview_string_free(const char* str) noexcept;

    WebViewResult webview_run(void* window, void* webview) noexcept;
    WebViewResult webview_eval_script(void* window, const char* script, char** value) noexcept;
    WebViewResult webview_inject_css(void* window, const char* css) noexcept;
}
