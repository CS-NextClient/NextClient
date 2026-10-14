use std::{env, path::PathBuf};

fn main() {
    println!("cargo:rerun-if-env-changed=NEXTCLIENT_WIN7_THUNKS");
    if let Some(path) = env::var_os("NEXTCLIENT_WIN7_THUNKS") {
        assert_eq!(env::var("TARGET").unwrap(), "i686-pc-windows-msvc");
        let path = PathBuf::from(path)
            .canonicalize()
            .expect("Windows 7 thunks");
        println!("cargo:rerun-if-changed={}", path.display());
        println!("cargo:rustc-link-arg-cdylib={}", path.display());
    }
}
