extern crate modern_webview;

use modern_webview::*;
use modern_webview::Event::*;

static CSS: &'static str = ".script-message { color: #36ab00; }";

static HTML: &'static str = "<!DOCTYPE html>
    <head>
        <title>Rust WebView Script Demo</title>
    </head>
    <body>
        <h1>Rust WebView Script Demo</h1>
    </body>";

fn main() {
    // Create a WebView hosting a static HTML string.
    webview("Rust WebView Demo", Content::Html(HTML), (1280, 800), true, |webview, event| {
        match event {
            DOMContentLoaded => {
                // Inject a static CSS snippet.
                webview.inject_css(CSS).unwrap();

                // Run a script that return a value and print it to the console.
                let value = webview.eval_script(
                    "document.body.innerHTML += '<span class=\\\"script-message\\\">Host called eval_script.</span>';
                    'ping'").unwrap();

                println!("Returned from eval_script: {}", value);
                
                // Send an async notification using notify.
                webview.eval_script("window.external.notify('pong')").unwrap();
            },
            ScriptNotify(response) => {
                // Receive the script notification and print the data to the console.
                println!("Sent via script notify: {}", response);
            },
            _ => {}
        }
    }).unwrap();
}
