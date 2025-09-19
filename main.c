#include <gtk/gtk.h>
#include <adwaita.h>
#include <gio/gio.h>
#include <glib.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    GtkApplication *app;
    GtkApplicationWindow *window;
    AdwHeaderBar *header_bar;
    GtkButton *back_btn;
    GtkButton *forward_btn;
    GtkButton *up_btn;
    GtkButton *view_toggle_btn;
    GtkEntry *path_entry;
    GtkScrolledWindow *scrolled_window;
    GtkListView *list_view;
    GtkGridView *grid_view;
    GtkStringList *string_list;
    GtkSingleSelection *selection_model;
    
    GFile *current_directory;
    GQueue *back_history;
    GQueue *forward_history;
    
    GtkPopover *context_menu;
    GtkWidget *selected_item;
    gboolean is_grid_view;
} FileManager;

static void update_navigation_buttons(FileManager *fm);
static void load_directory(FileManager *fm, GFile *directory);
static void refresh_current_directory(FileManager *fm);

// Format file size for display
static char* format_file_size(goffset size) {
    if (size < 1024) {
        return g_strdup_printf("%ld B", size);
    } else if (size < 1024 * 1024) {
        return g_strdup_printf("%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        return g_strdup_printf("%.1f MB", size / (1024.0 * 1024.0));
    } else {
        return g_strdup_printf("%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

// Format file modification time
static char* format_file_time(GTimeVal *time) {
    GDateTime *dt = g_date_time_new_from_timeval_local(time);
    char *formatted = g_date_time_format(dt, "%Y-%m-%d %H:%M");
    g_date_time_unref(dt);
    return formatted;
}

// Get file icon name based on file type
static const char* get_file_icon(GFile *file, GFileInfo *info) {
    GFileType type = g_file_info_get_file_type(info);
    const char *content_type = g_file_info_get_content_type(info);
    
    if (type == G_FILE_TYPE_DIRECTORY) {
        return "folder";
    }
    
    if (content_type) {
        if (g_str_has_prefix(content_type, "image/")) {
            return "image-x-generic";
        } else if (g_str_has_prefix(content_type, "text/")) {
            return "text-x-generic";
        } else if (g_str_has_prefix(content_type, "audio/")) {
            return "audio-x-generic";
        } else if (g_str_has_prefix(content_type, "video/")) {
            return "video-x-generic";
        }
    }
    
    return "text-x-generic";
}

// Create list item widget
static GtkWidget* create_list_item(const char *name, const char *icon_name, 
                                 const char *size, const char *modified) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    
    // Icon
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
    gtk_box_append(GTK_BOX(box), icon);
    
    // Name
    GtkWidget *name_label = gtk_label_new(name);
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_widget_set_hexpand(name_label, TRUE);
    gtk_box_append(GTK_BOX(box), name_label);
    
    // Size
    GtkWidget *size_label = gtk_label_new(size);
    gtk_label_set_xalign(GTK_LABEL(size_label), 1.0);
    gtk_widget_set_size_request(size_label, 80, -1);
    gtk_box_append(GTK_BOX(box), size_label);
    
    // Modified date
    GtkWidget *modified_label = gtk_label_new(modified);
    gtk_label_set_xalign(GTK_LABEL(modified_label), 1.0);
    gtk_widget_set_size_request(modified_label, 120, -1);
    gtk_box_append(GTK_BOX(box), modified_label);
    
    return box;
}

// Navigate back
static void on_back_clicked(GtkButton *button, FileManager *fm) {
    if (g_queue_is_empty(fm->back_history)) return;
    
    // Add current to forward history
    g_queue_push_head(fm->forward_history, g_object_ref(fm->current_directory));
    
    // Get previous directory
    GFile *prev_dir = G_FILE(g_queue_pop_head(fm->back_history));
    load_directory(fm, prev_dir);
    g_object_unref(prev_dir);
    
    update_navigation_buttons(fm);
}

// Navigate forward
static void on_forward_clicked(GtkButton *button, FileManager *fm) {
    if (g_queue_is_empty(fm->forward_history)) return;
    
    // Add current to back history
    g_queue_push_head(fm->back_history, g_object_ref(fm->current_directory));
    
    // Get next directory
    GFile *next_dir = G_FILE(g_queue_pop_head(fm->forward_history));
    load_directory(fm, next_dir);
    g_object_unref(next_dir);
    
    update_navigation_buttons(fm);
}

// Navigate up
static void on_up_clicked(GtkButton *button, FileManager *fm) {
    GFile *parent = g_file_get_parent(fm->current_directory);
    if (parent) {
        g_queue_push_head(fm->back_history, g_object_ref(fm->current_directory));
        g_queue_clear_full(fm->forward_history, g_object_unref);
        load_directory(fm, parent);
        g_object_unref(parent);
        update_navigation_buttons(fm);
    }
}

// Handle path entry activation
static void on_path_activated(GtkEntry *entry, FileManager *fm) {
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    GFile *file = g_file_new_for_path(path);
    
    if (g_file_query_exists(file, NULL)) {
        GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            g_queue_push_head(fm->back_history, g_object_ref(fm->current_directory));
            g_queue_clear_full(fm->forward_history, g_object_unref);
            load_directory(fm, file);
            update_navigation_buttons(fm);
        }
        if (info) g_object_unref(info);
    }
    
    g_object_unref(file);
}

// Toggle between grid and list view
static void on_view_toggle_clicked(GtkButton *button, FileManager *fm) {
    fm->is_grid_view = !fm->is_grid_view;
    
    if (fm->is_grid_view) {
        gtk_button_set_icon_name(button, "view-list");
        gtk_scrolled_window_set_child(fm->scrolled_window, GTK_WIDGET(fm->grid_view));
    } else {
        gtk_button_set_icon_name(button, "view-grid");
        gtk_scrolled_window_set_child(fm->scrolled_window, GTK_WIDGET(fm->list_view));
    }
}

// Handle double-click on list item
static void on_item_activated_list(GtkListView *list_view, guint position, FileManager *fm) {
    GtkStringObject *obj = g_list_model_get_item(G_LIST_MODEL(fm->string_list), position);
    if (!obj) return;
    
    const char *item_text = gtk_string_object_get_string(obj);
    g_object_unref(obj);
    
    // Extract filename from the display text (first part before the tab)
    char **parts = g_strsplit(item_text, "\t", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return;
    }
    
    GFile *child = g_file_get_child(fm->current_directory, parts[0]);
    g_strfreev(parts);
    
    GFileInfo *info = g_file_query_info(child, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        g_queue_push_head(fm->back_history, g_object_ref(fm->current_directory));
        g_queue_clear_full(fm->forward_history, g_object_unref);
        load_directory(fm, child);
        update_navigation_buttons(fm);
    } else {
        // Open file with default application
        GError *error = NULL;
        if (!g_app_info_launch_default_for_uri(g_file_get_uri(child), NULL, &error)) {
            g_print("Error opening file: %s\n", error->message);
            g_error_free(error);
        }
    }
    
    if (info) g_object_unref(info);
    g_object_unref(child);
}

// Handle double-click on grid item
static void on_item_activated_grid(GtkGridView *grid_view, guint position, FileManager *fm) {
    GtkStringObject *obj = g_list_model_get_item(G_LIST_MODEL(fm->string_list), position);
    if (!obj) return;
    
    const char *item_text = gtk_string_object_get_string(obj);
    g_object_unref(obj);
    
    // Extract filename from the display text (first part before the tab)
    char **parts = g_strsplit(item_text, "\t", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return;
    }
    
    GFile *child = g_file_get_child(fm->current_directory, parts[0]);
    g_strfreev(parts);
    
    GFileInfo *info = g_file_query_info(child, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        g_queue_push_head(fm->back_history, g_object_ref(fm->current_directory));
        g_queue_clear_full(fm->forward_history, g_object_unref);
        load_directory(fm, child);
        update_navigation_buttons(fm);
    } else {
        // Open file with default application
        GError *error = NULL;
        if (!g_app_info_launch_default_for_uri(g_file_get_uri(child), NULL, &error)) {
            g_print("Error opening file: %s\n", error->message);
            g_error_free(error);
        }
    }
    
    if (info) g_object_unref(info);
    g_object_unref(child);
}
// Update navigation button sensitivity
static void update_navigation_buttons(FileManager *fm) {
    gtk_widget_set_sensitive(GTK_WIDGET(fm->back_btn), !g_queue_is_empty(fm->back_history));
    gtk_widget_set_sensitive(GTK_WIDGET(fm->forward_btn), !g_queue_is_empty(fm->forward_history));
    
    GFile *parent = g_file_get_parent(fm->current_directory);
    gtk_widget_set_sensitive(GTK_WIDGET(fm->up_btn), parent != NULL);
    if (parent) g_object_unref(parent);
}

// Load directory contents
static void load_directory(FileManager *fm, GFile *directory) {
    if (fm->current_directory) {
        g_object_unref(fm->current_directory);
    }
    fm->current_directory = g_object_ref(directory);
    
    // Update path entry
    char *path = g_file_get_path(directory);
    if (path) {
        gtk_editable_set_text(GTK_EDITABLE(fm->path_entry), path);
        g_free(path);
    }
    
    // Clear current list
    gtk_string_list_splice(fm->string_list, 0, 
                          g_list_model_get_n_items(G_LIST_MODEL(fm->string_list)), NULL);
    
    // Enumerate directory contents
    GError *error = NULL;
    GFileEnumerator *enumerator = g_file_enumerate_children(
        directory,
        G_FILE_ATTRIBUTE_STANDARD_NAME ","
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_STANDARD_SIZE ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED,
        G_FILE_QUERY_INFO_NONE,
        NULL, &error);
    
    if (error) {
        g_print("Error enumerating directory: %s\n", error->message);
        g_error_free(error);
        return;
    }
    
    GList *file_list = NULL;
    GFileInfo *info;
    
    while ((info = g_file_enumerator_next_file(enumerator, NULL, NULL)) != NULL) {
        const char *name = g_file_info_get_name(info);
        GFileType type = g_file_info_get_file_type(info);
        goffset size = g_file_info_get_size(info);
        
        GTimeVal modified_time;
        g_file_info_get_modification_time(info, &modified_time);
        
        char *size_str = (type == G_FILE_TYPE_DIRECTORY) ? g_strdup("") : format_file_size(size);
        char *time_str = format_file_time(&modified_time);
        
        // Create display string (using tabs for simple parsing)
        char *display_text = g_strdup_printf("%s\t%s\t%s\t%d", 
                                           name, size_str, time_str, (int)type);
        
        file_list = g_list_prepend(file_list, display_text);
        
        g_free(size_str);
        g_free(time_str);
        g_object_unref(info);
    }
    
    g_file_enumerator_close(enumerator, NULL, NULL);
    g_object_unref(enumerator);
    
    // Sort list (directories first, then alphabetically)
    file_list = g_list_sort(file_list, (GCompareFunc)strcmp);
    
    // Add to string list
    for (GList *l = file_list; l != NULL; l = l->next) {
        gtk_string_list_append(fm->string_list, (char*)l->data);
    }
    
    g_list_free_full(file_list, g_free);
}

// Refresh current directory
static void refresh_current_directory(FileManager *fm) {
    if (fm->current_directory) {
        load_directory(fm, fm->current_directory);
    }
}

// Create context menu
static void create_context_menu(FileManager *fm) {
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Open", "app.open");
    g_menu_append(menu, "Refresh", "app.refresh");
    
    fm->context_menu = GTK_POPOVER(gtk_popover_menu_new_from_model(G_MENU_MODEL(menu)));
    gtk_widget_set_parent(GTK_WIDGET(fm->context_menu), GTK_WIDGET(fm->list_view));
}

// Setup grid view factory
static void setup_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(box, 120, 140);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    
    GtkWidget *icon = gtk_image_new();
    gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), icon);
    
    GtkWidget *name_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.5);
    gtk_label_set_wrap(GTK_LABEL(name_label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(name_label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(name_label), 12);
    gtk_label_set_lines(GTK_LABEL(name_label), 2);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(name_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), name_label);
    
    gtk_list_item_set_child(list_item, box);
}

static void bind_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm) {
    GtkStringObject *obj = gtk_list_item_get_item(list_item);
    if (!obj) return;
    
    const char *text = gtk_string_object_get_string(obj);
    char **parts = g_strsplit(text, "\t", 4);
    
    if (g_strv_length(parts) >= 4) {
        GtkWidget *box = gtk_list_item_get_child(list_item);
        GtkWidget *icon = gtk_widget_get_first_child(box);
        GtkWidget *name_label = gtk_widget_get_next_sibling(icon);
        
        // Set icon based on file type
        int file_type = atoi(parts[3]);
        const char *icon_name = (file_type == G_FILE_TYPE_DIRECTORY) ? 
                               "folder" : "text-x-generic";
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name);
        
        gtk_label_set_text(GTK_LABEL(name_label), parts[0]);
    }
    
    g_strfreev(parts);
}

// Setup list view factory
static void setup_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);
    
    GtkWidget *icon = gtk_image_new();
    gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_LARGE);
    gtk_box_append(GTK_BOX(box), icon);
    
    GtkWidget *name_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_widget_set_hexpand(name_label, TRUE);
    gtk_box_append(GTK_BOX(box), name_label);
    
    GtkWidget *size_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(size_label), 1.0);
    gtk_widget_set_size_request(size_label, 80, -1);
    gtk_box_append(GTK_BOX(box), size_label);
    
    GtkWidget *time_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(time_label), 1.0);
    gtk_widget_set_size_request(time_label, 120, -1);
    gtk_box_append(GTK_BOX(box), time_label);
    
    gtk_list_item_set_child(list_item, box);
}

static void bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm) {
    GtkStringObject *obj = gtk_list_item_get_item(list_item);
    if (!obj) return;
    
    const char *text = gtk_string_object_get_string(obj);
    char **parts = g_strsplit(text, "\t", 4);
    
    if (g_strv_length(parts) >= 4) {
        GtkWidget *box = gtk_list_item_get_child(list_item);
        GtkWidget *icon = gtk_widget_get_first_child(box);
        GtkWidget *name_label = gtk_widget_get_next_sibling(icon);
        GtkWidget *size_label = gtk_widget_get_next_sibling(name_label);
        GtkWidget *time_label = gtk_widget_get_next_sibling(size_label);
        
        // Set icon based on file type
        int file_type = atoi(parts[3]);
        const char *icon_name = (file_type == G_FILE_TYPE_DIRECTORY) ? 
                               "folder" : "text-x-generic";
        gtk_image_set_from_icon_name(GTK_IMAGE(icon), icon_name);
        
        gtk_label_set_text(GTK_LABEL(name_label), parts[0]);
        gtk_label_set_text(GTK_LABEL(size_label), parts[1]);
        gtk_label_set_text(GTK_LABEL(time_label), parts[2]);
    }
    
    g_strfreev(parts);
}

// Application actions
static void on_refresh_action(GSimpleAction *action, GVariant *parameter, FileManager *fm) {
    refresh_current_directory(fm);
}

static void on_open_action(GSimpleAction *action, GVariant *parameter, FileManager *fm) {
    guint position = gtk_single_selection_get_selected(fm->selection_model);
    if (position != GTK_INVALID_LIST_POSITION) {
        if (fm->is_grid_view) {
            on_item_activated_grid(fm->grid_view, position, fm);
        } else {
            on_item_activated_list(fm->list_view, position, fm);
        }
    }
}

// Application activate callback
static void on_activate(GtkApplication *app, FileManager *fm) {
    // Create main window
    fm->window = GTK_APPLICATION_WINDOW(adw_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(fm->window), "File Manager");
    gtk_window_set_default_size(GTK_WINDOW(fm->window), 900, 600);
    
    // Create main box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(fm->window), main_box);
    
    // Create header bar
    fm->header_bar = ADW_HEADER_BAR(adw_header_bar_new());
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(fm->header_bar));
    
    // Create navigation buttons
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nav_box, "linked");
    
    fm->back_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-previous"));
    fm->forward_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-next"));
    fm->up_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-up"));
    
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(fm->back_btn));
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(fm->forward_btn));
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(fm->up_btn));
    
    adw_header_bar_pack_start(fm->header_bar, nav_box);
    
    // Create view toggle button
    fm->view_toggle_btn = GTK_BUTTON(gtk_button_new_from_icon_name("view-grid"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(fm->view_toggle_btn), "Toggle Grid/List View");
    adw_header_bar_pack_end(fm->header_bar, GTK_WIDGET(fm->view_toggle_btn));
    
    // Create path entry
    fm->path_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(fm->path_entry), TRUE);
    adw_header_bar_set_title_widget(fm->header_bar, GTK_WIDGET(fm->path_entry));
    
    // Create list view
    fm->string_list = gtk_string_list_new(NULL);
    fm->selection_model = gtk_single_selection_new(G_LIST_MODEL(fm->string_list));
    fm->list_view = GTK_LIST_VIEW(gtk_list_view_new(GTK_SELECTION_MODEL(fm->selection_model), NULL));
    
    // Create grid view
    fm->grid_view = GTK_GRID_VIEW(gtk_grid_view_new(GTK_SELECTION_MODEL(fm->selection_model), NULL));
    gtk_grid_view_set_max_columns(fm->grid_view, 8);
    gtk_grid_view_set_min_columns(fm->grid_view, 2);
    
    // Setup list factory
    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(list_factory, "setup", G_CALLBACK(setup_factory), fm);
    g_signal_connect(list_factory, "bind", G_CALLBACK(bind_factory), fm);
    gtk_list_view_set_factory(fm->list_view, list_factory);
    
    // Setup grid factory
    GtkListItemFactory *grid_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(grid_factory, "setup", G_CALLBACK(setup_grid_factory), fm);
    g_signal_connect(grid_factory, "bind", G_CALLBACK(bind_grid_factory), fm);
    gtk_grid_view_set_factory(fm->grid_view, grid_factory);
    
    // Initialize with list view (is_grid_view = FALSE by default)
    fm->is_grid_view = FALSE;
    
    // Create scrolled window
    fm->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_child(fm->scrolled_window, GTK_WIDGET(fm->list_view));
    gtk_widget_set_vexpand(GTK_WIDGET(fm->scrolled_window), TRUE);
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(fm->scrolled_window));
    
    // Initialize history queues
    fm->back_history = g_queue_new();
    fm->forward_history = g_queue_new();
    
    // Connect signals
    g_signal_connect(fm->back_btn, "clicked", G_CALLBACK(on_back_clicked), fm);
    g_signal_connect(fm->forward_btn, "clicked", G_CALLBACK(on_forward_clicked), fm);
    g_signal_connect(fm->up_btn, "clicked", G_CALLBACK(on_up_clicked), fm);
    g_signal_connect(fm->view_toggle_btn, "clicked", G_CALLBACK(on_view_toggle_clicked), fm);
    g_signal_connect(fm->path_entry, "activate", G_CALLBACK(on_path_activated), fm);
    g_signal_connect(fm->list_view, "activate", G_CALLBACK(on_item_activated_list), fm);
    g_signal_connect(fm->grid_view, "activate", G_CALLBACK(on_item_activated_grid), fm);
    
    // Create actions
    GSimpleAction *refresh_action = g_simple_action_new("refresh", NULL);
    g_signal_connect(refresh_action, "activate", G_CALLBACK(on_refresh_action), fm);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(refresh_action));
    
    GSimpleAction *open_action = g_simple_action_new("open", NULL);
    g_signal_connect(open_action, "activate", G_CALLBACK(on_open_action), fm);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(open_action));
    
    // Load home directory
    const char *home = g_get_home_dir();
    GFile *home_file = g_file_new_for_path(home);
    load_directory(fm, home_file);
    g_object_unref(home_file);
    
    update_navigation_buttons(fm);
    
    gtk_window_present(GTK_WINDOW(fm->window));
}

int main(int argc, char *argv[]) {
    FileManager fm = {0};
    
    // Create application
    fm.app = GTK_APPLICATION(adw_application_new("com.example.filemanager", 
                                                G_APPLICATION_DEFAULT_FLAGS));
    
    g_signal_connect(fm.app, "activate", G_CALLBACK(on_activate), &fm);
    
    int status = g_application_run(G_APPLICATION(fm.app), argc, argv);
    
    // Cleanup
    if (fm.current_directory) g_object_unref(fm.current_directory);
    if (fm.back_history) {
        g_queue_free_full(fm.back_history, g_object_unref);
    }
    if (fm.forward_history) {
        g_queue_free_full(fm.forward_history, g_object_unref);
    }
    g_object_unref(fm.app);
    
    return status;
}
