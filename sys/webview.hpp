#pragma once

#include <cstdint>

extern "C"
{
    void* webview_new(
        const wchar_t* title,
        const wchar_t* url,
        const char* html,
        int32_t width,
        int32_t height,
        bool resizable) noexcept;

    int webview_run(void* window) noexcept;

    void webview_free(void* window) noexcept;
}
