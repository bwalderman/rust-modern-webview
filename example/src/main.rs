extern crate modern_webview;

#[macro_use]
extern crate include_dir;

use modern_webview::*;
use modern_webview::Event::*;
use modern_webview::Content::*;

use include_dir::Dir;

static HTML_DIR: Dir = include_dir!("./html");
static CSS: &'static str = ".script-message { color: #36ab00; }";

fn main() {
    webview("Rust WebView Demo", Content::Dir(HTML_DIR, "index.html"), (1280, 800), true, |webview, event| {
        match event {
            DOMContentLoaded() => {
                
                webview.inject_css(CSS).unwrap();

                let value = webview.eval_script(
                    "document.body.innerHTML += '<span class=\\\"script-message\\\">Host called eval_script.</span>';
                    'ping'").unwrap();

                println!("Returned from eval_script: {}", value);
                
                webview.eval_script("window.external.notify('pong')").unwrap();
            },
            ScriptNotify(response) => {
                println!("Sent via script notify: {}", response);
            }
        }
    }).unwrap();
}
