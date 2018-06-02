extern crate modern_webview_sys as ffi;
extern crate widestring;

use std::fmt::Display;
use std::ffi::CString;
use std::os::raw::*;
use std::ptr;

use ffi::*;

pub struct Size(pub i32, pub i32);

pub enum Content<S: Into<String>> {
    Html(S),
    Url(S)
}

pub enum Event {

}

pub struct WebView {
    window: *mut c_void
}

type Result<T> = std::result::Result<T, &'static str>;

impl WebView {
    pub(self) fn new<S: Into<String>>(title: &str, content: Content<S>, size: Size, resizable: bool) -> Result<WebView> {

        let mut _window: *mut c_void = ptr::null_mut();
        
        let title = CString::new(title).unwrap();
        
        match content {
            Content::Url(url) => {
                let url = CString::new(url.into()).unwrap();
                unsafe {
                    _window = webview_new(title.as_ptr(), url.as_ptr(), ContentType_Url, size.0, size.1, resizable);
                };
            },
            Content::Html(html) => {
                let html = CString::new(html.into()).unwrap();
                unsafe {
                    _window = webview_new(title.as_ptr(), html.as_ptr(), ContentType_Html, size.0, size.1, resizable);
                };
            }
        }

        if _window.is_null() {
            return Err("Window not created.");
        }

        Ok(WebView { window: _window })
    }

    pub(self) fn run<F>(&mut self, callback: F) -> Result<()> where F: FnOnce(&mut WebView, &Event) {
        let mut _result: i32 = 0;
        unsafe {
            _result = webview_run(self.window)
        }
        if _result == 0 {
            Ok(())
        } else {
            Err("WebView did not exit successfully.")
        }
    }
}

impl Drop for WebView {
    fn drop(&mut self) {
        unsafe {
            webview_free(self.window)
        }
    }
}

pub fn webview<S: Into<String>, F>(title: &str, content: Content<S>, size: Size, resizable: bool, callback: F) -> Result<()> where F: FnOnce(&mut WebView, &Event) {
    WebView::new(title, content, size, resizable)?.run(callback)
}