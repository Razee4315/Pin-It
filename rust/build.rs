fn main() {
    slint_build::compile("ui/app.slint").expect("Slint UI failed to compile");
    embed_resource::compile("app.rc", embed_resource::NONE)
        .manifest_optional()
        .unwrap();
}
