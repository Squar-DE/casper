#include "filemanager.h"

// Application actions
void on_refresh_action(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    refresh_current_directory(casper);
}

void on_open_action(GSimpleAction *action, GVariant *parameter, FileManager *casper) {
    guint position;
    
    if (casper->is_grid_view) {
        position = gtk_single_selection_get_selected(casper->grid_selection_model);
        if (position != GTK_INVALID_LIST_POSITION) {
            on_item_activated_grid(casper->grid_view, position, casper);
        }
    } else {
        position = gtk_single_selection_get_selected(casper->list_selection_model);
        if (position != GTK_INVALID_LIST_POSITION) {
            on_item_activated_list(casper->list_view, position, casper);
        }
    }
}
