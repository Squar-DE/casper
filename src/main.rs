use gtk4::prelude::*;
use gtk4::{
    Application, ApplicationWindow, Box as GtkBox, Image, ListBox,
    ListBoxRow, Orientation, ScrolledWindow, Button, Label, Entry, glib,
    Separator, Paned,
};
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::cell::RefCell;

fn main() -> glib::ExitCode {
    let application = Application::builder()
        .application_id("com.SquarDE.Casper")
        .build();

    application.connect_activate(build_ui);
    application.run()
}

fn build_ui(app: &Application) {
    // Create the main window
    let window = ApplicationWindow::builder()
        .application(app)
        .title("Casper -- File Manager")
        .default_width(900)
        .default_height(600)
        .build();

    // Create a paned container for sidebar and main content
    let paned = Paned::new(Orientation::Horizontal);
    paned.set_position(200); // Initial sidebar width
    
    // Create sidebar
    let sidebar_box = GtkBox::new(Orientation::Vertical, 5);
    sidebar_box.set_margin_start(5);
    sidebar_box.set_margin_end(5);
    sidebar_box.set_margin_top(5);
    sidebar_box.set_margin_bottom(5);
    
    // Create main content area
    let main_box = GtkBox::new(Orientation::Vertical, 5);
    
    // Create navigation controls
    let controls_box = GtkBox::new(Orientation::Horizontal, 5);
    let back_button = Button::with_label("⬆️ Up");
    let address_bar = Entry::new();
    let refresh_button = Button::with_label("🔄 Refresh");
    
    controls_box.append(&back_button);
    controls_box.append(&address_bar);
    controls_box.append(&refresh_button);
    
    // Set control box to not expand
    controls_box.set_margin_start(5);
    controls_box.set_margin_end(5);
    controls_box.set_margin_top(5);
    controls_box.set_margin_bottom(5);
    
    // Create file list with scrolled window
    let scrolled_window = ScrolledWindow::builder()
        .hscrollbar_policy(gtk4::PolicyType::Never)
        .vscrollbar_policy(gtk4::PolicyType::Automatic)
        .vexpand(true)  // This makes the scrolled window expand vertically
        .build();
    
    let list_box = ListBox::new();
    scrolled_window.set_child(Some(&list_box));
    
    // Status bar - prevent it from expanding
    let status_bar = Label::new(None);
    status_bar.set_halign(gtk4::Align::Start);
    status_bar.set_margin_start(5);
    status_bar.set_margin_end(5);
    status_bar.set_margin_top(3);
    status_bar.set_margin_bottom(3);
    
    // Add components to main layout
    main_box.append(&controls_box);
    main_box.append(&scrolled_window);
    main_box.append(&status_bar);
    
    // Create sidebar content
    let sidebar_list = create_sidebar();
    sidebar_box.append(&sidebar_list);
    
    // Add the layouts to the paned container
    paned.set_start_child(Some(&sidebar_box));
    paned.set_end_child(Some(&main_box));
    
    window.set_child(Some(&paned));
    
    // Current path state
    let current_path = Rc::new(RefCell::new(PathBuf::from(glib::home_dir())));
    
    // Update the file list with the contents of the current path
    let update_file_list = {
        let list_box_clone = list_box.clone();
        let current_path_clone = Rc::clone(&current_path);
        let status_bar_clone = status_bar.clone();
        let address_bar_clone = address_bar.clone();
        
        Rc::new(move || {
            // Clear the list box
            while let Some(child) = list_box_clone.first_child() {
                list_box_clone.remove(&child);
            }
            
            let path = current_path_clone.borrow().clone();
            
            // Update address bar
            address_bar_clone.set_text(&path.to_string_lossy());
            
            // Read directory contents
            match std::fs::read_dir(&path) {
                Ok(entries) => {
                    let mut count = 0;
                    let mut entries_vec: Vec<_> = entries.filter_map(Result::ok).collect();
                    
                    // Sort entries (directories first, then files, alphabetically)
                    entries_vec.sort_by(|a, b| {
                        let a_is_dir = a.path().is_dir();
                        let b_is_dir = b.path().is_dir();
                        
                        if a_is_dir && !b_is_dir {
                            std::cmp::Ordering::Less
                        } else if !a_is_dir && b_is_dir {
                            std::cmp::Ordering::Greater
                        } else {
                            a.file_name().cmp(&b.file_name())
                        }
                    });
                    
                    // Add entries to the list box
                    for entry in entries_vec {
                        let row = create_file_row(&entry.path());
                        list_box_clone.append(&row);
                        count += 1;
                    }
                    
                    status_bar_clone.set_text(&format!("{} items", count));
                }
                Err(e) => {
                    status_bar_clone.set_text(&format!("Error reading directory: {}", e));
                }
            }
        })
    };
    
    // Set up sidebar selection handler
    {
        let current_path_clone = Rc::clone(&current_path);
        let update_file_list_clone = Rc::clone(&update_file_list);
        
        sidebar_list.connect_row_selected(move |_, row| {
            if let Some(row) = row {
                if let Some(label) = get_label_from_row(row) {
                    let label_text = label.text().to_string();
                    let new_path = match label_text.as_str() {
                        "Home" => glib::home_dir(),
                        "Documents" => {
                            let mut path = glib::home_dir();
                            path.push("Documents");
                            path
                        },
                        "Music" => {
                            let mut path = glib::home_dir();
                            path.push("Music");
                            path
                        },
                        "Pictures" => {
                            let mut path = glib::home_dir();
                            path.push("Pictures");
                            path
                        },
                        "Videos" => {
                            let mut path = glib::home_dir();
                            path.push("Videos");
                            path
                        },
                        "Downloads" => {
                            let mut path = glib::home_dir();
                            path.push("Downloads");
                            path
                        },
                        "Trash" => {
                            // On most Unix systems, trash is at ~/.local/share/Trash
                            let mut path = glib::home_dir();
                            path.push(".local");
                            path.push("share");
                            path.push("Trash");
                            path
                        },
                        _ => return,
                    };
                    
                    if new_path.exists() && new_path.is_dir() {
                        *current_path_clone.borrow_mut() = new_path;
                        update_file_list_clone();
                    }
                }
            }
        });
    }
    
    // Initial load
    update_file_list();
    
    // Handle double-click on a file
    {
        let current_path_clone = Rc::clone(&current_path);
        let update_file_list_clone = Rc::clone(&update_file_list);
        
        list_box.connect_row_activated(move |_, row| {
            if let Some(box_layout) = row.child().and_then(|child| child.downcast::<GtkBox>().ok()) {
                if let Some(label) = box_layout.last_child().and_then(|l| l.downcast::<Label>().ok()) {
                    let file_name = label.text().to_string();
                    let mut new_path = current_path_clone.borrow().clone();
                    new_path.push(&file_name);
                    
                    if new_path.is_dir() {
                        *current_path_clone.borrow_mut() = new_path;
                        update_file_list_clone();
                    }
                }
            }
        });
    }
    
    // Handle back button
    {
        let current_path_clone = Rc::clone(&current_path);
        let update_file_list_clone = Rc::clone(&update_file_list);
        
        back_button.connect_clicked(move |_| {
            let mut path = current_path_clone.borrow().clone();
            if path.pop() {
                *current_path_clone.borrow_mut() = path;
                update_file_list_clone();
            }
        });
    }
    
    // Handle refresh button
    {
        let update_file_list_clone = Rc::clone(&update_file_list);
        
        refresh_button.connect_clicked(move |_| {
            update_file_list_clone();
        });
    }
    
    // Handle address bar
    {
        let current_path_clone = Rc::clone(&current_path);
        let update_file_list_clone = Rc::clone(&update_file_list);
        
        address_bar.connect_activate(move |entry| {
            let text = entry.text().to_string();
            let path = PathBuf::from(&text);
            
            if path.exists() && path.is_dir() {
                *current_path_clone.borrow_mut() = path;
                update_file_list_clone();
            }
        });
    }
    
    window.present();
}

fn create_sidebar() -> ListBox {
    let sidebar_list = ListBox::new();
    sidebar_list.set_selection_mode(gtk4::SelectionMode::Single);
    
    // Home
    let home_row = create_sidebar_row("Home", "user-home");
    sidebar_list.append(&home_row);
    
    // First separator
    let separator1 = create_separator_row();
    sidebar_list.append(&separator1);
    
    // User directories
    let documents_row = create_sidebar_row("Documents", "folder-documents");
    let music_row = create_sidebar_row("Music", "folder-music");
    let pictures_row = create_sidebar_row("Pictures", "folder-pictures");
    let videos_row = create_sidebar_row("Videos", "folder-videos");
    let downloads_row = create_sidebar_row("Downloads", "folder-download");
    
    sidebar_list.append(&documents_row);
    sidebar_list.append(&music_row);
    sidebar_list.append(&pictures_row);
    sidebar_list.append(&videos_row);
    sidebar_list.append(&downloads_row);
    
    // Second separator
    let separator2 = create_separator_row();
    sidebar_list.append(&separator2);
    
    // Trash
    let trash_row = create_sidebar_row("Trash", "user-trash");
    sidebar_list.append(&trash_row);
    
    sidebar_list
}

fn create_sidebar_row(label_text: &str, icon_name: &str) -> ListBoxRow {
    let row = ListBoxRow::new();
    let box_layout = GtkBox::new(Orientation::Horizontal, 10);
    box_layout.set_margin_start(5);
    box_layout.set_margin_end(5);
    box_layout.set_margin_top(5);
    box_layout.set_margin_bottom(5);
    
    let image = Image::from_icon_name(icon_name);
    let label = Label::new(Some(label_text));
    label.set_halign(gtk4::Align::Start);
    label.set_hexpand(true);
    
    box_layout.append(&image);
    box_layout.append(&label);
    
    row.set_child(Some(&box_layout));
    row
}

fn create_separator_row() -> ListBoxRow {
    let row = ListBoxRow::new();
    row.set_selectable(false);
    
    let separator = Separator::new(Orientation::Horizontal);
    separator.set_margin_top(5);
    separator.set_margin_bottom(5);
    
    row.set_child(Some(&separator));
    row
}

fn get_label_from_row(row: &ListBoxRow) -> Option<Label> {
    row.child()
        .and_then(|child| child.downcast::<GtkBox>().ok())
        .and_then(|box_layout| box_layout.last_child())
        .and_then(|widget| widget.downcast::<Label>().ok())
}

fn create_file_row(path: &Path) -> ListBoxRow {
    let row = ListBoxRow::new();
    let box_layout = GtkBox::new(Orientation::Horizontal, 10);
    box_layout.set_margin_start(5);
    box_layout.set_margin_end(5);
    box_layout.set_margin_top(3);
    box_layout.set_margin_bottom(3);
    
    // Choose icon based on file type
    let icon_name = if path.is_dir() {
        "folder"
    } else {
        match path.extension().and_then(|ext| ext.to_str()) {
            Some("rs") => "text-x-rust",
            Some("txt") | Some("md") => "text-x-generic",
            Some("png") | Some("jpg") | Some("jpeg") => "image-x-generic",
            Some("mp3") | Some("ogg") | Some("wav") => "audio-x-generic",
            Some("mp4") | Some("mkv") | Some("avi") => "video-x-generic",
            Some("pdf") => "application-pdf",
            _ => "text-x-generic",
        }
    };
    
    let image = Image::from_icon_name(icon_name);
    let filename = path.file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string();
    
    let label = Label::new(Some(&filename));
    label.set_halign(gtk4::Align::Start);
    label.set_hexpand(true);  // Make label expand horizontally
    
    box_layout.append(&image);
    box_layout.append(&label);
    
    row.set_child(Some(&box_layout));
    row
}
