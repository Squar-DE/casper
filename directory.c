#include "filemanager.h"

// Load directory contents
void load_directory(FileManager *casper, GFile *directory) {
    if (casper->current_directory) {
        g_object_unref(casper->current_directory);
    }
    casper->current_directory = g_object_ref(directory);
    
    // Update path entry
    char *path = g_file_get_path(directory);
    if (path) {
        gtk_editable_set_text(GTK_EDITABLE(casper->path_entry), path);
        g_free(path);
    }
    
    // Clear both string lists
    guint list_items = g_list_model_get_n_items(G_LIST_MODEL(casper->list_string_list));
    guint grid_items = g_list_model_get_n_items(G_LIST_MODEL(casper->grid_string_list));
    
    gtk_string_list_splice(casper->list_string_list, 0, list_items, NULL);
    gtk_string_list_splice(casper->grid_string_list, 0, grid_items, NULL);
    
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
    
    // Add to BOTH string lists
    for (GList *l = file_list; l != NULL; l = l->next) {
        gtk_string_list_append(casper->list_string_list, (char*)l->data);
        gtk_string_list_append(casper->grid_string_list, (char*)l->data);
    }
    
    g_list_free_full(file_list, g_free);
}

void refresh_current_directory(FileManager *fm) {
    // Clear selected file since we're reloading
    if (fm->selected_file) {
        g_object_unref(fm->selected_file);
        fm->selected_file = NULL;
    }
    
    if (fm->current_directory) {
        load_directory(fm, fm->current_directory);
    }
}
