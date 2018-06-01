extern crate cc;
extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {

    let bindings = bindgen::Builder::default()
        .header("webview.hpp")
        .whitelist_type("ContentType")
        .whitelist_function("webview_new")
        .whitelist_function("webview_run")
        .whitelist_function("webview_free")
        .generate()
        .expect("Unable to generate bindings.");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    
    bindings.write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings to output file.");

    let mut build = cc::Build::new();
    build.define("WIN32", None);
    build.define("_WINDOWS", None);
    build.define("_UNICODE", None);
    build.define("UNICODE", None);
    build.file("webview.cpp");
    build.file("window.cpp");
    build.flag_if_supported("/std:c++17");
    build.flag_if_supported("/EHsc");

    for &lib in &[
        "user32",
        "gdi32",
        "winspool",
        "comdlg32",
        "advapi32",
        "ole32",
        "oleaut32",
        "uuid",
        "odbc32",
        "odbccp32",
        "windowsapp"
    ] {
        println!("cargo:rustc-link-lib={}", lib);
    }

    build.compile("modern_webview_sys");
}