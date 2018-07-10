extern crate modern_webview;

#[macro_use]
extern crate include_dir;

use modern_webview::*;
use modern_webview::Event::*;

use include_dir::Dir;

// Use include_dir to compile the html folder directly into our binary.
static HTML_DIR: Dir = include_dir!("examples/html");
static CSS: &'static str = ".script-message { color: #36ab00; }";

fn main() {
    // Create a WebView that will serve the content in the html directory.
    webview("Rust WebView include_dir Demo", Content::Dir(HTML_DIR, "index.html"), (1280, 800), true, |webview, event| {}).unwrap();
}
