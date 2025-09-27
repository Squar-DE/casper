#include "filemanager.h"

// Toggle between grid and list view
void on_view_toggle_clicked(GtkButton *button, FileManager *casper) {
    // Switch the view flag
    casper->is_grid_view = !casper->is_grid_view;
    
    if (casper->is_grid_view) {
        gtk_button_set_icon_name(button, "view-list");
        gtk_stack_set_visible_child_name(casper->view_stack, "grid");
    } else {
        gtk_button_set_icon_name(button, "view-grid");
        gtk_stack_set_visible_child_name(casper->view_stack, "list");
    }
    
    // Clear selections
    gtk_single_selection_set_selected(casper->list_selection_model, GTK_INVALID_LIST_POSITION);
    gtk_single_selection_set_selected(casper->grid_selection_model, GTK_INVALID_LIST_POSITION);
}

// Handle file/folder activation (common logic)
static void activate_item_at_position(guint position, FileManager *casper) {
    GtkStringList *current_list = casper->is_grid_view ? casper->grid_string_list : casper->list_string_list;
    GtkStringObject *obj = g_list_model_get_item(G_LIST_MODEL(current_list), position);
    if (!obj) return;
    
    const char *item_text = gtk_string_object_get_string(obj);
    g_object_unref(obj);
    
    // Extract filename from the display text (first part before the tab)
    char **parts = g_strsplit(item_text, "\t", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return;
    }
    
    GFile *child = g_file_get_child(casper->current_directory, parts[0]);
    g_strfreev(parts);
    
    GFileInfo *info = g_file_query_info(child, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
        g_queue_clear_full(casper->forward_history, g_object_unref);
        load_directory(casper, child);
        update_navigation_buttons(casper);
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

// Handle double-click on list item
void on_item_activated_list(GtkListView *list_view, guint position, FileManager *casper) {
    activate_item_at_position(position, casper);
}

// Handle double-click on grid item
void on_item_activated_grid(GtkGridView *grid_view, guint position, FileManager *casper) {
    activate_item_at_position(position, casper);
}

// Setup list view factory
void setup_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *casper) {
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

void bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *casper) {
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

// Setup grid view factory
void setup_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *casper) {
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

void bind_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *casper) {
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
