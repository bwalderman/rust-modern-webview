extern crate modern_webview_sys as ffi;
extern crate include_dir;

use std::ffi::{ CString, CStr };
use std::path::{ Path };
use std::os::raw::*;
use std::ptr;
use std::fmt;
use ffi::*;

use include_dir::Dir;

pub enum Content<'a, S: Into<String>> {
    Html(S),
    Url(S),
    Dir(Dir<'a>, S)
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
    fn from (_: std::ffi::NulError) -> Error {
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

pub struct WebView<'a> {
    window: *mut c_void,
    dir: Option<include_dir::Dir<'a>>
}

struct Container<'a> {
    webview: WebView<'a>,
    callback: Box<Callback<'a>>
}

impl<'a> WebView<'a> {
    pub fn eval_script(&mut self, script: &str) -> Result<String> {

        let script = CString::new(script)?;

        let ret = ffi_result(unsafe {
            let mut ret: *mut c_char = ptr::null_mut();
            let result = webview_eval_script(self.window, script.as_ptr(), &mut ret);
            (ret, result)
        })?;

        let value = unsafe {
            let value = CStr::from_ptr(ret).to_string_lossy().into_owned();
            webview_string_free(ret);
            value
        };
        
        Ok(value)
    }

    pub fn inject_css(&mut self, css: &str) -> Result<()> {

        let css = CString::new(css)?;
        ffi_result(unsafe {
            let result = webview_inject_css(self.window, css.as_ptr());
            ((), result)
        })
    }
}

fn ffi_result<T>(result: (T, u32)) -> Result<T> {
    match result {
        (value, WebViewResult_Success) => Ok(value),
        (_, err) => match err {
            WebViewResult_InvalidArgument => Err(InvalidArgument()),
            WebViewResult_InternalError => Err(InternalError()),
            _ => Err(UnknownError())
        }
    }
}

pub fn webview<'a, S: Into<String>, F>(
    title: &str, content: Content<S>, size: (i32, i32), resizable: bool, callback: F) -> Result<()> where F: FnMut(&mut WebView, Event) + 'a {

    let title = CString::new(title)?;
    
    let window = ffi_result(unsafe {
        let mut window: *mut c_void = ptr::null_mut();
        let result = webview_new(title.as_ptr(), size.0, size.1, resizable, &mut window);
        (window, result)
    })?;

    let mut container = Box::new(Container { 
        webview: WebView { window, dir: None },
        callback: Box::new(callback)
    });

    let result = ffi_result(unsafe {
        match content {
            Content::Url(url) => {
                let url = CString::new(url.into())?;
                let webview_ptr = Box::into_raw(container) as *mut c_void;
                let result = webview_run(window, webview_ptr , url.as_ptr(), ContentType_Url);
                ((), result)
            }
            Content::Html(html) => {
                let html = CString::new(html.into())?;
                let webview_ptr = Box::into_raw(container) as *mut c_void;
                let result = webview_run(window, webview_ptr, html.as_ptr(), ContentType_Html);
                ((), result)                
            }
            Content::Dir(dir, source) => {
                container.webview.dir = Some(dir);
                let source = CString::new(source.into())?;
                let webview_ptr = Box::into_raw(container) as *mut c_void;
                let result = webview_run_with_streamresolver(window, webview_ptr, source.as_ptr());
                ((), result)
            }
        }
    });

    unsafe {
        webview_free(window)
    };

    result
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

#[no_mangle]
pub extern "C" fn webview_get_content(webview_ptr: *mut c_void, source: *const c_char, content: *mut *const u8, length: *mut usize) -> u32 {
    let container = unsafe {
        (webview_ptr as *mut Container).as_mut().unwrap()
    };
    
    unsafe {
        *content = ptr::null();
        *length = 0;
    };

    match container.webview.dir {
        Some(dir) => {
            let source = unsafe { CStr::from_ptr(source).to_str().unwrap() };
            
            let path = Path::new(source);
            let path = if path.starts_with("/") {
                path.strip_prefix("/").unwrap()
            } else {
                path
            };
            
            if dir.contains(path) {
                let file = dir.get_file(path.to_str().unwrap()).unwrap();
                let body = file.contents();
                unsafe {
                    *content = body.as_ptr();
                    *length = body.len();
                };
                WebViewResult_Success
            } else {
                WebViewResult_NotFound
            }   
        }
        None => WebViewResult_NotFound
    }
}