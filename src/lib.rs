extern crate modern_webview_sys as ffi;
extern crate widestring;

use std::ffi::CString;
use std::os::raw::*;
use ffi::*;

pub enum Content<S: Into<String>> {
    Html(S),
    Url(S)
}

pub enum Event {
    ScriptNotify()
}

type Callback<'a> = FnMut(&mut WebView, &Event) + 'a;

pub struct WebView {
    window: *mut c_void
}

struct Container<'a> {
    webview: WebView,
    callback: Box<Callback<'a>>
}

type Result<T> = std::result::Result<T, &'static str>;

impl WebView {

}

impl Drop for WebView {
    fn drop(&mut self) {
        unsafe {
            webview_free(self.window)
        }
    }
}

pub fn webview<'a, S: Into<String>, F>(
    title: &str, content: Content<S>, size: (i32, i32), resizable: bool, callback: F) -> Result<()> where F: FnMut(&mut WebView, &Event) + 'a {

    let title = CString::new(title).unwrap();

    let content = match content {
        Content::Url(url) => (CString::new(url.into()).unwrap(), ContentType_Url),
        Content::Html(html) => (CString::new(html.into()).unwrap(), ContentType_Html)
    };

    let window: *mut c_void = unsafe {
        webview_new(title.as_ptr(), content.0.as_ptr(), content.1, size.0, size.1, resizable)
    };

    if window.is_null() {
        return Err("Window not created.");
    }

    let webview = WebView { window };
    let container = Box::new(Container { webview, callback: Box::new(callback) });

    let result: i32 = unsafe {
        let webview_ptr = Box::into_raw(container);
        webview_run(window, webview_ptr as *mut c_void)
    };

    if result == 0 {
        Ok(())
    } else {
        Err("WebView did not exit successfully.")
    }
}

#[no_mangle]
pub extern "C" fn webview_callback(webview_ptr: *mut c_void) {
    let container = unsafe { (webview_ptr as *mut Container).as_mut().unwrap() };
    let e = Event::ScriptNotify();
    (container.callback)(&mut container.webview, &e);
}