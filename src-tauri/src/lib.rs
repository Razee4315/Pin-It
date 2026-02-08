// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

mod always_on_top;
mod commands;

use tauri::{
    menu::{Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    Manager, WindowEvent,
};

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

            // Create tray menu
            let show_item = MenuItem::with_id(app, "show", "Show PinIt", true, None::<&str>)?;
            let quit_item = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
            let menu = Menu::with_items(app, &[&show_item, &quit_item])?;

            // Create system tray
            let _tray = TrayIconBuilder::new()
                .icon(app.default_window_icon().unwrap().clone())
                .menu(&menu)
                .show_menu_on_left_click(false)
                .tooltip("PinIt - Always on Top")
                .on_tray_icon_event(|tray, event| {
                    if let TrayIconEvent::Click {
                        button: MouseButton::Left,
                        button_state: MouseButtonState::Up,
                        ..
                    } = event
                    {
                        let app = tray.app_handle();
                        if let Some(window) = app.get_webview_window("main") {
                            let _ = window.show();
                            let _ = window.set_focus();
                        }
                    }
                })
                .on_menu_event(|app, event| match event.id.as_ref() {
                    "show" => {
                        if let Some(window) = app.get_webview_window("main") {
                            let _ = window.show();
                            let _ = window.set_focus();
                        }
                    }
                    "quit" => {
                        app.exit(0);
                    }
                    _ => {}
                })
                .build(app)?;

            Ok(())
        })
        .on_window_event(|window, event| {
            // Hide window instead of closing
            if let WindowEvent::CloseRequested { api, .. } = event {
                let _ = window.hide();
                api.prevent_close();
            }
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
