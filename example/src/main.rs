extern crate modern_webview;

use modern_webview::{ WebView, Content, Size };

fn main() {

    let mut webview = WebView::new("Sample WebView Project", Content::Html("<h1>Hello World!</h1>"), Size(1280, 800), true).unwrap();
    webview.run();
}
