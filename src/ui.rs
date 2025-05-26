use gtk::prelude::*;
use gtk::{
    Box, Button, ListBox, ListBoxRow, Orientation, PolicyType, ScrolledWindow,
    Label, SelectionMode, Image, Box as GtkBox, Paned, Separator, Entry,
};
use libadwaita as adw;
use dirs_next;
use std::path::Path;
use crate::file_types;

pub fn load_css() {
    let provider = gtk::CssProvider::new();
    provider.load_from_data("
        .navigation-sidebar {
            background-color: @sidebar_bg_color;
            padding: 6px 0;
        }
        .sidebar-row {
            border-radius: 6px;
            margin: 2px 6px;
            transition: all 100ms ease-out;
        }
        .sidebar-row:selected {
            background-color: alpha(@accent_bg_color, 0.3);
        }
        .sidebar-row:hover {
            background-color: alpha(@accent_bg_color, 0.15);
        }
        .sidebar-icon {
            color: @sidebar_fg_color;
            opacity: 0.8;
        }
        .sidebar-label {
            color: @sidebar_fg_color;
            margin-left: 6px;
            min-width: 0;
        }
        .sidebar-scrolled {
            background-color: @sidebar_bg_color;
        }
        separator {
            background-color: alpha(@borders, 0.5);
            margin: 6px 0;
        }
    ");
    
    // Add provider to default display
    if let Some(display) = gtk::gdk::Display::default() {
        gtk::style_context_add_provider_for_display(
            &display,
            &provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );
    }
}

pub fn create_header_bar(path_entry: &Entry) -> Box {
    let header_box = Box::new(Orientation::Horizontal, 6);
    header_box.add_css_class("custom-headerbar");
    header_box.set_margin_end(5);
    header_box.set_margin_top(5);
    header_box.set_margin_bottom(5);
    
    // Navigation buttons (left side)
    let back_button = Button::from_icon_name("go-previous-symbolic");
    back_button.add_css_class("flat");
    let forward_button = Button::from_icon_name("go-next-symbolic");
    forward_button.add_css_class("flat");
    let up_button = Button::from_icon_name("go-up-symbolic");
    up_button.add_css_class("flat");
    let home_button = Button::from_icon_name("go-home-symbolic");
    home_button.add_css_class("flat");
    
    path_entry.set_hexpand(true);
    
    // Close button (right side)
    let close_button = Button::from_icon_name("window-close-symbolic");
    close_button.add_css_class("circular");
    close_button.set_tooltip_text(Some("Close"));
    
    // Add buttons to header
    header_box.append(&back_button);
    header_box.append(&forward_button);
    header_box.append(&up_button);
    header_box.append(&home_button);
    header_box.append(path_entry);
    header_box.append(&close_button);
    
    header_box
}

pub fn create_sidebar() -> (ScrolledWindow, ListBox) {
    let sidebar_list = ListBox::new();
    sidebar_list.add_css_class("navigation-sidebar");
    sidebar_list.set_selection_mode(SelectionMode::Single);
    
    // Stylish sidebar item creation function
    let add_sidebar_item = |sidebar_list: &ListBox, name: &str, icon: &str| {
        let row = ListBoxRow::new();
        row.add_css_class("sidebar-row");
        
        let hbox = GtkBox::new(Orientation::Horizontal, 12);
        hbox.set_margin_start(12);
        hbox.set_margin_end(12);
        hbox.set_margin_top(6);
        hbox.set_margin_bottom(6);
        
        let icon = Image::from_icon_name(icon);
        icon.set_icon_size(gtk::IconSize::Large);
        icon.add_css_class("sidebar-icon");
        
        let label = Label::new(Some(name));
        label.set_halign(gtk::Align::Start);
        label.set_ellipsize(gtk::pango::EllipsizeMode::End);
        label.add_css_class("sidebar-label");
        
        hbox.append(&icon);
        hbox.append(&label);
        row.set_child(Some(&hbox));
        sidebar_list.append(&row);
        row
    };

    // Add styled sidebar items with separators
    add_sidebar_item(&sidebar_list, "Home", "go-home");
    sidebar_list.append(&Separator::new(Orientation::Horizontal));
    
    add_sidebar_item(&sidebar_list, "Downloads", "folder-download");
    add_sidebar_item(&sidebar_list, "Documents", "folder-documents");
    add_sidebar_item(&sidebar_list, "Pictures", "folder-pictures");
    add_sidebar_item(&sidebar_list, "Music", "folder-music");
    add_sidebar_item(&sidebar_list, "Videos", "folder-videos");
    
    sidebar_list.append(&Separator::new(Orientation::Horizontal));
    add_sidebar_item(&sidebar_list, "Trash", "user-trash");

    // Enhanced sidebar scrolling with proper constraints
    let sidebar_scrolled = ScrolledWindow::new();
    sidebar_scrolled.set_policy(PolicyType::Never, PolicyType::Automatic);
    sidebar_scrolled.set_child(Some(&sidebar_list));
    sidebar_scrolled.set_size_request(180, -1); // Reduced minimum width
    sidebar_scrolled.set_max_content_width(280); // Set maximum width
    sidebar_scrolled.set_propagate_natural_width(false);
    sidebar_scrolled.add_css_class("sidebar-scrolled");
    
    (sidebar_scrolled, sidebar_list)
}

pub fn create_main_content_area(header_box: &Box, list_box: &ListBox) -> Box {
    let scrolled_window = ScrolledWindow::new();
    scrolled_window.set_child(Some(list_box));
    scrolled_window.set_hexpand(true);
    scrolled_window.set_vexpand(true);

    let content_box = Box::new(Orientation::Vertical, 0);
    content_box.append(header_box);
    content_box.append(&scrolled_window);
    
    content_box
}

// Create a row with icon and label for a file or directory
pub fn create_file_row(path: &Path, name: &str) -> ListBoxRow {
    let row = ListBoxRow::new();
    
    // Create horizontal box for the row
    let hbox = GtkBox::new(Orientation::Horizontal, 12);
    hbox.set_margin_start(12);
    hbox.set_margin_end(12);
    hbox.set_margin_top(8);
    hbox.set_margin_bottom(8);
    
    // Get appropriate icon
    let icon_name = file_types::get_icon_for_file(path);
    let icon = Image::from_icon_name(icon_name);
    icon.set_icon_size(gtk::IconSize::Large);
    
    // Create label
    let label = Label::new(Some(name));
    label.set_halign(gtk::Align::Start);
    label.set_hexpand(true);

    hbox.set_hexpand(true);
    // Pack icon and label into the box
    hbox.append(&icon);
    hbox.append(&label);
    
    row.set_child(Some(&hbox));
    row
}

pub fn get_sidebar_path(index: i32) -> Option<std::path::PathBuf> {
    match index {
        0 => dirs_next::home_dir(),
        1 => None, // Separator
        2 => dirs_next::download_dir(),
        3 => dirs_next::document_dir(),
        4 => dirs_next::picture_dir(),
        5 => dirs_next::audio_dir(),
        6 => dirs_next::video_dir(),
        7 => None, // Separator
        8 => dirs_next::home_dir().map(|mut path| {
            path.push(".local/share/Trash/files");
            path
        }),
        _ => None,
    }
}
