#pragma once

#include <cstdint>
#include <windows.h>

extern "C"
{
    enum class ContentType : uint32_t
    {
        Url = 1,
        Html = 2
    };

    enum class EventType : uint32_t
    {
        None,
        Quit,
        DOMContentLoaded,
        ScriptNotify
    };

    HRESULT webview_new(const char* title, int32_t width, int32_t height, bool resizable, void** window) noexcept;
    void webview_free(void* window) noexcept;
    void webview_string_free(const char* str) noexcept;
    HRESULT webview_navigate(void* window, void* webview, const char* content, ContentType contentType) noexcept;
    HRESULT webview_navigate_with_streamresolver(void* window, void* webview, const char* source) noexcept;
    HRESULT webview_loop(void* window, bool blocking, EventType* event, char** data) noexcept;
    HRESULT webview_dispatch(void* window, void* webview, void* callback) noexcept;
    HRESULT webview_eval_script(void* window, const char* script, char** value) noexcept;
    HRESULT webview_inject_css(void* window, const char* css) noexcept;
    HRESULT webview_get_error_message(char** message) noexcept;
}
