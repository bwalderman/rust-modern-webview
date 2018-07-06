extern crate modern_webview;

use modern_webview::*;
use modern_webview::Event::*;
use modern_webview::Content::*;

fn main() {

    webview("Sample WebView Project", Html("<h1>Hello World!</h1>"), (1280, 800), true, |webview, event| {
        match event {
            DOMContentLoaded() => {
                
                webview.inject_css("body { color: #00f; }").unwrap();

                let value = webview.eval_script("document.body.innerHTML += 'Host called eval_script.'; 'ping'").unwrap();
                println!("Returned from eval_script: {}", value);
                
                webview.eval_script("window.external.notify('pong')").unwrap();
            },
            ScriptNotify(response) => {
                println!("Sent via script notify: {}", response);
            }
        }
    }).unwrap();
}
