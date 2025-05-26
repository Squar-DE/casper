use dirs_next;
use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use gtk::prelude::*;
use gtk::{
    gio, glib, ListBox, SelectionMode, Entry, Button, Paned, Orientation,
    GridView, SignalListItemFactory, SingleSelection
};
use libadwaita::{self as adw, prelude::*};
use crate::ui;
use crate::file_types::FileItem;

pub struct FileManager {
    current_path: Rc<RefCell<PathBuf>>,
    list_box: ListBox,
    grid_view: GridView,
    window: adw::ApplicationWindow,
    row_paths: Rc<RefCell<Vec<PathBuf>>>,
    sidebar_list: ListBox,
    path_entry: Entry,
    view_mode: Rc<RefCell<ViewMode>>,
}

#[derive(Clone, Copy, PartialEq)]
enum ViewMode {
    List,
    Grid,
}

impl Clone for FileManager {
    fn clone(&self) -> Self {
        Self {
            current_path: self.current_path.clone(),
            list_box: self.list_box.clone(),
            grid_view: self.grid_view.clone(),
            window: self.window.clone(),
            row_paths: self.row_paths.clone(),
            sidebar_list: self.sidebar_list.clone(),
            path_entry: self.path_entry.clone(),
            view_mode: self.view_mode.clone(),
        }
    }
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
        window.set_size_request(500, -1);
        
        // Create path entry for header
        let path_entry = Entry::new();
        
        // Create grid view first
        let factory = SignalListItemFactory::new();
        ui::setup_grid_view_factory(&factory);
        let selection_model = SingleSelection::new(None::<gio::ListStore>);
        let grid_view = GridView::new(Some(selection_model), Some(factory));
        
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
        let content_box = ui::create_main_content_area(&header_box, &list_box, &grid_view);

        // Create sidebar
        let (sidebar_scrolled, sidebar_list) = ui::create_sidebar();

        // Main paned layout with resizing constraints
        let paned = Paned::new(Orientation::Horizontal);
        paned.set_start_child(Some(&sidebar_scrolled));
        paned.set_end_child(Some(&content_box));
        paned.set_position(200);
        paned.set_shrink_start_child(true);
        paned.set_shrink_end_child(false);
        paned.set_resize_start_child(false);
        paned.set_resize_end_child(true);
        
        // Set window content
        window.set_content(Some(&paned));

        // Initialize path tracking
        let home_dir = dirs_next::home_dir().unwrap_or_else(|| PathBuf::from("/"));
        let current_path = Rc::new(RefCell::new(home_dir));
        let row_paths = Rc::new(RefCell::new(Vec::new()));
        
        // Main file list - initially hidden
        list_box.set_visible(false);
        
        // Grid view - initially visible
        grid_view.set_visible(true);
        
        // View mode tracking
        let view_mode = Rc::new(RefCell::new(ViewMode::Grid));
        
        // Create instance
        let file_manager = FileManager {
            current_path: current_path.clone(),
            list_box: list_box.clone(),
            grid_view: grid_view.clone(),
            window,
            row_paths: row_paths.clone(),
            sidebar_list: sidebar_list.clone(),
            path_entry: path_entry.clone(),
            view_mode,
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
            // Clone widget before downcast to avoid move
            if let Ok(button) = widget.clone().downcast::<Button>() {
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
        let list_box_clone = self.list_box.clone();
        let grid_view_clone = self.grid_view.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        
        self.sidebar_list.connect_row_activated(move |_, row| {
            let index = row.index();
            if let Some(path) = ui::get_sidebar_path(index) {
                Self::populate_files(&list_box_clone, &grid_view_clone, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            }
        });
    }

    fn toggle_view_mode(&self) {
        let mut view_mode = self.view_mode.borrow_mut();
        *view_mode = match *view_mode {
            ViewMode::List => ViewMode::Grid,
            ViewMode::Grid => ViewMode::List,
        };
        
        self.list_box.set_visible(*view_mode == ViewMode::List);
        self.grid_view.set_visible(*view_mode == ViewMode::Grid);
    }

    fn setup_file_list_handlers(&self) {
        let current_path_clone = self.current_path.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        let grid_view_clone = self.grid_view.clone();
        
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
                Self::populate_files(list_box, &grid_view_clone, &path, &row_paths_clone);
                path_entry_clone.set_text(&path.to_string_lossy());
            } else {
                let file = gio::File::for_path(&path);
                let _ = gio::AppInfo::launch_default_for_uri(&file.uri(), None::<&gio::AppLaunchContext>);
            }
        });
    }

    fn setup_navigation_handlers(&self, header_box: &gtk::Box) {
        // Find navigation buttons
        let mut buttons = Vec::new();
        let mut child = header_box.first_child();
        
        while let Some(widget) = child {
            if let Ok(button) = widget.clone().downcast::<Button>() {
                buttons.push(button);
            }
            child = widget.next_sibling();
        }

        // For the view toggle button
        if buttons.len() >= 5 {
            let view_mode_clone = self.view_mode.clone();
            let list_box_clone = self.list_box.clone();
            let grid_view_clone = self.grid_view.clone();
            
            buttons[4].connect_clicked(move |_| {
                let mut view_mode = view_mode_clone.borrow_mut();
                *view_mode = match *view_mode {
                    ViewMode::List => ViewMode::Grid,
                    ViewMode::Grid => ViewMode::List,
                };
                
                list_box_clone.set_visible(*view_mode == ViewMode::List);
                grid_view_clone.set_visible(*view_mode == ViewMode::Grid);
            });
        }
    }

    fn setup_back_button(&self, back_button: &Button) {
        let list_box_clone = self.list_box.clone();
        let grid_view_clone = self.grid_view.clone();
        let current_path_clone = self.current_path.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        
        back_button.connect_clicked(move |_| {
            Self::populate_files(&list_box_clone, &grid_view_clone, &current_path_clone.borrow(), &row_paths_clone);
            path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
        });
    }
    
    fn setup_up_button(&self, up_button: &Button) {
        let list_box_clone = self.list_box.clone();
        let grid_view_clone = self.grid_view.clone();
        let current_path_clone = self.current_path.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();
        
        up_button.connect_clicked(move |_| {
            let current = current_path_clone.borrow().clone();
            if let Some(parent) = current.parent() {
                *current_path_clone.borrow_mut() = parent.to_path_buf();
                FileManager::populate_files(&list_box_clone, &grid_view_clone, parent, &row_paths_clone);
                path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
            }
        });
    }

    fn setup_home_button(&self, home_button: &Button) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let grid_view_clone = self.grid_view.clone();
        let path_entry_clone = self.path_entry.clone();
        let row_paths_clone = self.row_paths.clone();

        home_button.connect_clicked(move |_| {
            if let Some(home_dir) = dirs_next::home_dir() {
                *current_path_clone.borrow_mut() = home_dir.clone();
                Self::populate_files(&list_box_clone, &grid_view_clone, &home_dir, &row_paths_clone);
                path_entry_clone.set_text(&current_path_clone.borrow().to_string_lossy());
            }
        });
    }

    fn setup_path_entry_handler(&self) {
        let current_path_clone = self.current_path.clone();
        let list_box_clone = self.list_box.clone();
        let grid_view_clone = self.grid_view.clone();
        let row_paths_clone = self.row_paths.clone();
        
        self.path_entry.connect_activate(move |entry| {
            let text = entry.text();
            let path = PathBuf::from(text.as_str());
            
            if path.exists() && path.is_dir() {
                *current_path_clone.borrow_mut() = path.clone();
                Self::populate_files(&list_box_clone, &grid_view_clone, &path, &row_paths_clone);
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
        Self::populate_files(&self.list_box, &self.grid_view, &current_path, &self.row_paths);
        self.update_path_display();
    }
    
    fn populate_files(list_box: &ListBox, grid_view: &GridView, path: &Path, row_paths: &Rc<RefCell<Vec<PathBuf>>>) {
        // Clear existing items
        while let Some(child) = list_box.first_child() {
            list_box.remove(&child);
        }
        
        // Clear grid view model
        if let Some(selection_model) = grid_view.model().and_downcast::<SingleSelection>() {
            selection_model.set_model(None::<&gio::ListStore>);
        }

        // Clear row paths
        row_paths.borrow_mut().clear();

        // Read directory contents
        let mut directories = Vec::new();
        let mut files = Vec::new();
        
        if let Ok(entries) = std::fs::read_dir(path) {
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
        
            // Create a ListStore for the grid view
            let store = gio::ListStore::new::<FileItem>();
            
            // Add directories to both views
            for entry in directories {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                let path = entry.path();
            
                // Store this path in our vector
                row_paths.borrow_mut().push(path.clone());
            
                // Create row for list view
                let row = ui::create_file_row(&path, &file_name);
                list_box.append(&row);
                
                // Create item for grid view
                let item = FileItem::new(
                    &file_name,
                    &path.to_string_lossy(),
                    crate::file_types::get_icon_for_file(&path)
                );
                store.append(&item);
            }
        
            // Add files to both views
            for entry in files {
                let file_name = entry.file_name();
                let file_name = file_name.to_string_lossy();
                let path = entry.path();
            
                // Store this path in our vector
                row_paths.borrow_mut().push(path.clone());
            
                // Create row for list view
                let row = ui::create_file_row(&path, &file_name);
                list_box.append(&row);
                
                // Create item for grid view
                let item = FileItem::new(
                    &file_name,
                    &path.to_string_lossy(),
                    crate::file_types::get_icon_for_file(&path)
                );
                store.append(&item);
            }
            
            // Set the model for grid view
            let selection_model = SingleSelection::new(Some(store));
            grid_view.set_model(Some(&selection_model));
        }
    }    
    
    pub fn show_all(&self) {
        self.window.show();
    }
}
