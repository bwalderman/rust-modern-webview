extern crate modern_webview_sys as ffi;

use std::ffi::{ CString, CStr };
use std::os::raw::*;
use std::ptr;
use std::fmt;
use ffi::*;

pub enum Content<S: Into<String>> {
    Html(S),
    Url(S)
}

pub enum Event {
    DOMContentLoaded(),
    ScriptNotify(String)
}

use Error::*;

#[derive(Debug)]
pub enum Error {
    InvalidArgument(),
    InternalError(),
    UnknownError()
}

impl From<std::ffi::NulError> for Error {
    fn from (err: std::ffi::NulError) -> Error {
        InternalError()
    }
}

impl std::error::Error for Error {
    fn description(&self) -> &str {
        match *self {
            InvalidArgument() => "Invalid argument.",
            InternalError() => "An internal error occurred.",
            UnknownError() => "An unknown error occurred."
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use std::error::Error;
        write!(f, "{}", self.description())
    }
}

pub type Result<T> = std::result::Result<T, Error>;

type Callback<'a> = FnMut(&mut WebView, Event) + 'a;

pub struct WebView {
    window: *mut c_void
}

struct Container<'a> {
    webview: WebView,
    callback: Box<Callback<'a>>
}

impl WebView {
    pub fn eval_script(&mut self, script: &str) -> Result<String> {
        
        let mut ret: *mut c_char = ptr::null_mut();

        let script = CString::new(script)?;
        let result: u32 = unsafe {    
            webview_eval_script(self.window, script.as_ptr(), &mut ret)
        };

        let ret = map_result(ret, result)?;

        let value = unsafe {
            let value = CStr::from_ptr(ret).to_string_lossy().into_owned();
            webview_string_free(ret);
            value
        };
        
        Ok(value)
    }

    pub fn inject_css(&mut self, css: &str) -> Result<()> {

        let css = CString::new(css)?;
        let result: u32 = unsafe {
            webview_inject_css(self.window, css.as_ptr())
        };

        map_result((), result)
    }
}

fn map_result<T>(value: T, result: u32) -> Result<T> {
    match result {
        WebViewResult_Success => Ok(value),
        WebViewResult_InvalidArgument => Err(InvalidArgument()),
        WebViewResult_InternalError => Err(InternalError()),
        _ => Err(UnknownError())
    }
}

pub fn webview<'a, S: Into<String>, F>(
    title: &str, content: Content<S>, size: (i32, i32), resizable: bool, callback: F) -> Result<()> where F: FnMut(&mut WebView, Event) + 'a {

    let title = CString::new(title)?;

    let content = match content {
        Content::Url(url) => (CString::new(url.into())?, ContentType_Url),
        Content::Html(html) => (CString::new(html.into())?, ContentType_Html)
    };

    let mut window: *mut c_void = ptr::null_mut();
    let result: u32 = unsafe {
        webview_new(title.as_ptr(), content.0.as_ptr(), content.1, size.0, size.1, resizable, &mut window)
    };

    let window = map_result(window, result)?;
    let webview = WebView { window };
    let container = Box::new(Container { webview, callback: Box::new(callback) });

    let result: u32 = unsafe {
        let webview_ptr = Box::into_raw(container);
        webview_run(window, webview_ptr as *mut c_void)
    };

    unsafe {
        webview_free(window)
    };

    map_result((), result)
}

const DOMCONTENTLOADED: u32 = 1;

fn invoke_callback(webview_ptr: *mut c_void, event: Event) {
    let container = unsafe {
        (webview_ptr as *mut Container).as_mut().unwrap()
    };

    (container.callback)(&mut container.webview, event);
}

#[no_mangle]
pub extern "C" fn webview_generic_callback(webview_ptr: *mut c_void, event: u32) {
    match event {
        DOMCONTENTLOADED => invoke_callback(webview_ptr, Event::DOMContentLoaded()),
        _ => {} 
    };
}

#[no_mangle]
pub extern "C" fn webview_script_notify_callback(webview_ptr: *mut c_void, value: *mut c_char) {
    let value = unsafe { CStr::from_ptr(value).to_string_lossy().into_owned() };
    invoke_callback(webview_ptr, Event::ScriptNotify(value));
}