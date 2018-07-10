extern crate modern_webview;

use modern_webview::*;
use modern_webview::Event::*;
use modern_webview::Content::*;

fn main() {
    // Minimal example using webview() to navigate to GitHub.
    webview("Rust WebView Basic Example", Content::Url("https://github.com"), (1280, 800), true, |_webview, _event| {}).unwrap();
}
