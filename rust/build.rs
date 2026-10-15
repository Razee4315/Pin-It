use std::path::{Path, PathBuf};

fn main() {
    slint_build::compile("ui/app.slint").expect("Slint UI failed to compile");

    // app.rc carries @PLACEHOLDERS@ for the version, so Cargo.toml stays the
    // only place it is written down. The filled-in copy lives in OUT_DIR,
    // which is why the icon and manifest paths become absolute.
    let dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let rc_path = |p: &Path| p.display().to_string().replace('\\', "\\\\");
    let numeric = ["MAJOR", "MINOR", "PATCH"]
        .map(|part| std::env::var(format!("CARGO_PKG_VERSION_{part}")).unwrap())
        .join(",");
    let rc = std::fs::read_to_string(dir.join("app.rc"))
        .expect("app.rc is missing")
        .replace("@VERSION@", env!("CARGO_PKG_VERSION"))
        .replace("@VERSION_NUMERIC@", &format!("{numeric},0"))
        .replace("@ICON@", &rc_path(&dir.join("../resources/icon.ico")))
        .replace("@MANIFEST@", &rc_path(&dir.join("pinit.manifest")));
    let out = PathBuf::from(std::env::var("OUT_DIR").unwrap()).join("app.rc");
    std::fs::write(&out, rc).unwrap();

    println!("cargo:rerun-if-changed=app.rc");
    println!("cargo:rerun-if-changed=pinit.manifest");
    println!("cargo:rerun-if-changed=../resources/icon.ico");
    embed_resource::compile(&out, embed_resource::NONE).manifest_optional().unwrap();
}
