extern crate modern_webview;

use modern_webview::*;

fn main() {

    webview("Sample WebView Project", Content::Html("<h1>Hello World!</h1>"), Size(1280, 800), true, |webview, arg| {

    });
}
