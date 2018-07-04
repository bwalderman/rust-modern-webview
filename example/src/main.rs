extern crate modern_webview;

use modern_webview::*;

fn main() {

    webview("Sample WebView Project", Content::Html("<h1>Hello World!</h1>"), (1280, 800), true, |webview, event| {
        match event {
            Event::DOMContentLoaded() => {
                webview.eval_script("document.body.innerHTML += 'Host called eval_script.'; window.external.notify('WebView responded via notify.')");
            },
            Event::ScriptNotify(response) => {
                println!("{}", response);
            }
        }
    });
}
