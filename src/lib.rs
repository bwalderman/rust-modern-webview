extern crate modern_webview_sys as ffi;
extern crate widestring;

use std::fmt::Display;
use std::ffi::CString;
use std::os::raw::*;
use std::ptr;

use ffi::*;

pub struct Size(pub i32, pub i32);

pub enum Content<S: Into<String> + Display> {
    Html(S),
    Url(S)
}

pub struct WebView {
    window: *mut c_void
}

impl WebView {
    pub fn new<S: Into<String> + Display>(title: &str, content: Content<S>, size: Size, resizable: bool) -> Result<WebView, &'static str> {

        let mut window: *mut c_void = ptr::null_mut();
        
        let title = CString::new(title).unwrap();
        
        match content {
            Content::Url(url) => {
                let url = CString::new(url.into()).unwrap();
                unsafe {
                    window = webview_new(title.as_ptr(), url.as_ptr(), ContentType_Url, size.0, size.1, resizable);
                };
            },
            Content::Html(html) => {
                let html = CString::new(html.into()).unwrap();
                unsafe {
                    window = webview_new(title.as_ptr(), html.as_ptr(), ContentType_Html, size.0, size.1, resizable);
                };
            }
        }

        if window.is_null() {
            return Err("Window not created.");
        }

        Ok(WebView { window })
    }

    pub fn run(&mut self) -> i32 {
        unsafe {
            webview_run(self.window)
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