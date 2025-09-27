#include "filemanager.h"

// Navigate back
void on_back_clicked(GtkButton *button, FileManager *casper) {
    if (g_queue_is_empty(casper->back_history)) return;
    
    // Add current to forward history
    g_queue_push_head(casper->forward_history, g_object_ref(casper->current_directory));
    
    // Get previous directory
    GFile *prev_dir = G_FILE(g_queue_pop_head(casper->back_history));
    load_directory(casper, prev_dir);
    g_object_unref(prev_dir);
    
    update_navigation_buttons(casper);
}

// Navigate forward
void on_forward_clicked(GtkButton *button, FileManager *casper) {
    if (g_queue_is_empty(casper->forward_history)) return;
    
    // Add current to back history
    g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
    
    // Get next directory
    GFile *next_dir = G_FILE(g_queue_pop_head(casper->forward_history));
    load_directory(casper, next_dir);
    g_object_unref(next_dir);
    
    update_navigation_buttons(casper);
}

// Navigate up
void on_up_clicked(GtkButton *button, FileManager *casper) {
    GFile *parent = g_file_get_parent(casper->current_directory);
    if (parent) {
        g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
        g_queue_clear_full(casper->forward_history, g_object_unref);
        load_directory(casper, parent);
        g_object_unref(parent);
        update_navigation_buttons(casper);
    }
}

// Handle path entry activation
void on_path_activated(GtkEntry *entry, FileManager *casper) {
    const char *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    GFile *file = g_file_new_for_path(path);
    
    if (g_file_query_exists(file, NULL)) {
        GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
            g_queue_clear_full(casper->forward_history, g_object_unref);
            load_directory(casper, file);
            update_navigation_buttons(casper);
        }
        if (info) g_object_unref(info);
    }
    
    g_object_unref(file);
}

// Update navigation button sensitivity
void update_navigation_buttons(FileManager *casper) {
    gtk_widget_set_sensitive(GTK_WIDGET(casper->back_btn), !g_queue_is_empty(casper->back_history));
    gtk_widget_set_sensitive(GTK_WIDGET(casper->forward_btn), !g_queue_is_empty(casper->forward_history));
    
    GFile *parent = g_file_get_parent(casper->current_directory);
    gtk_widget_set_sensitive(GTK_WIDGET(casper->up_btn), parent != NULL);
    if (parent) g_object_unref(parent);
}
