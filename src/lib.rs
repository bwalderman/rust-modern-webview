extern crate modern_webview_sys as ffi;
extern crate include_dir;

use std::ffi::{ CString, CStr };
use std::marker::PhantomData;
use std::path::Path;
use std::os::raw::*;
use std::ptr;
use std::fmt;
use ffi::*;

use include_dir::Dir;

/// Types of content a new webview can navigate to.
pub enum Content<'a, S: Into<String>> {
    /// Navigate to an HTML source string.
    Html(S), 
    /// Navigate to an http or https URL such as a local or internet server.
    Url(S), 
    /// Navigate to local content embedded within the binary.
    Dir(Dir<'a>, S)
}

/// Async notifications from the WinRT WebViewControl.
pub enum Event {
    /// The WebView window was closed.
    Quit,
    /// The DOMContentLoaded event was fired on the page.
    DOMContentLoaded,
    /// Script on the page called window.external.notify.
    ScriptNotify(String)
}

/// Errors reported the library.
#[derive(Debug)]
pub enum Error {
    Null(std::ffi::NulError),
    Runtime(i32, String)
}

impl From<std::ffi::NulError> for Error {
    fn from (err: std::ffi::NulError) -> Error {
        Error::Null(err)
    }
}

impl std::error::Error for Error {
    fn description(&self) -> &str {
        match *self {
            Error::Null(ref err) => err.description(),
            Error::Runtime(_, ref message) => message.as_str()
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Error::Null(ref err) => write!(f, "Null error: {}", err),
            Error::Runtime(code, ref message) => write!(f, "Windows Runtime error 0x{:08x}: \"{}\"", code, message)
        }
    }
}

pub type Result<T> = std::result::Result<T, Error>;

/// This struct represents the web view control and includes methods and properties for manipulating and inspecting the HTML content.
pub struct WebView<'a> {
    window: *mut c_void,
    internal: Box<InternalData<'a>>
}

struct InternalData<'a> {
    dir: Option<include_dir::Dir<'a>>
}

/// A thread-safe struct that can be used to dispatch calls on the WebView's UI thread.
pub struct Dispatcher<'a> {
    phantom: PhantomData<&'a WebView<'a>>,
    window: *mut c_void,
    webview: *mut c_void
}

struct CallbackInfo<'a> {
    callback: Box<FnMut(&'a mut WebView<'a>) + 'a>
}

/// Iterator that provides a sequence of WebView events.
pub struct EventIterator<'a> {
    webview: &'a WebView<'a>,
    blocking: bool
}

impl<'a> WebView<'a> {
    pub fn new<S: Into<String>>(title: &str, content: Content<'a, S>, size: (i32, i32), resizable: bool) -> Result<WebView<'a>> {

        let title = CString::new(title)?;
        
        let window = ffi_result(unsafe {
            let mut window: *mut c_void = ptr::null_mut();
            let result = webview_new(title.as_ptr(), size.0, size.1, resizable, &mut window);
            (window, result)
        })?;

        let mut webview = WebView { window, internal: Box::new(InternalData { dir: None }) };
        let internal = webview.internal.as_mut() as *mut InternalData as *mut c_void; 

        ffi_result(unsafe {
            match content {
                Content::Url(url) => {
                    let url = CString::new(url.into())?;
                    let result = webview_navigate(window, internal, url.as_ptr(), ContentType_Url);
                    ((), result)
                }
                Content::Html(html) => {
                    let html = CString::new(html.into())?;
                    let result = webview_navigate(window, internal, html.as_ptr(), ContentType_Html);
                    ((), result)                
                }
                Content::Dir(dir, source) => {
                    webview.internal.dir = Some(dir);
                    let source = CString::new(source.into())?;
                    let result = webview_navigate_with_streamresolver(window, internal, source.as_ptr());
                    ((), result)
                }
            }
        })?;

        Ok(webview)
    }

    /// Obtain a dispatcher which can be used to post callbacks to be executed on the WebView's UI thread.
    pub fn dispatcher(&mut self) -> Dispatcher<'a> {
        Dispatcher { phantom: PhantomData, window: self.window, webview: self as *mut WebView as *mut c_void }
    }

    /// Returns a non-blocking event loop iterator. The iterator will return an event if available, or return
    /// None once no more events are available.
    pub fn poll_iter(&self) -> EventIterator {
        EventIterator { webview: self, blocking: false }
    }

    /// Returns a blocking event loop iterator. The iterator will return an event if available, or block
    /// until one becomes available.
    pub fn wait_iter(&self) -> EventIterator {
        EventIterator { webview: self, blocking: true }
    }

    /// Execute JavaScript on the top-level page. This function blocks until script execution is complete.
    /// The JavaScript can return a string value to Rust.
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

    /// Apply CSS styles to the top-level page.
    pub fn inject_css(&mut self, css: &str) -> Result<()> {

        let css = CString::new(css)?;
        ffi_result(unsafe {
            let result = webview_inject_css(self.window, css.as_ptr());
            ((), result)
        })
    }
}

impl<'a> Drop for WebView<'a> {
    fn drop(&mut self) {
        unsafe { webview_free(self.window) };
    }
}

fn ffi_result<T>(result: (T, i32)) -> Result<T> {
    match result {
        (value, 0) => Ok(value),
        (_, code) => {
            let mut msg: *mut c_char = ptr::null_mut();
            unsafe { webview_get_error_message(&mut msg); };

            let message = unsafe { CStr::from_ptr(msg).to_string_lossy().into_owned() };
            unsafe { webview_string_free(msg) };

            Err(Error::Runtime(code, message))
        }
    }
}

unsafe impl<'a> Send for Dispatcher<'a> {}
unsafe impl<'a> Sync for Dispatcher<'a> {}

impl<'a> Dispatcher<'a> {
    pub fn dispatch<F>(&mut self, callback: F) -> Result<()> where F: FnMut(&mut WebView) + 'a {
        ffi_result(unsafe {
            let info_ptr = Box::into_raw(Box::new(CallbackInfo { callback: Box::new(callback) }));
            let result = webview_dispatch(self.window, self.webview, info_ptr as *mut c_void);
            ((), result)
        })
    }
}

impl<'a> Clone for Dispatcher<'a> {
    fn clone(&self) -> Dispatcher<'a> {
        Dispatcher { phantom: self.phantom, window: self.window, webview: self.webview }
    }
}

impl<'a> Iterator for EventIterator<'a> {
    type Item = Event;

    fn next(&mut self) -> Option<Event> {
        
        let mut event: u32 = EventType_None;
        let mut data: *mut c_char = ptr::null_mut();

        unsafe {
            webview_loop(self.webview.window, self.blocking, &mut event, &mut data)
        };

        match event {
            EventType_Quit => Some(Event::Quit),
            EventType_DOMContentLoaded => Some(Event::DOMContentLoaded),
            EventType_ScriptNotify => {
                let response = unsafe { CStr::from_ptr(data).to_string_lossy().to_string() };
                unsafe { webview_string_free(data) };
                Some(Event::ScriptNotify(response))
            },
            _ => None
        }
    }
}

/// Create a new Window with a WebViewControl. The title and size can be customized. The content parameter indicates the
/// initial content to navigate to. This can be a simple HTML string, a URL, or a directory of files embedded in the binary
/// using include_dir. The provided callback will be called when events such as DOMContentLoaded, or a script notification
/// occur. See the Event enum for a full list of available events. The webview functions blocks until the WebViewControl
/// window is closed.
pub fn webview<'a, S: Into<String>, F>(
    title: &str, content: Content<S>, size: (i32, i32), resizable: bool, mut callback: F) -> Result<()> where F: FnMut(&mut WebView, Event) + 'a {

    let mut webview = WebView::new(title, content, size, resizable)?;
    
    'running: loop {
        while let Some(event) = webview.wait_iter().next() {
            match event {
                Event::Quit => {
                    break 'running;
                },
                event => {
                    callback(&mut webview, event);
                }
            }
        }
    }

    Ok(())
}

#[no_mangle]
pub extern "C" fn webview_get_content(webview_ptr: *mut c_void, source: *const c_char, content: *mut *const u8, length: *mut usize) -> bool {
    let internal = unsafe {
        (webview_ptr as *mut InternalData).as_mut().unwrap()
    };
    
    unsafe {
        *content = ptr::null();
        *length = 0;
    };

    if let Some(ref dir) = internal.dir {
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
            
            return true
        }
    }

    false
}

#[no_mangle]
pub extern "C" fn webview_dispatch_callback(webview_ptr: *mut c_void, info_ptr: *mut c_void) {
    let mut webview = unsafe { (webview_ptr as *mut WebView).as_mut().unwrap() };
    let mut info = unsafe { Box::from_raw(info_ptr as *mut CallbackInfo) };
    (info.callback)(&mut webview);
}