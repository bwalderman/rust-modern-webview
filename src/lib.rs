extern crate modern_webview_sys as ffi;
extern crate widestring;

use std::ffi::{ CString, CStr };
use std::os::raw::*;
use ffi::*;

pub enum Content<S: Into<String>> {
    Html(S),
    Url(S)
}

pub enum Event {
    DOMContentLoaded(),
    ScriptNotify(String)
}

type Callback<'a> = FnMut(&mut WebView, Event) + 'a;

pub struct WebView {
    window: *mut c_void
}

struct Container<'a> {
    webview: WebView,
    callback: Box<Callback<'a>>
}

type Result<T> = std::result::Result<T, &'static str>;

impl WebView {
    pub fn eval_script<S: Into<String>>(&mut self, script: S) -> Result<()> {
        
        let script = CString::new(script.into()).unwrap();
        let result: i32 = unsafe {
            webview_eval_script(self.window, script.as_ptr())
        };

        if result == 0 {
            Ok(())
        } else {
            Err("Could not execute script")
        }
    }
}

pub fn webview<'a, S: Into<String>, F>(
    title: &str, content: Content<S>, size: (i32, i32), resizable: bool, callback: F) -> Result<()> where F: FnMut(&mut WebView, Event) + 'a {

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

    unsafe {
        webview_free(window)
    }

    if result == 0 {
        Ok(())
    } else {
        Err("WebView did not exit successfully.")
    }
}

const DOMCONTENTLOADED: u32 = 1;

#[no_mangle]
pub extern "C" fn webview_generic_callback(webview_ptr: *mut c_void, event: u32) {
    let container = unsafe {
        (webview_ptr as *mut Container).as_mut().unwrap()
    };
    
    match event {
        DOMCONTENTLOADED => {
            (container.callback)(&mut container.webview, Event::DOMContentLoaded());
        },
        _ => {} 
    };
}

#[no_mangle]
pub extern "C" fn webview_script_notify_callback(webview_ptr: *mut c_void, value: *mut c_char) {
    let container = unsafe {
        (webview_ptr as *mut Container).as_mut().unwrap()
    };

    let value = unsafe { CStr::from_ptr(value).to_string_lossy().into_owned() };
    (container.callback)(&mut container.webview, Event::ScriptNotify(value));
}