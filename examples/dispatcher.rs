extern crate modern_webview;

use modern_webview::*;
use modern_webview::Event::*;
use modern_webview::Content::*;

use std::thread;
use std::time::Duration;

fn main() {
    // Create a simple WebView and dispatcher.
    let mut webview = WebView::new("Rust WebView Basic Example", Content::Html("<h1>Hello World!</h1>"), (1280, 800), true).unwrap();
    let mut dispatcher = webview.dispatcher();

    // Start a worker thread that will take ownership of the dispatcher.
    let worker = thread::spawn(move || {
        // Sleep for a bit and then dispatch an eval_script call on the main thread.
        thread::sleep(Duration::from_secs(5));
        dispatcher.dispatch(|webview| {
            webview.eval_script("document.body.style.backgroundColor = '#0f0';").unwrap();
        });
    });

    // Main loop for the WebView control.
    'running: loop {
        while let Some(event) = webview.poll_iter().next() {
            match event {
                Event::Quit => {
                    break 'running;
                },
                _ => {}
            }
        }
    }

    worker.join().unwrap();
}
