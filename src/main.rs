use dirs_next;
use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use gtk::prelude::*;
use gtk::{
    gio, glib, Box, Button, ListBox, ListBoxRow, Orientation, PolicyType, ScrolledWindow, 
    Label, SelectionMode, Image, Box as GtkBox, Paned, Separator, Entry,
};
use libadwaita as adw;
use adw::prelude::*;

// File type icon mappings
struct FileTypeIcon {
    extension: &'static str,
    icon_name: &'static str,
}

// List of file type icons
const FILE_TYPE_ICONS: &[FileTypeIcon] = &[
    // Documents
    FileTypeIcon { extension: "txt", icon_name: "text-x-generic" },
    FileTypeIcon { extension: "pdf", icon_name: "application-pdf" },
    FileTypeIcon { extension: "doc", icon_name: "x-office-document" },
    FileTypeIcon { extension: "docx", icon_name: "x-office-document" },
    FileTypeIcon { extension: "odt", icon_name: "x-office-document" },
    
    // Spreadsheets
    FileTypeIcon { extension: "xls", icon_name: "x-office-spreadsheet" },
    FileTypeIcon { extension: "xlsx", icon_name: "x-office-spreadsheet" },
    FileTypeIcon { extension: "ods", icon_name: "x-office-spreadsheet" },
    
    // Presentations
    FileTypeIcon { extension: "ppt", icon_name: "x-office-presentation" },
    FileTypeIcon { extension: "pptx", icon_name: "x-office-presentation" },
    FileTypeIcon { extension: "odp", icon_name: "x-office-presentation" },
    
    // Images
    FileTypeIcon { extension: "jpg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "jpeg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "png", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "gif", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "svg", icon_name: "image-x-generic" },
    FileTypeIcon { extension: "webp", icon_name: "image-x-generic" },
    
    // Audio
    FileTypeIcon { extension: "mp3", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "ogg", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "wav", icon_name: "audio-x-generic" },
    FileTypeIcon { extension: "flac", icon_name: "audio-x-generic" },
    
    // Video
    FileTypeIcon { extension: "mp4", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "webm", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "mkv", icon_name: "video-x-generic" },
    FileTypeIcon { extension: "avi", icon_name: "video-x-generic" },
    
    // Archives
    FileTypeIcon { extension: "zip", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "tar", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "rar", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "gz", icon_name: "package-x-generic" },
    FileTypeIcon { extension: "7z", icon_name: "package-x-generic" },
    
    // Code
    FileTypeIcon { extension: "c", icon_name: "text-x-script" },
    FileTypeIcon { extension: "cpp", icon_name: "text-x-script" },
    FileTypeIcon { extension: "h", icon_name: "text-x-script" },
    FileTypeIcon { extension: "rs", icon_name: "text-x-script" },
    FileTypeIcon { extension: "py", icon_name: "text-x-script" },
    FileTypeIcon { extension: "js", icon_name: "text-x-script" },
    FileTypeIcon { extension: "html", icon_name: "text-html" },
    FileTypeIcon { extension: "css", icon_name: "text-x-script" },
    FileTypeIcon { extension: "json", icon_name: "text-x-script" },
    FileTypeIcon { extension: "xml", icon_name: "text-xml" },
    
    // Executables
    FileTypeIcon { extension: "exe", icon_name: "application-x-executable" },
    FileTypeIcon { extension: "sh", icon_name: "application-x-executable" },
    FileTypeIcon { extension: "appimage", icon_name: "application-x-executable" },
];

struct FileManager {
    current_path: Rc<RefCell<PathBuf>>,
    list_box: ListBox,
    window: adw::ApplicationWindow,
    row_paths: Rc<RefCell<Vec<PathBuf>>>,
    sidebar_list: ListBox,
    path_entry: Entry, // Add this for the path bar
}


impl FileManager {
    fn new(app: &adw::Application) -> Self {
        // Create window
        let window = adw::ApplicationWindow::builder()
            .application(app)
            .title("Simple File Manager")
            .default_width(800)
            .default_height(600)
            .build();

        // Create custom header bar
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
        
        // Path entry (center)
        let path_entry = Entry::new();
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
        header_box.append(&path_entry);
        header_box.append(&close_button); // Add close button last to right-align it

        // Connect close button to window close action
        close_button.connect_clicked(glib::clone!(@weak window => move |_| {
            window.close();
        }));        // Main file list
        let list_box = ListBox::new();
        list_box.set_selection_mode(SelectionMode::Single);
        list_box.set_activate_on_single_click(false); 
        let scrolled_window = ScrolledWindow::new();
        scrolled_window.set_child(Some(&list_box));
        scrolled_window.set_hexpand(true);
        scrolled_window.set_vexpand(true);

        // Main content box
        let content_box = Box::new(Orientation::Vertical, 0);
        content_box.append(&header_box);
        content_box.append(&scrolled_window);

        // Create sidebar with enhanced styling
        let sidebar_list = ListBox::new();
        sidebar_list.add_css_class("navigation-sidebar");
        sidebar_list.set_selection_mode(SelectionMode::Single);
        // Stylish sidebar item creation function
        let add_sidebar_item = |name: &str, icon: &str| {
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
            label.add_css_class("sidebar-label");
            
            hbox.append(&icon);
            hbox.append(&label);
            row.set_child(Some(&hbox));
            sidebar_list.append(&row);
            row
        };

        // Add styled sidebar items with separators
        add_sidebar_item("Home", "go-home");
        sidebar_list.append(&Separator::new(Orientation::Horizontal));
        
        add_sidebar_item("Downloads", "folder-download");
        add_sidebar_item("Documents", "folder-documents");
        add_sidebar_item("Pictures", "folder-pictures");
        add_sidebar_item("Music", "folder-music");
        add_sidebar_item("Videos", "folder-videos");
        
        sidebar_list.append(&Separator::new(Orientation::Horizontal));
        add_sidebar_item("Trash", "user-trash");

        // Enhanced sidebar scrolling
        let sidebar_scrolled = ScrolledWindow::new();
        sidebar_scrolled.set_policy(PolicyType::Never, PolicyType::Automatic);
        sidebar_scrolled.set_child(Some(&sidebar_list));
        sidebar_scrolled.set_size_request(220, -1); // Slightly wider for better spacing
        sidebar_scrolled.add_css_class("sidebar-scrolled");
        // Main paned layout
        let paned = Paned::new(Orientation::Horizontal);
        paned.set_start_child(Some(&sidebar_scrolled));
        paned.set_end_child(Some(&content_box));
        paned.set_position(200); // Set initial sidebar width
        
        // Set window content
        window.set_content(Some(&paned));

        // Initialize path tracking
        let home_dir = dirs_next::home_dir().unwrap_or_else(|| PathBuf::from("/"));
        let current_path = Rc::new(RefCell::new(home_dir));
        let row_paths = Rc::new(RefCell::new(Vec::new()));
        
        // Create instance
        let file_manager = FileManager {
            current_path: current_path.clone(),
            list_box: list_box.clone(),
            window,
            row_paths: row_paths.clone(),
            sidebar_list: sidebar_list.clone(),
            path_entry: path_entry.clone(),
        };

        // Proper sidebar navigation handler
        let current_path_clone = current_path.clone();
        let list_box_clone = list_box.clone();
        let path_entry_clone = path_entry.clone();
        let row_paths_clone = row_paths.clone();
        
        sidebar_list.connect_row_activated(move |_, row| {
            let index = row.index();
            let path = match index {
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
            };

            if let Some(path) = path {
                *current_path_clone.borrow_mut() = path.clone();
                FileManager::populate_files(&list_box_clone, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            }
        });

        // File list activation handler (requires double-click)
        list_box.set_activate_on_single_click(false);
        let current_path_clone = current_path.clone();
        let path_entry_clone = path_entry.clone();
        let row_paths_clone = row_paths.clone();
        
        list_box.connect_row_activated(move |list_box, row| {
            let index = row.index();
            let path = {
                let paths = row_paths_clone.borrow();
                if index as usize >= paths.len() {
                    return;
                }
                paths[index as usize].clone()
            };

            if path.is_dir() {
                *current_path_clone.borrow_mut() = path.clone();
                FileManager::populate_files(list_box, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            } else {
                // Open file with default application - simplified file handling
                let file = gio::File::for_path(&path);
                let _ = gio::AppInfo::launch_default_for_uri(&file.uri(), None::<&gio::AppLaunchContext>);
            }
        });

        // Set up handlers and populate initial view
        file_manager.setup_handlers(&back_button, &up_button, &home_button);
        file_manager.populate_file_list();
        
        file_manager
    }

   
    fn update_path_display(&self) {
        let path_str = self.current_path.borrow().to_string_lossy().to_string();
        self.path_entry.set_text(&path_str);
    }
    
fn setup_handlers(&self, back_button: &Button, up_button: &Button, home_button: &Button) {
    let current_path_clone = self.current_path.clone();
    let list_box_clone = self.list_box.clone();
    let path_entry_clone = self.path_entry.clone();
    let row_paths_clone = self.row_paths.clone();

    // Back button handler
    back_button.connect_clicked(move |_| {
        FileManager::populate_files(&list_box_clone, &current_path_clone.borrow(), &row_paths_clone);
        path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
    });

    let current_path_clone = self.current_path.clone();
    let list_box_clone = self.list_box.clone();
    let path_entry_clone = self.path_entry.clone();
    let row_paths_clone = self.row_paths.clone();

    // Up button handler
    up_button.connect_clicked(move |_| {
        let current = current_path_clone.borrow().clone();
        if let Some(parent) = current.parent() {
            *current_path_clone.borrow_mut() = parent.to_path_buf();
            FileManager::populate_files(&list_box_clone, parent, &row_paths_clone);
            path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
        }
    });

    let current_path_clone = self.current_path.clone();
    let list_box_clone = self.list_box.clone();
    let path_entry_clone = self.path_entry.clone();
    let row_paths_clone = self.row_paths.clone();

    // Home button handler
    home_button.connect_clicked(move |_| {
        if let Some(home_dir) = dirs_next::home_dir() {
            *current_path_clone.borrow_mut() = home_dir.clone();
            FileManager::populate_files(&list_box_clone, &home_dir, &row_paths_clone);
            path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
        }
    });

    // Path entry handler
    let current_path_clone = self.current_path.clone();
    let list_box_clone = self.list_box.clone();
    let row_paths_clone = self.row_paths.clone();
    self.path_entry.connect_activate(move |entry| {
        let text = entry.text();
        let path = PathBuf::from(text.as_str());
        
        if path.exists() && path.is_dir() {
            *current_path_clone.borrow_mut() = path.clone();
            FileManager::populate_files(&list_box_clone, &path, &row_paths_clone);
        } else {
            // Show error or reset to current path
            entry.set_text(&current_path_clone.borrow().to_string_lossy());
        }
    });
    
}
    
    
    fn populate_file_list(&self) {
        let current_path = self.current_path.borrow();
        FileManager::populate_files(&self.list_box, &current_path, &self.row_paths);
    }
    
    // Get icon name for file based on extension
    fn get_icon_for_file(file_path: &Path) -> &'static str {
        if file_path.is_dir() {
            return "folder";
        }
        
        if let Some(extension) = file_path.extension() {
            if let Some(ext_str) = extension.to_str() {
                for file_type in FILE_TYPE_ICONS {
                    if ext_str.eq_ignore_ascii_case(file_type.extension) {
                        return file_type.icon_name;
                    }
                }
            }
        }
        
        // Default icon for unknown file types
        "text-x-generic"
    }
    
    fn populate_files(list_box: &ListBox, path: &Path, row_paths: &Rc<RefCell<Vec<PathBuf>>>) {
        // Clear existing items
        while let Some(child) = list_box.first_child() {
            list_box.remove(&child);
        }
    
        // Clear existing paths
        row_paths.borrow_mut().clear();
    
        // Read directory contents
        if let Ok(entries) = std::fs::read_dir(path) {
            // Sort entries: directories first, then files, both alphabetically
            let mut directories = Vec::new();
            let mut files = Vec::new();
        
            for entry in entries.filter_map(Result::ok) {
                let path = entry.path();
                if path.is_dir() {
                    directories.push(entry);
                } else {
                    files.push(entry);
                }
            }
        
            directories.sort_by(|a, b| a.file_name().cmp(&b.file_name()));
            files.sort_by(|a, b| a.file_name().cmp(&b.file_name()));
        
            // Add directories to list box
            for entry in directories {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                let path = entry.path();
            
                // Store this path in our vector
                row_paths.borrow_mut().push(path.clone());
            
                // Create row with icon and label
                let row = FileManager::create_file_row(&path, &file_name);
                list_box.append(&row);
            }
        
            // Add files to list box
            for entry in files {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                let path = entry.path();
            
                // Store this path in our vector
                row_paths.borrow_mut().push(path.clone());
            
                // Create row with icon and label
                let row = FileManager::create_file_row(&path, &file_name);
                list_box.append(&row);
            }
        }
    }
    
    // Create a row with icon and label for a file or directory
    fn create_file_row(path: &Path, name: &str) -> ListBoxRow {
        let row = ListBoxRow::new();
        
        // Create horizontal box for the row
        let hbox = GtkBox::new(Orientation::Horizontal, 12);
        hbox.set_margin_start(12);
        hbox.set_margin_end(12);
        hbox.set_margin_top(8);
        hbox.set_margin_bottom(8);
        // Get appropriate icon
        let icon_name = FileManager::get_icon_for_file(path);
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
    
    fn show_all(&self) {
        self.window.show();
    }
}


fn main() -> glib::ExitCode {
    // Initialize GTK with Adw
    let application = adw::Application::builder()
        .application_id("com.SquarDE.Casper")
        .build();

    application.connect_activate(|app| {
        // Load custom CSS for the headerbar
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

        // Create the file manager
        let file_manager = FileManager::new(app);
        file_manager.show_all();
    });
    
    // Run the application
    application.run()
} 
