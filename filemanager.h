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
    AdwHeaderBar *sidebar_header;
    GtkButton *back_btn;
    GtkButton *forward_btn;
    GtkButton *up_btn;
    GtkButton *view_toggle_btn;
    GtkEntry *path_entry;
    GtkScrolledWindow *scrolled_window;
    GFile *selected_file;
    GtkListView *list_view;
    GtkGridView *grid_view;
    GtkStringList *list_string_list;
    GtkStringList *grid_string_list;
    GtkSingleSelection *list_selection_model;
    GtkSingleSelection *grid_selection_model;
    
    GFile *current_directory;
    GQueue *back_history;
    GQueue *forward_history;
    
    GtkPopoverMenu *context_menu;
    GtkWidget *selected_item;
    GtkStack *view_stack;
    GtkListBox *sidebar_listbox;
    gboolean is_grid_view;
} FileManager;


// Context menu functions
void show_context_menu(FileManager *fm, GFile *file, GdkRectangle *rect);
void on_context_menu_open(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_context_menu_open_with(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_context_menu_cut(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_context_menu_copy(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_context_menu_paste(GSimpleAction *action, GVariant *parameter, FileManager *casper);
void on_context_menu_move_to(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_context_menu_delete(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void setup_context_menu_actions(GtkApplication *app);
gboolean on_right_click(GtkGestureClick *gesture, int n_press, double x, double y, FileManager *fm);


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
void setup_sidebar(FileManager *fm);
void on_sidebar_row_activated(GtkListBox *listbox, GtkListBoxRow *row, FileManager *fm);
void populate_sidebar(FileManager *fm);
void add_sidebar_item(FileManager *fm, const char *name, const char *icon, GFile *file, gboolean is_separator);

// Function declarations from actions.c
void on_refresh_action(GSimpleAction *action, GVariant *parameter, FileManager *fm);
void on_open_action(GSimpleAction *action, GVariant *parameter, FileManager *fm);

#endif // FILEMANAGER_H
