// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

mod always_on_top;
mod autostart;
mod commands;
mod persistence;

use tauri::{
    menu::{Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    Manager, RunEvent, WindowEvent,
};

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    // Initialize logging so log::info!, log::error! etc. actually print
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .format_timestamp_millis()
        .init();

    log::info!("PinIt starting up");

    let app = tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .setup(|app| {
            // Store app handle for event emission from hooks
            always_on_top::event_hook::set_app_handle(app.handle().clone());

            // Initialize event hooks for window tracking
            if let Err(e) = always_on_top::event_hook::init_event_hooks() {
                log::error!("Failed to initialize event hooks: {}", e);
            }

            // Register global shortcuts
            if let Err(e) = always_on_top::hotkey::register_shortcuts(&app.handle()) {
                log::error!("Failed to register shortcuts: {:?}", e);
            }

            // Restore previously pinned windows
            persistence::restore();

            // Create tray menu
            let show_item = MenuItem::with_id(app, "show", "Show PinIt", true, None::<&str>)?;
            let quit_item = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
            let menu = Menu::with_items(app, &[&show_item, &quit_item])?;

            // Create system tray
            let _tray = TrayIconBuilder::with_id("main-tray")
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
            commands::focus_window,
            commands::get_pinned_count,
            commands::get_auto_start,
            commands::set_auto_start,
            commands::get_sound_enabled,
            commands::set_sound_enabled,
            commands::get_has_seen_tray_notice,
            commands::set_has_seen_tray_notice,
        ])
        .build(tauri::generate_context!())
        .expect("error while building tauri application");

    app.run(|_app_handle, event| {
        if let RunEvent::Exit = event {
            log::info!("PinIt shutting down, saving state...");
            persistence::save_current();
            always_on_top::event_hook::cleanup_event_hooks();
        }
    });
}
