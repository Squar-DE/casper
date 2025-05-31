mod file_manager;
mod file_types;
mod ui;

use libadwaita as adw;
use adw::prelude::*;
use gtk::glib;
use file_manager::FileManager;

fn main() -> glib::ExitCode {
    let application = adw::Application::builder()
        .application_id("org.SquarDE.Casper")
        .build();
    application.connect_activate(|app| {
        ui::load_css();
        let file_manager = FileManager::new(app);
        file_manager.show_all();
    });
    application.run()
} 
