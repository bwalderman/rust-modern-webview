extern crate modern_webview;

use modern_webview::{ WebView, Content, Size };

fn main() {

    let mut webview = WebView::new("Sample WebView Project", Content::Url("https://github.com"), Size(1280, 800), true).unwrap();
    webview.run();
}
