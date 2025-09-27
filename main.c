#include "filemanager.h"

// Forward declaration
static void on_window_destroy(GtkWidget *widget, FileManager *casper);

// Window close handler
static void on_window_destroy(GtkWidget *widget, FileManager *casper) {
    // Clean up resources when window is being destroyed
    if (casper->current_directory) {
        g_object_unref(casper->current_directory);
        casper->current_directory = NULL;
    }
    
    if (casper->back_history) {
        g_queue_free_full(casper->back_history, g_object_unref);
        casper->back_history = NULL;
    }
    
    if (casper->forward_history) {
        g_queue_free_full(casper->forward_history, g_object_unref);
        casper->forward_history = NULL;
    }
}

// Application activate callback
static void on_activate(GtkApplication *app, FileManager *casper) {
    // Create main window
    casper->window = GTK_APPLICATION_WINDOW(adw_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(casper->window), "File Manager");
    gtk_window_set_default_size(GTK_WINDOW(casper->window), 900, 600);
    
    // Create main box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(casper->window), main_box);
    
    // Create header bar
    casper->header_bar = ADW_HEADER_BAR(adw_header_bar_new());
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(casper->header_bar));
    
    // Create navigation buttons
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nav_box, "linked");
    
    casper->back_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-previous-symbolic"));
    casper->forward_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-next-symbolic"));
    casper->up_btn = GTK_BUTTON(gtk_button_new_from_icon_name("go-up-symbolic"));
    
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(casper->back_btn));
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(casper->forward_btn));
    gtk_box_append(GTK_BOX(nav_box), GTK_WIDGET(casper->up_btn));
    
    adw_header_bar_pack_start(casper->header_bar, nav_box);
    
    // Create view toggle button
    casper->view_toggle_btn = GTK_BUTTON(gtk_button_new_from_icon_name("view-grid-symbolic"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(casper->view_toggle_btn), "Toggle Grid/List View");
    adw_header_bar_pack_end(casper->header_bar, GTK_WIDGET(casper->view_toggle_btn));
    
    // Create path entry
    casper->path_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(casper->path_entry), TRUE);
    adw_header_bar_set_title_widget(casper->header_bar, GTK_WIDGET(casper->path_entry));
    
    // Create COMPLETELY separate data models for each view
    casper->list_string_list = gtk_string_list_new(NULL);
    casper->grid_string_list = gtk_string_list_new(NULL);
    casper->list_selection_model = gtk_single_selection_new(G_LIST_MODEL(casper->list_string_list));
    casper->grid_selection_model = gtk_single_selection_new(G_LIST_MODEL(casper->grid_string_list));
    
    // Create list view
    casper->list_view = GTK_LIST_VIEW(gtk_list_view_new(GTK_SELECTION_MODEL(casper->list_selection_model), NULL));
    
    // Create grid view with completely separate data
    casper->grid_view = GTK_GRID_VIEW(gtk_grid_view_new(GTK_SELECTION_MODEL(casper->grid_selection_model), NULL));
    gtk_grid_view_set_max_columns(casper->grid_view, 8);
    gtk_grid_view_set_min_columns(casper->grid_view, 2);
    
    // Setup list factory
    GtkListItemFactory *list_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(list_factory, "setup", G_CALLBACK(setup_factory), casper);
    g_signal_connect(list_factory, "bind", G_CALLBACK(bind_factory), casper);
    gtk_list_view_set_factory(casper->list_view, list_factory);
    
    // Setup grid factory
    GtkListItemFactory *grid_factory = gtk_signal_list_item_factory_new();
    g_signal_connect(grid_factory, "setup", G_CALLBACK(setup_grid_factory), casper);
    g_signal_connect(grid_factory, "bind", G_CALLBACK(bind_grid_factory), casper);
    gtk_grid_view_set_factory(casper->grid_view, grid_factory);
    
    // Initialize with list view (is_grid_view = FALSE by default)
    casper->is_grid_view = FALSE;
    
    // Create stack for view switching
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(stack), GTK_WIDGET(casper->list_view), "list");
    gtk_stack_add_named(GTK_STACK(stack), GTK_WIDGET(casper->grid_view), "grid");

    // Create scrolled window
    casper->scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_scrolled_window_set_child(casper->scrolled_window, stack);
    gtk_widget_set_vexpand(GTK_WIDGET(casper->scrolled_window), TRUE);
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(casper->scrolled_window));

    // Store stack reference in FileManager struct for later access
    casper->view_stack = GTK_STACK(stack);    
    // Initialize history queues

    casper->back_history = g_queue_new();
    casper->forward_history = g_queue_new();
    
    // Connect signals
    g_signal_connect(casper->back_btn, "clicked", G_CALLBACK(on_back_clicked), casper);
    g_signal_connect(casper->forward_btn, "clicked", G_CALLBACK(on_forward_clicked), casper);
    g_signal_connect(casper->up_btn, "clicked", G_CALLBACK(on_up_clicked), casper);
    g_signal_connect(casper->view_toggle_btn, "clicked", G_CALLBACK(on_view_toggle_clicked), casper);
    g_signal_connect(casper->path_entry, "activate", G_CALLBACK(on_path_activated), casper);
    g_signal_connect(casper->list_view, "activate", G_CALLBACK(on_item_activated_list), casper);
    g_signal_connect(casper->grid_view, "activate", G_CALLBACK(on_item_activated_grid), casper);
    g_signal_connect(casper->window, "destroy", G_CALLBACK(on_window_destroy), casper);
    
    // Create actions
    GSimpleAction *refresh_action = g_simple_action_new("refresh", NULL);
    g_signal_connect(refresh_action, "activate", G_CALLBACK(on_refresh_action), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(refresh_action));
    
    GSimpleAction *open_action = g_simple_action_new("open", NULL);
    g_signal_connect(open_action, "activate", G_CALLBACK(on_open_action), casper);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(open_action));
    
    // Load home directory
    const char *home = g_get_home_dir();
    GFile *home_file = g_file_new_for_path(home);
    load_directory(casper, home_file);
    g_object_unref(home_file);
    
    update_navigation_buttons(casper);
    
    gtk_window_present(GTK_WINDOW(casper->window));
}

int main(int argc, char *argv[]) {
    FileManager casper = {0};
    
    // Create application
    casper.app = GTK_APPLICATION(adw_application_new("org.SquarDE.casper", 
                                                G_APPLICATION_DEFAULT_FLAGS));
    
    g_signal_connect(casper.app, "activate", G_CALLBACK(on_activate), &casper);
    
    int status = g_application_run(G_APPLICATION(casper.app), argc, argv);
    
    // Cleanup - be more careful with reference counting
    if (casper.current_directory) {
        g_object_unref(casper.current_directory);
        casper.current_directory = NULL;
    }
    
    if (casper.back_history) {
        g_queue_free_full(casper.back_history, g_object_unref);
        casper.back_history = NULL;
    }
    
    if (casper.forward_history) {
        g_queue_free_full(casper.forward_history, g_object_unref);
        casper.forward_history = NULL;
    }
    
    // The application will clean up the widgets automatically
    g_object_unref(casper.app);
    
    return status;
}
