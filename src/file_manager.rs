use dirs_next;
use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use gtk::prelude::*;
use gtk::{
    gio, glib, ListBox, SelectionMode, Entry, Button, Paned, Orientation,
};
use libadwaita::{self as adw, prelude::*};
use crate::ui;


pub struct FileManager {
    current_path: Rc<RefCell<PathBuf>>,
    list_box: ListBox,
    window: adw::ApplicationWindow,
    row_paths: Rc<RefCell<Vec<PathBuf>>>,
    sidebar_list: ListBox,
    path_entry: Entry,
}

impl FileManager {
    pub fn new(app: &adw::Application) -> Self {
        // Create window
        let window = adw::ApplicationWindow::builder()
            .application(app)
            .title("Simple File Manager")
            .default_width(800)
            .default_height(600)
            .build();

        // Create path entry for header
        let path_entry = Entry::new();
        
        // Create custom header bar
        let header_box = ui::create_header_bar(&path_entry);
        
        // Connect close button functionality (find it in the header_box)
        if let Some(close_button) = Self::find_close_button(&header_box) {
            close_button.connect_clicked(glib::clone!(@weak window => move |_| {
                window.close();
            }));
        }

        // Main file list
        let list_box = ListBox::new();
        list_box.set_selection_mode(SelectionMode::Single);
        list_box.set_activate_on_single_click(false);

        // Main content area
        let content_box = ui::create_main_content_area(&header_box, &list_box);

        // Create sidebar
        let (sidebar_scrolled, sidebar_list) = ui::create_sidebar();

        // Main paned layout
        let paned = Paned::new(Orientation::Horizontal);
        paned.set_start_child(Some(&sidebar_scrolled));
        paned.set_end_child(Some(&content_box));
        paned.set_position(200);
        
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

        // Set up event handlers
        file_manager.setup_sidebar_handlers();
        file_manager.setup_file_list_handlers();
        file_manager.setup_navigation_handlers(&header_box);
        file_manager.populate_file_list();
        
        file_manager
    }

    fn find_close_button(header_box: &gtk::Box) -> Option<Button> {
        let mut close_button = None;
        let mut child = header_box.first_child();
        
        while let Some(widget) = child {
            let next_sibling = widget.next_sibling();
            if let Ok(button) = widget.downcast::<Button>() {
                // Check if this looks like a close button (has close icon)
                if button.icon_name().as_deref() == Some("window-close-symbolic") {
                    close_button = Some(button);
                    break;
                }
            }
            child = next_sibling;
        }
        
        close_button
    }

    fn setup_sidebar_handlers(&self) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        
        self.sidebar_list.connect_row_activated(move |_, row| {
            let index = row.index();
            
            if let Some(path) = ui::get_sidebar_path(index) {
                *current_path_clone.borrow_mut() = path.clone();
                Self::populate_files(&list_box_clone, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            }
        });
    }

    fn setup_file_list_handlers(&self) {
        self.list_box.set_activate_on_single_click(false);
        let current_path_clone = self.current_path.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        
        self.list_box.connect_row_activated(move |list_box, row| {
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
                Self::populate_files(list_box, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            } else {
                // Open file with default application
                let file = gio::File::for_path(&path);
                let _ = gio::AppInfo::launch_default_for_uri(&file.uri(), None::<&gio::AppLaunchContext>);
            }
        });
    }

    fn setup_navigation_handlers(&self, header_box: &gtk::Box) {
        // Find navigation buttons in the header
        let mut buttons = Vec::new();
        let mut child = header_box.first_child();
        
        while let Some(widget) = child {
            let next_sibling = widget.next_sibling();
            if let Ok(button) = widget.downcast::<Button>() {
                buttons.push(button);
            }
            child = next_sibling;
        }

        // Assuming the buttons are in order: back, forward, up, home
        if buttons.len() >= 4 {
            self.setup_back_button(&buttons[0]);
            self.setup_up_button(&buttons[2]);
            self.setup_home_button(&buttons[3]);
        }

        self.setup_path_entry_handler();
    }

    fn setup_back_button(&self, back_button: &Button) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();

        back_button.connect_clicked(move |_| {
            Self::populate_files(&list_box_clone, &current_path_clone.borrow(), &row_paths_clone);
            path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
        });
    }

    fn setup_up_button(&self, up_button: &Button) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();

        up_button.connect_clicked(move |_| {
            let current = current_path_clone.borrow().clone();
            if let Some(parent) = current.parent() {
                *current_path_clone.borrow_mut() = parent.to_path_buf();
                Self::populate_files(&list_box_clone, parent, &row_paths_clone);
                path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
            }
        });
    }

    fn setup_home_button(&self, home_button: &Button) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();

        home_button.connect_clicked(move |_| {
            if let Some(home_dir) = dirs_next::home_dir() {
                *current_path_clone.borrow_mut() = home_dir.clone();
                Self::populate_files(&list_box_clone, &home_dir, &row_paths_clone);
                path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
            }
        });
    }

    fn setup_path_entry_handler(&self) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let row_paths_clone = self.row_paths.clone();
        
        self.path_entry.connect_activate(move |entry| {
            let text = entry.text();
            let path = PathBuf::from(text.as_str());
            
            if path.exists() && path.is_dir() {
                *current_path_clone.borrow_mut() = path.clone();
                Self::populate_files(&list_box_clone, &path, &row_paths_clone);
            } else {
                // Show error or reset to current path
                entry.set_text(&current_path_clone.borrow().to_string_lossy());
            }
        });
    }

    fn update_path_display(&self) {
        let path_str = self.current_path.borrow().to_string_lossy().to_string();
        self.path_entry.set_text(&path_str);
    }
    
    fn populate_file_list(&self) {
        let current_path = self.current_path.borrow();
        Self::populate_files(&self.list_box, &current_path, &self.row_paths);
        self.update_path_display();
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
                let row = ui::create_file_row(&path, &file_name);
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
                let row = ui::create_file_row(&path, &file_name);
                list_box.append(&row);
            }
        }
    }
    
    pub fn show_all(&self) {
        self.window.show();
    }
}
