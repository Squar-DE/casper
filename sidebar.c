#include "filemanager.h"

typedef struct {
    GFile *file;
    FileManager *casper;
} SidebarItemData;

// Setup sidebar
void setup_sidebar(FileManager *casper) {
    // Create listbox for sidebar
    casper->sidebar_listbox = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(casper->sidebar_listbox, GTK_SELECTION_SINGLE);
    
    // Let the parent container handle sizing, just make it expand vertically
    gtk_widget_set_vexpand(GTK_WIDGET(casper->sidebar_listbox), TRUE);
    
    // Connect row activated signal
    g_signal_connect(casper->sidebar_listbox, "row-activated", G_CALLBACK(on_sidebar_row_activated), casper);
    
    // Populate sidebar
    populate_sidebar(casper);
}

// Add item to sidebar
void add_sidebar_item(FileManager *casper, const char *name, const char *icon, GFile *file, gboolean is_separator) {
    if (is_separator) {
        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_list_box_insert(casper->sidebar_listbox, separator, -1);
        return;
    }
    
    // Create row
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    
    // Add icon
    GtkWidget *icon_widget = gtk_image_new_from_icon_name(icon);
    gtk_image_set_icon_size(GTK_IMAGE(icon_widget), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(box), icon_widget);
    
    // Add label
    GtkWidget *label = gtk_label_new(name);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(box), label);
    
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    gtk_list_box_insert(casper->sidebar_listbox, row, -1);
    
    // Store file reference in row data
    SidebarItemData *data = g_new0(SidebarItemData, 1);
    data->file = g_object_ref(file);
    data->casper = casper;
    g_object_set_data_full(G_OBJECT(row), "sidebar-item-data", data, (GDestroyNotify)g_free);
}

// Populate sidebar with items
void populate_sidebar(FileManager *casper) {
    // Home directory
    GFile *home = g_file_new_for_path(g_get_home_dir());
    add_sidebar_item(casper, "Home", "user-home-symbolic", home, FALSE);
    g_object_unref(home);
    
    add_sidebar_item(casper, NULL, NULL, NULL, TRUE); // Separator
    
    // Standard directories
    const char *standard_dirs[] = {
        "Downloads", "Music", "Pictures", "Videos", "Documents"
    };
    
    const char *standard_icons[] = {
        "folder-download-symbolic", "folder-music-symbolic", 
        "folder-pictures-symbolic", "folder-videos-symbolic", 
        "folder-documents-symbolic"
    };
    
    for (int i = 0; i < 5; i++) {
        char *path = g_build_filename(g_get_home_dir(), standard_dirs[i], NULL);
        GFile *dir = g_file_new_for_path(path);
        
        // Only add if directory exists
        if (g_file_query_exists(dir, NULL)) {
            add_sidebar_item(casper, standard_dirs[i], standard_icons[i], dir, FALSE);
        }
        
        g_object_unref(dir);
        g_free(path);
    }
    
    // Check for mount points
    GVolumeMonitor *monitor = g_volume_monitor_get();
    GList *mounts = g_volume_monitor_get_mounts(monitor);
    
    if (mounts) {
        add_sidebar_item(casper, NULL, NULL, NULL, TRUE); // Separator
        
        for (GList *l = mounts; l != NULL; l = l->next) {
            GMount *mount = l->data;
            GFile *root = g_mount_get_root(mount);
            char *name = g_mount_get_name(mount);
            
            add_sidebar_item(casper, name, "drive-harddisk-symbolic", root, FALSE);
            
            g_free(name);
            g_object_unref(root);
            g_object_unref(mount);
        }
        
        g_list_free(mounts);
    }
    
    g_object_unref(monitor);
    
    add_sidebar_item(casper, NULL, NULL, NULL, TRUE); // Separator
    
    // Trash
    GFile *trash = g_file_new_for_uri("trash://");
    add_sidebar_item(casper, "Trash", "user-trash-symbolic", trash, FALSE);
    g_object_unref(trash);
}

// Handle sidebar row activation
void on_sidebar_row_activated(GtkListBox *listbox, GtkListBoxRow *row, FileManager *casper) {
    SidebarItemData *data = g_object_get_data(G_OBJECT(row), "sidebar-item-data");
    if (data && data->file) {
        // Add current to back history and clear forward history
        g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
        g_queue_clear_full(casper->forward_history, g_object_unref);
        
        // Load the selected directory
        load_directory(casper, data->file);
        update_navigation_buttons(casper);
    }
}
