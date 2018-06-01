#pragma once

#include <cstdint>

extern "C"
{
    void* webview_new(
        const char* title,
        const char* url,
        const char* html,
        int32_t width,
        int32_t height,
        bool resizable) noexcept;

    int webview_run(void* window) noexcept;

    void webview_free(void* window) noexcept;
}
