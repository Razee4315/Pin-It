// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

mod always_on_top;
mod commands;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .setup(|app| {
            // Initialize event hooks for window tracking
            if let Err(e) = always_on_top::event_hook::init_event_hooks() {
                log::error!("Failed to initialize event hooks: {}", e);
            }

            // Register global shortcuts
            if let Err(e) = always_on_top::hotkey::register_shortcuts(&app.handle()) {
                log::error!("Failed to register shortcuts: {:?}", e);
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::toggle_pin_foreground,
            commands::pin_window,
            commands::unpin_window,
            commands::get_pinned_windows,
            commands::adjust_opacity,
            commands::set_window_opacity,
            commands::is_window_topmost,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
