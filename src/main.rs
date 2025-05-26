mod file_manager;
mod file_types;
mod ui;

use libadwaita as adw;
use adw::prelude::*;
use gtk::glib;
use file_manager::FileManager;

fn main() -> glib::ExitCode {
    // Initialize GTK with Adw
    let application = adw::Application::builder()
        .application_id("com.SquarDE.Casper")
        .build();
    application.connect_activate(|app| {
        // Load custom CSS for the headerbar
        ui::load_css();

        // Create the file manager
        let file_manager = FileManager::new(app);
        file_manager.show_all();
    });
    
    // Run the application
    application.run()
} 
