#include "filemanager.h"

static void on_delete_dialog_response(GtkDialog *dialog, int response, FileManager *casper);
static void on_open_with_dialog_response(GtkDialog *dialog, int response, FileManager *casper);
static void on_move_to_dialog_response(GtkDialog *dialog, int response, FileManager *casper);
static void on_clipboard_text_received(GObject *source, GAsyncResult *result, gpointer user_data);

static void clear_selected_file(FileManager *casper) {
    if (casper->selected_file) {
        g_object_unref(casper->selected_file);
        casper->selected_file = NULL;
    }
}

// Setup context menu actions - connect signals only once here
void setup_context_menu_actions(GtkApplication *app) {
    // Get the FileManager from application data
    FileManager *casper = g_object_get_data(G_OBJECT(app), "file-manager");
    if (!casper) return;
    
    // Context menu actions
    GSimpleAction *ctx_open = g_simple_action_new("context-open", NULL);
    g_signal_connect(ctx_open, "activate", G_CALLBACK(on_context_menu_open), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_open));
    
    GSimpleAction *ctx_open_with = g_simple_action_new("context-open-with", NULL);
    g_signal_connect(ctx_open_with, "activate", G_CALLBACK(on_context_menu_open_with), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_open_with));
    
    GSimpleAction *ctx_cut = g_simple_action_new("context-cut", NULL);
    g_signal_connect(ctx_cut, "activate", G_CALLBACK(on_context_menu_cut), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_cut));
    
    GSimpleAction *ctx_copy = g_simple_action_new("context-copy", NULL);
    g_signal_connect(ctx_copy, "activate", G_CALLBACK(on_context_menu_copy), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_copy));
    
    GSimpleAction *ctx_paste = g_simple_action_new("context-paste", NULL);
    g_signal_connect(ctx_paste, "activate", G_CALLBACK(on_context_menu_paste), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_paste));
    
    GSimpleAction *ctx_move_to = g_simple_action_new("context-move-to", NULL);
    g_signal_connect(ctx_move_to, "activate", G_CALLBACK(on_context_menu_move_to), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_move_to));
    
    GSimpleAction *ctx_delete = g_simple_action_new("context-delete", NULL);
    g_signal_connect(ctx_delete, "activate", G_CALLBACK(on_context_menu_delete), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(ctx_delete));
}

// Handle right-click
gboolean on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, FileManager *casper) {
    if (n_press != 1) return FALSE;
    
    // Get the clicked widget
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    
    // Get the position by checking selection models
    guint position = GTK_INVALID_LIST_POSITION;
    
    if (casper->is_grid_view) {
        position = gtk_single_selection_get_selected(casper->grid_selection_model);
    } else {
        position = gtk_single_selection_get_selected(casper->list_selection_model);
    }
    
    if (position == GTK_INVALID_LIST_POSITION) return FALSE;
    
    // Get the file at this position
    GtkStringList *current_list = casper->is_grid_view ? casper->grid_string_list : casper->list_string_list;
    GtkStringObject *obj = g_list_model_get_item(G_LIST_MODEL(current_list), position);
    if (!obj) return FALSE;
    
    const char *item_text = gtk_string_object_get_string(obj);
    g_object_unref(obj);
    
    // Extract filename
    char **parts = g_strsplit(item_text, "\t", 2);
    if (!parts[0]) {
        g_strfreev(parts);
        return FALSE;
    }
    
    // Store the selected file
    if (casper->selected_file) {
        g_object_unref(casper->selected_file);
    }
    casper->selected_file = g_file_get_child(casper->current_directory, parts[0]);
    g_strfreev(parts);
    
    // Calculate menu position (convert to widget coordinates)
    GdkRectangle rect;
    rect.x = x;
    rect.y = y;
    rect.width = 1;
    rect.height = 1;
    
    // Show context menu
    show_context_menu(casper, casper->selected_file, &rect);
    
    return TRUE;
}

// Show context menu
void show_context_menu(FileManager *casper, GFile *file, GdkRectangle *rect) {
    // Create menu model
    GMenu *menu = g_menu_new();
    
    // Query file info to determine if it's a directory or file
    GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    GFileType file_type = G_FILE_TYPE_UNKNOWN;
    if (info) {
        file_type = g_file_info_get_file_type(info);
        g_object_unref(info);
    }
    
    // Add menu items
    g_menu_append(menu, "Open", "app.context-open");
    
    // Only show "Open with..." for files (not directories)
    if (file_type != G_FILE_TYPE_DIRECTORY) {
        g_menu_append(menu, "Open with...", "app.context-open-with");
    }
    
    g_menu_append(menu, "Cut", "app.context-cut");
    g_menu_append(menu, "Copy", "app.context-copy");
    g_menu_append(menu, "Paste", "app.context-paste");
    g_menu_append(menu, "Move to...", "app.context-move-to");
    g_menu_append(menu, "Delete", "app.context-delete");
    
    // Create popover menu
    if (casper->context_menu) {
        gtk_widget_unparent(GTK_WIDGET(casper->context_menu));
        g_object_unref(casper->context_menu);
        casper->context_menu = NULL;
    }
    
    GtkWidget *popover_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    casper->context_menu = GTK_POPOVER_MENU(popover_menu);
    
    // Set parent and show
    GtkWidget *current_view = casper->is_grid_view ? GTK_WIDGET(casper->grid_view) : GTK_WIDGET(casper->list_view);
    gtk_widget_set_parent(GTK_WIDGET(casper->context_menu), current_view);
    gtk_popover_set_pointing_to(GTK_POPOVER(casper->context_menu), rect);
    gtk_popover_popup(GTK_POPOVER(casper->context_menu));
    
    g_object_unref(menu);
}

// Context menu action handlers
void on_context_menu_open(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    GFileInfo *info = g_file_query_info(casper->selected_file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    if (info) {
        GFileType file_type = g_file_info_get_file_type(info);
        
        if (file_type == G_FILE_TYPE_DIRECTORY) {
            // Navigate to directory
            g_queue_push_head(casper->back_history, g_object_ref(casper->current_directory));
            g_queue_clear_full(casper->forward_history, g_object_unref);
            load_directory(casper, casper->selected_file);
            update_navigation_buttons(casper);
        } else {
            // Open file with default application
            GError *error = NULL;
            if (!g_app_info_launch_default_for_uri(g_file_get_uri(casper->selected_file), NULL, &error)) {
                g_print("Error opening file: %s\n", error->message);
                g_error_free(error);
            }
        }
        
        g_object_unref(info);
    }
    
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
    clear_selected_file(casper);
}

void on_context_menu_open_with(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    // Get all apps that can handle this file
    GFileInfo *info = g_file_query_info(casper->selected_file, 
                                       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                       G_FILE_QUERY_INFO_NONE, NULL, NULL);
    
    if (!info) {
        gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
        return;
    }
    
    const char *content_type = g_file_info_get_content_type(info);
    GList *apps = g_app_info_get_all_for_type(content_type);
    g_object_unref(info);
    
    if (!apps) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(casper->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "No applications found for this file type.");
        gtk_window_present(GTK_WINDOW(dialog));
        g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
        gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
        return;
    }
    
    // Create dialog with app chooser
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Open With",
                                                    GTK_WINDOW(casper->window),
                                                    GTK_DIALOG_MODAL,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Open", GTK_RESPONSE_OK,
                                                    NULL);
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Create scrolled window for the app chooser
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_widget_set_hexpand(scrolled, TRUE);
    
    GtkWidget *chooser = gtk_app_chooser_widget_new(content_type);
    gtk_app_chooser_widget_set_show_default(GTK_APP_CHOOSER_WIDGET(chooser), TRUE);
    gtk_app_chooser_widget_set_show_recommended(GTK_APP_CHOOSER_WIDGET(chooser), TRUE);
    gtk_app_chooser_widget_set_show_fallback(GTK_APP_CHOOSER_WIDGET(chooser), FALSE);
    gtk_app_chooser_widget_set_show_other(GTK_APP_CHOOSER_WIDGET(chooser), FALSE);
    gtk_app_chooser_widget_set_show_all(GTK_APP_CHOOSER_WIDGET(chooser), FALSE);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), chooser);
    gtk_box_append(GTK_BOX(content), scrolled);
    
    g_object_set_data_full(G_OBJECT(dialog), "chooser", chooser, NULL);
    g_signal_connect(dialog, "response", G_CALLBACK(on_open_with_dialog_response), casper);
    gtk_window_present(GTK_WINDOW(dialog));
    
    g_list_free_full(apps, g_object_unref);
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
}

static void on_open_with_dialog_response(GtkDialog *dialog, int response, FileManager *casper) {
    if (response == GTK_RESPONSE_OK) {
        GtkWidget *chooser = g_object_get_data(G_OBJECT(dialog), "chooser");
        GAppInfo *app = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(chooser));
        
        if (app && casper->selected_file) {
            GList *files = g_list_append(NULL, casper->selected_file);
            GError *error = NULL;
            
            if (!g_app_info_launch(app, files, NULL, &error)) {
                g_print("Error launching app: %s\n", error->message);
                g_error_free(error);
            }
            
            g_list_free(files);
            g_object_unref(app);
        }
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_context_menu_cut(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    // Set clipboard with file URI and cut flag
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    
    // Create clipboard content with cut flag
    char *uri = g_file_get_uri(casper->selected_file);
    char *clipboard_text = g_strdup_printf("cut\n%s", uri);
    
    gdk_clipboard_set_text(clipboard, clipboard_text);
    
    g_free(uri);
    g_free(clipboard_text);
    
    g_print("Cut: %s\n", g_file_get_basename(casper->selected_file));
    
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
}

void on_context_menu_copy(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    // Set clipboard with file URI
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    
    char *uri = g_file_get_uri(casper->selected_file);
    char *clipboard_text = g_strdup_printf("copy\n%s", uri);
    
    gdk_clipboard_set_text(clipboard, clipboard_text);
    
    g_free(uri);
    g_free(clipboard_text);
    
    g_print("Copy: %s\n", g_file_get_basename(casper->selected_file));
    
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
}

// Callback for clipboard text
static void on_clipboard_text_received(GObject *source, GAsyncResult *result, gpointer user_data) {
    FileManager *casper = (FileManager *)user_data;
    GdkClipboard *clipboard = GDK_CLIPBOARD(source);
    
    char *text = gdk_clipboard_read_text_finish(clipboard, result, NULL);
    if (!text) return;
    
    // Parse clipboard text (format: "cut\nfile://..." or "copy\nfile://...")
    char **lines = g_strsplit(text, "\n", 2);
    g_free(text);
    
    if (!lines[0] || !lines[1]) {
        g_strfreev(lines);
        return;
    }
    
    gboolean is_cut = g_strcmp0(lines[0], "cut") == 0;
    GFile *source_file = g_file_new_for_uri(lines[1]);
    g_strfreev(lines);
    
    if (!source_file) return;
    
    // Get destination path
    char *basename = g_file_get_basename(source_file);
    GFile *dest_file = g_file_get_child(casper->current_directory, basename);
    
    GError *error = NULL;
    gboolean success = FALSE;
    
    if (is_cut) {
        // Move file
        success = g_file_move(source_file, dest_file, G_FILE_COPY_NONE, 
                            NULL, NULL, NULL, &error);
        if (success) {
            g_print("Moved: %s\n", basename);
        }
    } else {
        // Copy file
        success = g_file_copy(source_file, dest_file, G_FILE_COPY_NONE, 
                            NULL, NULL, NULL, &error);
        if (success) {
            g_print("Copied: %s\n", basename);
        }
    }
    
    if (!success && error) {
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(casper->window),
                                                        GTK_DIALOG_MODAL,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_OK,
                                                        "Failed to paste: %s",
                                                        error->message);
        gtk_window_present(GTK_WINDOW(error_dialog));
        g_signal_connect(error_dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
        g_error_free(error);
    } else if (success) {
        // Refresh directory
        refresh_current_directory(casper);
        clear_selected_file(casper);
    }
    
    g_free(basename);
    g_object_unref(dest_file);
    g_object_unref(source_file);
}


void on_context_menu_paste(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    
    // Read text from clipboard asynchronously
    gdk_clipboard_read_text_async(clipboard, NULL, 
                                  (GAsyncReadyCallback)on_clipboard_text_received, 
                                  casper);
    
    if (casper->context_menu) {
        gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
    }
}


void on_context_menu_move_to(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    // Create file chooser dialog for destination
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Move To",
                                                    GTK_WINDOW(casper->window),
                                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Move", GTK_RESPONSE_OK,
                                                    NULL);
    
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_current_folder(chooser, casper->current_directory, NULL);
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_move_to_dialog_response), casper);
    gtk_window_present(GTK_WINDOW(dialog));
    
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
}

static void on_move_to_dialog_response(GtkDialog *dialog, int response, FileManager *casper) {
    if (response == GTK_RESPONSE_OK) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        GFile *dest_dir = gtk_file_chooser_get_file(chooser);
        
        if (dest_dir && casper->selected_file) {
            char *basename = g_file_get_basename(casper->selected_file);
            GFile *dest_file = g_file_get_child(dest_dir, basename);
            
            GError *error = NULL;
            if (!g_file_move(casper->selected_file, dest_file, G_FILE_COPY_NONE, 
                           NULL, NULL, NULL, &error)) {
                GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(casper->window),
                                                                GTK_DIALOG_MODAL,
                                                                GTK_MESSAGE_ERROR,
                                                                GTK_BUTTONS_OK,
                                                                "Failed to move file: %s",
                                                                error ? error->message : "Unknown error");
                gtk_window_present(GTK_WINDOW(error_dialog));
                g_signal_connect(error_dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
                if (error) g_error_free(error);
            } else {
                // Refresh the view
                refresh_current_directory(casper);
                clear_selected_file(casper);
            }
            
            g_free(basename);
            g_object_unref(dest_file);
        }
        
        if (dest_dir) {
            g_object_unref(dest_dir);
        }
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_context_menu_delete(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    (void)action;
    (void)parameter;
    
    if (!casper->selected_file) return;
    
    // Create a simple message dialog for confirmation
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(casper->window),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_YES_NO,
                                             "Are you sure you want to move '%s' to trash?",
                                             g_file_get_basename(casper->selected_file));
    
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Delete");
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_delete_dialog_response), casper);
    gtk_window_present(GTK_WINDOW(dialog));
    
    gtk_popover_popdown(GTK_POPOVER(casper->context_menu));
}

// Delete confirmation dialog response
static void on_delete_dialog_response(GtkDialog *dialog, int response, FileManager *casper) {
    if (response == GTK_RESPONSE_YES && casper->selected_file) {
        GError *error = NULL;
        
        // Keep a reference to the file before trashing
        GFile *file_to_delete = g_object_ref(casper->selected_file);
        
        // Clear selected_file to prevent dangling reference
        g_object_unref(casper->selected_file);
        casper->selected_file = NULL;
        
        // Move to trash instead of permanent delete
        if (!g_file_trash(file_to_delete, NULL, &error)) {
            g_print("Error deleting file: %s\n", error->message);
            
            // Show error dialog
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(casper->window),
                                                           GTK_DIALOG_MODAL,
                                                           GTK_MESSAGE_ERROR,
                                                           GTK_BUTTONS_OK,
                                                           "Failed to delete file: %s",
                                                           error->message);
            gtk_window_present(GTK_WINDOW(error_dialog));
            g_signal_connect(error_dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
            g_error_free(error);
        } else {
            // Refresh the view after successful deletion
            refresh_current_directory(casper);
        }
        
        g_object_unref(file_to_delete);
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}
