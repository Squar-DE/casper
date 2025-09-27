#ifndef FILEMANAGER_H
#define FILEMANAGER_H

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
    GtkStringList *list_string_list;
    GtkStringList *grid_string_list;
    GtkSingleSelection *list_selection_model;
    GtkSingleSelection *grid_selection_model;
    
    GFile *current_directory;
    GQueue *back_history;
    GQueue *forward_history;
    
    GtkPopover *context_menu;
    GtkWidget *selected_item;
    GtkStack *view_stack;
    gboolean is_grid_view;
} FileManager;

// Function declarations from utils.c
char* format_file_size(goffset size);
char* format_file_time(GTimeVal *time);
const char* get_file_icon(GFile *file, GFileInfo *info);

// Function declarations from navigation.c
void on_back_clicked(GtkButton *button, FileManager *fm);
void on_forward_clicked(GtkButton *button, FileManager *fm);
void on_up_clicked(GtkButton *button, FileManager *fm);
void on_path_activated(GtkEntry *entry, FileManager *fm);
void update_navigation_buttons(FileManager *fm);

// Function declarations from directory.c
void load_directory(FileManager *fm, GFile *directory);
void refresh_current_directory(FileManager *fm);

// Function declarations from views.c
void on_view_toggle_clicked(GtkButton *button, FileManager *fm);
void on_item_activated_list(GtkListView *list_view, guint position, FileManager *fm);
void on_item_activated_grid(GtkGridView *grid_view, guint position, FileManager *fm);
void setup_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm);
void bind_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm);
void setup_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm);
void bind_grid_factory(GtkListItemFactory *factory, GtkListItem *list_item, FileManager *fm);

// Function declarations from actions.c
void on_refresh_action(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_open_action(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void create_context_menu(FileManager *fm);

#endif // FILEMANAGER_H
