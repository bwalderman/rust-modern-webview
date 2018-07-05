#pragma once

#include <cstdint>

extern "C"
{
    enum class ContentType : uint32_t
    {
        Url = 1,
        Html = 2
    };

    void* webview_new(
        const char* title,
        const char* const content,
        const ContentType contentType,
        int32_t width,
        int32_t height,
        bool resizable) noexcept;
    void webview_free(void* window) noexcept;

    int webview_run(void* window, void* webview) noexcept;
    int webview_eval_script(void* window, const char* script, char** value) noexcept;
    void webview_string_free(const char* str) noexcept;
}
