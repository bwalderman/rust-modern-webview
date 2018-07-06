extern crate modern_webview;

use modern_webview::*;

fn main() {

    webview("Sample WebView Project", Content::Html("<h1>Hello World!</h1>"), (1280, 800), true, |webview, event| {
        match event {
            Event::DOMContentLoaded() => {
                
                webview.inject_css("body { color: #00f; }").unwrap();

                let value = webview.eval_script("document.body.innerHTML += 'Host called eval_script.'; 'ping'").unwrap();
                println!("Returned from eval_script: {}", value);
                
                webview.eval_script("window.external.notify('pong')").unwrap();
            },
            Event::ScriptNotify(response) => {
                println!("Sent via script notify: {}", response);
            }
        }
    }).unwrap();
}
