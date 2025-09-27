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

// Create context menu
void create_context_menu(FileManager *casper) {
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Open", "app.open");
    g_menu_append(menu, "Refresh", "app.refresh");
    
    casper->context_menu = GTK_POPOVER(gtk_popover_menu_new_from_model(G_MENU_MODEL(menu)));
    gtk_widget_set_parent(GTK_WIDGET(casper->context_menu), GTK_WIDGET(casper->list_view));
}
