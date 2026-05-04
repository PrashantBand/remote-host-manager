/*
 * File: session_tab.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Implements each remote host session tab with integrated SSH terminal,
 * SFTP file browser, navigation, file operations, and lifecycle cleanup.
 *
 * License: Apache License Version 2.0
 */
 
#include "session_tab.h"
#include "app.h"
#include "util.h"
#include <signal.h>

enum {
    COL_ICON = 0,
    COL_NAME,
    COL_SIZE,
    COL_TYPE,
    COL_URI,
    COL_IS_DIR,
    NUM_COLS
};

typedef struct {
    SessionTab *tab;
    gboolean remember_current;
} SessionLoadContext;

static void session_load_directory(SessionTab *tab, GFile *dir, gboolean remember_current);
static void session_mount_and_load(SessionTab *tab, GFile *dir, gboolean remember_current);
static void session_set_status(SessionTab *tab, SessionStatus status);
static void session_open_terminal(SessionTab *tab);
static void session_update_nav_buttons(SessionTab *tab);
static void session_clear_back_stack(SessionTab *tab);

static SessionTab *session_tab_ref(SessionTab *tab);
static void session_tab_unref(SessionTab *tab);
static void session_tab_cleanup_resources(SessionTab *tab, gboolean update_ui);


static gchar *session_build_sftp_uri(const HostConfig *host) {
    return g_strdup_printf("sftp://%s@%s:%d/",
                           host->user, host->host, host->port);
}

static gchar *session_build_ssh_target(const HostConfig *host) {
    return g_strdup_printf("%s@%s", host->user, host->host);
}

//added
static GtkWidget *session_create_icon_button(const gchar *icon_name, const gchar *tooltip) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);

    gtk_button_set_image(GTK_BUTTON(button), image);
    gtk_button_set_always_show_image(GTK_BUTTON(button), TRUE);
    gtk_widget_set_tooltip_text(button, tooltip);

    gtk_widget_set_size_request(button, 34, 30);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NORMAL);

    return button;
}


static void session_set_status(SessionTab *tab, SessionStatus status) {
    if (!tab) return;

    tab->status = status;

    if (tab->destroying) return;
    if (!tab->status_label || !GTK_IS_LABEL(tab->status_label)) return;

    const gchar *markup = NULL;
    switch (status) {
        case SESSION_STATUS_CONNECTING:
            markup = "<span foreground='orange'><b>● Connecting...</b></span>";
            break;
        case SESSION_STATUS_CONNECTED:
            markup = "<span foreground='green'><b>● Connected</b></span>";
            break;
        case SESSION_STATUS_MOUNT_FAILED:
            markup = "<span foreground='red'><b>● Mount failed</b></span>";
            break;
        case SESSION_STATUS_DISCONNECTED:
        default:
            markup = "<span foreground='gray'><b>● Disconnected</b></span>";
            break;
    }

    gtk_label_set_markup(GTK_LABEL(tab->status_label), markup);
}

static void session_set_path_entry(SessionTab *tab, GFile *dir) {
    if (!tab || tab->destroying) return;
    if (!dir || !G_IS_FILE(dir)) return;
    if (!tab->path_entry || !GTK_IS_ENTRY(tab->path_entry)) return;

    gchar *uri = g_file_get_uri(dir);
    gtk_entry_set_text(GTK_ENTRY(tab->path_entry), uri ? uri : "");
    g_free(uri);
}

static void session_update_nav_buttons(SessionTab *tab) {
    if (!tab || tab->destroying) return;

    if (tab->back_btn && GTK_IS_WIDGET(tab->back_btn)) {
        gboolean can_back = tab->back_stack && !g_queue_is_empty(tab->back_stack);
        gtk_widget_set_sensitive(tab->back_btn, can_back);
    }

    if (tab->up_btn && GTK_IS_WIDGET(tab->up_btn)) {
        gboolean can_up = FALSE;

        if (tab->current_dir && G_IS_FILE(tab->current_dir)) {
            GFile *parent = g_file_get_parent(tab->current_dir);
            can_up = parent != NULL;
            if (parent) g_object_unref(parent);
        }

        gtk_widget_set_sensitive(tab->up_btn, can_up);
    }
}

static void session_clear_back_stack(SessionTab *tab) {
    if (!tab || !tab->back_stack) return;

    while (!g_queue_is_empty(tab->back_stack)) {
        GFile *file = g_queue_pop_head(tab->back_stack);
        if (file) g_object_unref(file);
    }
}

static SessionTab *session_tab_ref(SessionTab *tab) {
    if (!tab) return NULL;
    tab->ref_count++;
    return tab;
}

static void session_tab_unref(SessionTab *tab) {
    if (!tab) return;

    tab->ref_count--;

    if (tab->ref_count > 0) {
        return;
    }

    if (tab->back_stack) {
        session_clear_back_stack(tab);
        g_queue_free(tab->back_stack);
        tab->back_stack = NULL;
    }

    if (tab->current_dir) {
        g_object_unref(tab->current_dir);
        tab->current_dir = NULL;
    }

    host_config_free(tab->host);
    g_free(tab);
}

static void on_unmount_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)user_data;
    GError *error = NULL;
    g_mount_unmount_with_operation_finish(G_MOUNT(source_object), res, &error);
    if (error) {
        g_clear_error(&error);
    }
}

static void session_tab_cleanup_resources(SessionTab *tab, gboolean update_ui) {
    if (!tab) return;

    if (tab->terminal_pid > 0) {
        kill(tab->terminal_pid, SIGHUP);
        tab->terminal_pid = 0;
    }

    if (tab->current_dir && G_IS_FILE(tab->current_dir)) {
        GError *error = NULL;
        GMount *mount = g_file_find_enclosing_mount(tab->current_dir, NULL, &error);

        if (mount) {
            g_mount_unmount_with_operation(
                mount,
                G_MOUNT_UNMOUNT_NONE,
                NULL,
                NULL,
                on_unmount_done,
                tab
            );

            g_object_unref(mount);
        }

        if (error) {
            g_clear_error(&error);
        }

        g_object_unref(tab->current_dir);
        tab->current_dir = NULL;
    }

    session_clear_back_stack(tab);

    if (update_ui && !tab->destroying) {
        if (tab->store) {
            gtk_list_store_clear(tab->store);
        }

        if (tab->path_entry && GTK_IS_ENTRY(tab->path_entry)) {
            gtk_entry_set_text(GTK_ENTRY(tab->path_entry), "");
        }

        if (tab->status_label && GTK_IS_LABEL(tab->status_label)) {
            session_set_status(tab, SESSION_STATUS_DISCONNECTED);
        }

        session_update_nav_buttons(tab);
    }
}

static void session_remember_current_dir(SessionTab *tab, GFile *next_dir) {
    if (!tab || !tab->back_stack) return;
    if (!tab->current_dir || !G_IS_FILE(tab->current_dir)) return;

    if (next_dir && G_IS_FILE(next_dir) && g_file_equal(tab->current_dir, next_dir)) {
        return;
    }

    g_queue_push_tail(tab->back_stack, g_object_ref(tab->current_dir));

    while (g_queue_get_length(tab->back_stack) > 100) {
        GFile *old = g_queue_pop_head(tab->back_stack);
        if (old) g_object_unref(old);
    }
}

static void on_terminal_spawned(VteTerminal *terminal,
                                GPid pid,
                                GError *error,
                                gpointer user_data) {
    (void)terminal;
    SessionTab *tab = user_data;

    if (!tab) return;

    if (tab->destroying) {
        session_tab_unref(tab);
        return;
    }

    if (error) {
        session_set_status(tab, SESSION_STATUS_DISCONNECTED);

        if (tab->app && tab->app->window) {
            util_show_error(GTK_WINDOW(tab->app->window), error->message);
        }

        session_tab_unref(tab);
        return;
    }

    tab->terminal_pid = pid;
    session_tab_unref(tab);
}

static void session_open_terminal(SessionTab *tab) {
     if (!tab || tab->destroying) return;

    gchar *target = session_build_ssh_target(tab->host);

    char **argv = g_new0(char *, 7);
    argv[0] = g_strdup("ssh");
    argv[1] = g_strdup("-p");
    argv[2] = g_strdup_printf("%d", tab->host->port);
    argv[3] = g_strdup("-o");
    argv[4] = g_strdup("ServerAliveInterval=30");
    argv[5] = target;
    argv[6] = NULL;

    vte_terminal_spawn_async(
        tab->terminal,
        VTE_PTY_DEFAULT,
        NULL,
        argv,
        NULL,
        G_SPAWN_DEFAULT,
        NULL,
        NULL,
        NULL,
        -1,
        NULL,
        (VteTerminalSpawnAsyncCallback)on_terminal_spawned,
        session_tab_ref(tab)
    );

    g_strfreev(argv);
}

static void on_mount_done(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SessionLoadContext *ctx = user_data;
    SessionTab *tab = ctx->tab;
    GFile *dir = G_FILE(source_object);
    GError *error = NULL;

    if (tab->destroying) {
        g_free(ctx);
        session_tab_unref(tab);
        return;
    }

    if (!g_file_mount_enclosing_volume_finish(dir, res, &error)) {
        if (error) {
            if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)) {
                if (!tab->destroying) {
                    session_set_status(tab, SESSION_STATUS_MOUNT_FAILED);
                    util_show_error(GTK_WINDOW(tab->app->window), error->message);
                }

                g_clear_error(&error);
                g_free(ctx);
                session_tab_unref(tab);
                return;
            }

            g_clear_error(&error);
        }
    }

    if (!tab->destroying) {
        session_load_directory(tab, dir, ctx->remember_current);
    }

    g_free(ctx);
    session_tab_unref(tab);
}

static void session_mount_and_load(SessionTab *tab, GFile *dir, gboolean remember_current) {
    if (!dir || !G_IS_FILE(dir)) {
        session_set_status(tab, SESSION_STATUS_MOUNT_FAILED);
        util_show_error(GTK_WINDOW(tab->app->window), "Invalid remote path.");
        session_update_nav_buttons(tab);
        return;
    }

    session_set_status(tab, SESSION_STATUS_CONNECTING);

    GMountOperation *mount_op =
        G_MOUNT_OPERATION(gtk_mount_operation_new(GTK_WINDOW(tab->app->window)));

    g_mount_operation_set_username(mount_op, tab->host->user);

    SessionLoadContext *ctx = g_new0(SessionLoadContext, 1);
    ctx->tab = session_tab_ref(tab);
    ctx->remember_current = remember_current;

    g_file_mount_enclosing_volume(
        dir,
        G_MOUNT_MOUNT_NONE,
        mount_op,
        NULL,
        on_mount_done,
        ctx
    );

    g_object_unref(mount_op);
}

static void session_load_directory(SessionTab *tab, GFile *dir, gboolean remember_current) {
    GError *error = NULL;
    GFileEnumerator *enumerator = NULL;
    GFileInfo *info = NULL;

    if (!dir || !G_IS_FILE(dir)) {
        session_set_status(tab, SESSION_STATUS_MOUNT_FAILED);
        util_show_error(GTK_WINDOW(tab->app->window), "Invalid directory object.");
        session_update_nav_buttons(tab);
        return;
    }

    GFile *target_dir = g_object_ref(dir);

    enumerator = g_file_enumerate_children(
        target_dir,
        "standard::name,standard::type,standard::size,standard::icon",
        G_FILE_QUERY_INFO_NONE,
        NULL,
        &error
    );

    if (!enumerator) {
        session_set_status(tab, SESSION_STATUS_MOUNT_FAILED);
        if (error) {
            util_show_error(GTK_WINDOW(tab->app->window), error->message);
            g_clear_error(&error);
        }

        g_object_unref(target_dir);
        session_update_nav_buttons(tab);
        return;
    }

    if (remember_current) {
        session_remember_current_dir(tab, target_dir);
    }

    if (tab->current_dir) g_object_unref(tab->current_dir);
    tab->current_dir = target_dir;

    gtk_list_store_clear(tab->store);
    session_set_path_entry(tab, tab->current_dir);

    while ((info = g_file_enumerator_next_file(enumerator, NULL, &error)) != NULL) {
        GtkTreeIter iter;
        const gchar *name = g_file_info_get_name(info);
        gboolean is_dir = (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);

        GIcon *icon = g_file_info_get_icon(info);
        if (icon) g_object_ref(icon);

        GFile *child = g_file_get_child(tab->current_dir, name);
        gchar *uri = g_file_get_uri(child);

        gchar *size_str = is_dir ? g_strdup("-")
                                 : util_human_readable_size(g_file_info_get_size(info));
        gchar *type_str = g_strdup(is_dir ? "Folder" : "File");

        gtk_list_store_append(tab->store, &iter);
        gtk_list_store_set(tab->store, &iter,
                           COL_ICON, icon,
                           COL_NAME, name,
                           COL_SIZE, size_str,
                           COL_TYPE, type_str,
                           COL_URI, uri,
                           COL_IS_DIR, is_dir,
                           -1);

        if (icon) g_object_unref(icon);
        g_object_unref(child);
        g_free(uri);
        g_free(size_str);
        g_free(type_str);
        g_object_unref(info);
    }

    if (error) {
        session_set_status(tab, SESSION_STATUS_MOUNT_FAILED);
        util_show_error(GTK_WINDOW(tab->app->window), error->message);
        g_clear_error(&error);
    } else {
        session_set_status(tab, SESSION_STATUS_CONNECTED);
    }

    g_object_unref(enumerator);
    session_update_nav_buttons(tab);
}

static gboolean session_get_selected(SessionTab *tab, GtkTreeIter *iter_out, gchar **uri_out, gboolean *is_dir_out) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->treeview));
    GtkTreeModel *model = GTK_TREE_MODEL(tab->store);
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter))
        return FALSE;

    gchar *uri = NULL;
    gboolean is_dir = FALSE;
    gtk_tree_model_get(model, &iter,
                       COL_URI, &uri,
                       COL_IS_DIR, &is_dir,
                       -1);

    if (iter_out) *iter_out = iter;
    if (uri_out) *uri_out = uri; else g_free(uri);
    if (is_dir_out) *is_dir_out = is_dir;

    return TRUE;
}

static void session_send_command(SessionTab *tab, const gchar *cmd) {
    vte_terminal_feed_child(tab->terminal, cmd, -1);
    vte_terminal_feed_child(tab->terminal, "\n", 1);
}

static const gchar *session_get_selected_preset_command(SessionTab *tab) {
    gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(tab->preset_combo));
    if (idx < 0 || !tab->app->presets || idx >= (gint)tab->app->presets->len) return NULL;

    PresetCommand *p = g_ptr_array_index(tab->app->presets, idx);
    return p ? p->command : NULL;
}

static void on_run_preset_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;
    const gchar *cmd = session_get_selected_preset_command(tab);
    if (cmd) session_send_command(tab, cmd);
}

static void on_back_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    if (!tab->back_stack || g_queue_is_empty(tab->back_stack)) {
        session_update_nav_buttons(tab);
        return;
    }

    GFile *previous_dir = g_queue_pop_tail(tab->back_stack);
    if (!previous_dir) {
        session_update_nav_buttons(tab);
        return;
    }

    session_mount_and_load(tab, previous_dir, FALSE);
    g_object_unref(previous_dir);

    session_update_nav_buttons(tab);
}

static void on_up_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    if (!tab->current_dir || !G_IS_FILE(tab->current_dir)) {
        session_update_nav_buttons(tab);
        return;
    }

    GFile *parent = g_file_get_parent(tab->current_dir);
    if (!parent) {
        session_update_nav_buttons(tab);
        return;
    }

    session_mount_and_load(tab, parent, TRUE);
    g_object_unref(parent);
}

static void on_reconnect_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    session_tab_disconnect(tab);

    gchar *root_uri = session_build_sftp_uri(tab->host);
    GFile *root_dir = g_file_new_for_uri(root_uri);

    session_open_terminal(tab);
    session_mount_and_load(tab, root_dir, FALSE);

    g_object_unref(root_dir);
    g_free(root_uri);
}

static void on_refresh_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    if (!tab->current_dir) return;

    GFile *dir = g_object_ref(tab->current_dir);
    session_mount_and_load(tab, dir, FALSE);
    g_object_unref(dir);
}

static void on_path_activate(GtkEntry *entry, gpointer user_data) {
    SessionTab *tab = user_data;
    const gchar *uri = gtk_entry_get_text(entry);

    if (!uri || !*uri) return;

    GFile *dir = g_file_new_for_uri(uri);
    session_mount_and_load(tab, dir, TRUE);
    g_object_unref(dir);
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                             GtkTreeViewColumn *column, gpointer user_data) {
    (void)tree_view;
    (void)column;
    SessionTab *tab = user_data;

    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL(tab->store);

    if (!gtk_tree_model_get_iter(model, &iter, path))
        return;

    gboolean is_dir = FALSE;
    gchar *uri = NULL;
    gtk_tree_model_get(model, &iter,
                       COL_URI, &uri,
                       COL_IS_DIR, &is_dir,
                       -1);

    if (uri && *uri) {
        GFile *file = g_file_new_for_uri(uri);
        if (is_dir) session_mount_and_load(tab, file, TRUE);
        g_object_unref(file);
    }

    g_free(uri);
}

static GtkWidget *session_create_treeview(SessionTab *tab) {
    GtkWidget *tree;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    tab->store = gtk_list_store_new(
        NUM_COLS,
        G_TYPE_ICON,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN
    );

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tab->store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);

    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("", renderer, "gicon", COL_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Size", renderer, "text", COL_SIZE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", COL_TYPE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    g_signal_connect(tree, "row-activated", G_CALLBACK(on_row_activated), tab);
    return tree;
}

static void on_new_folder_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    if (!tab->current_dir || !G_IS_FILE(tab->current_dir)) {
        util_show_error(GTK_WINDOW(tab->app->window), "No active remote directory.");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Create Folder",
        GTK_WINDOW(tab->app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Create", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Folder name");
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (name && *name) {
            GError *error = NULL;
            GFile *base = g_object_ref(tab->current_dir);
            GFile *child = g_file_get_child(base, name);

            util_make_directory(child, GTK_WINDOW(tab->app->window), &error);
            if (error) g_clear_error(&error);

            g_object_unref(child);
            g_object_unref(base);

            on_refresh_clicked(NULL, tab);
        }
    }

    gtk_widget_destroy(dialog);
}

static void on_delete_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    gchar *uri = NULL;
    if (!session_get_selected(tab, NULL, &uri, NULL)) return;

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(tab->app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_OK_CANCEL,
        "Delete selected item?"
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        GError *error = NULL;
        GFile *file = g_file_new_for_uri(uri);

        util_delete_gfile(file, GTK_WINDOW(tab->app->window), &error);
        if (error) g_clear_error(&error);

        g_object_unref(file);
        on_refresh_clicked(NULL, tab);
    }

    gtk_widget_destroy(dialog);
    g_free(uri);
}

static void on_rename_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    gchar *uri = NULL;
    if (!session_get_selected(tab, NULL, &uri, NULL)) return;

    GFile *file = g_file_new_for_uri(uri);
    gchar *old_name = g_file_get_basename(file);

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Rename",
        GTK_WINDOW(tab->app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Rename", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), old_name ? old_name : "");
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (new_name && *new_name) {
            GFile *parent = g_file_get_parent(file);
            if (parent) {
                GError *error = NULL;
                GFile *dest = g_file_get_child(parent, new_name);

                util_move_gfile(file, dest, GTK_WINDOW(tab->app->window), &error);
                if (error) g_clear_error(&error);

                g_object_unref(dest);
                g_object_unref(parent);

                on_refresh_clicked(NULL, tab);
            }
        }
    }

    gtk_widget_destroy(dialog);
    g_free(old_name);
    g_object_unref(file);
    g_free(uri);
}

static void on_upload_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    if (!tab->current_dir || !G_IS_FILE(tab->current_dir)) {
        util_show_error(GTK_WINDOW(tab->app->window), "No active remote directory.");
        return;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Upload Local File",
        GTK_WINDOW(tab->app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Upload", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *local_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (local_path) {
            GFile *src = g_file_new_for_path(local_path);
            gchar *basename = g_file_get_basename(src);

            GFile *base = g_object_ref(tab->current_dir);
            GFile *dest = g_file_get_child(base, basename);

            GError *error = NULL;
            util_copy_gfile(src, dest, GTK_WINDOW(tab->app->window), &error);
            if (error) g_clear_error(&error);

            g_free(basename);
            g_object_unref(dest);
            g_object_unref(base);
            g_object_unref(src);
            g_free(local_path);

            on_refresh_clicked(NULL, tab);
        }
    }

    gtk_widget_destroy(dialog);
}

static void on_download_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    SessionTab *tab = user_data;

    gchar *uri = NULL;
    gboolean is_dir = FALSE;
    if (!session_get_selected(tab, NULL, &uri, &is_dir)) return;

    if (is_dir) {
        util_show_info(GTK_WINDOW(tab->app->window), "Folder download is not implemented in this first version.");
        g_free(uri);
        return;
    }

    GFile *src = g_file_new_for_uri(uri);
    gchar *basename = g_file_get_basename(src);

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Download Remote File",
        GTK_WINDOW(tab->app->window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Download", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *local_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (local_dir) {
            GFile *dest_dir = g_file_new_for_path(local_dir);
            GFile *dest = g_file_get_child(dest_dir, basename);

            GError *error = NULL;
            util_copy_gfile(src, dest, GTK_WINDOW(tab->app->window), &error);
            if (error) g_clear_error(&error);

            g_object_unref(dest);
            g_object_unref(dest_dir);
            g_free(local_dir);
        }
    }

    gtk_widget_destroy(dialog);
    g_free(basename);
    g_object_unref(src);
    g_free(uri);
}

static void on_terminal_child_exited(VteTerminal *terminal, gint status, gpointer user_data) {
    (void)terminal;
    (void)status;

    SessionTab *tab = user_data;
    if (!tab) return;

    tab->terminal_pid = 0;

    if (tab->destroying) {
        return;
    }

    session_set_status(tab, SESSION_STATUS_DISCONNECTED);
}


void session_tab_disconnect(SessionTab *tab) {
    if (!tab) return;
    session_tab_cleanup_resources(tab, TRUE);
}

static void on_tab_close_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *page = GTK_WIDGET(user_data);
    GtkWidget *notebook = gtk_widget_get_parent(page);
    if (!GTK_IS_NOTEBOOK(notebook)) return;

    SessionTab *tab = g_object_get_data(G_OBJECT(page), "session-tab");
    if (tab) {
        session_tab_disconnect(tab);
    }

    gint page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
    if (page_num >= 0) gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), page_num);
}

SessionTab *session_tab_new(App *app, const HostConfig *host) {
    SessionTab *tab = g_new0(SessionTab, 1);
    tab->app = app;
    tab->host = host_config_clone(host);
    tab->status = SESSION_STATUS_DISCONNECTED;
    tab->terminal_pid = 0;
    tab->back_stack = g_queue_new();
    tab->ref_count = 1;
    tab->destroying = FALSE;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    tab->root = root;

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(root), toolbar, FALSE, FALSE, 0);

    tab->back_btn = session_create_icon_button("go-previous-symbolic", "Back");
    tab->up_btn = session_create_icon_button("go-up-symbolic", "Up");

    tab->refresh_btn = session_create_icon_button("view-refresh-symbolic", "Refresh");
    tab->reconnect_btn = session_create_icon_button("network-transmit-receive-symbolic", "Reconnect");

    tab->new_folder_btn = session_create_icon_button("folder-new-symbolic", "New Folder");
    tab->rename_btn = session_create_icon_button("document-edit-symbolic", "Rename");
    tab->delete_btn = session_create_icon_button("user-trash-symbolic", "Delete");

    tab->upload_btn = session_create_icon_button("document-send-symbolic", "Upload");
    tab->download_btn = session_create_icon_button("document-save-symbolic", "Download");

    tab->preset_combo = gtk_combo_box_text_new();
    for (guint i = 0; i < app->presets->len; i++) {
        PresetCommand *p = g_ptr_array_index(app->presets, i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tab->preset_combo), p->label);
    }
    if (app->presets->len > 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(tab->preset_combo), 0);
    }

    // tab->run_preset_btn = gtk_button_new_with_label("Run");
    tab->run_preset_btn = session_create_icon_button("system-run-symbolic", "Run Selected Command");
    tab->status_label = gtk_label_new(NULL);
    tab->path_entry = gtk_entry_new();

    session_set_status(tab, SESSION_STATUS_DISCONNECTED);

    gtk_box_pack_start(GTK_BOX(toolbar), tab->back_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->up_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->refresh_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->reconnect_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->new_folder_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->rename_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->delete_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->upload_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->download_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->preset_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->run_preset_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->status_label, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(toolbar), tab->path_entry, TRUE, TRUE, 0);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(root), paned, TRUE, TRUE, 0);

    GtkWidget *left_scrolled = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *right_scrolled = gtk_scrolled_window_new(NULL, NULL);

    tab->treeview = session_create_treeview(tab);
    gtk_container_add(GTK_CONTAINER(left_scrolled), tab->treeview);

    tab->terminal = VTE_TERMINAL(vte_terminal_new());
    vte_terminal_set_scrollback_lines(tab->terminal, 10000);
    gtk_container_add(GTK_CONTAINER(right_scrolled), GTK_WIDGET(tab->terminal));

    gtk_paned_pack1(GTK_PANED(paned), left_scrolled, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), right_scrolled, TRUE, FALSE);

    g_signal_connect(tab->back_btn, "clicked", G_CALLBACK(on_back_clicked), tab);
    g_signal_connect(tab->up_btn, "clicked", G_CALLBACK(on_up_clicked), tab);
    g_signal_connect(tab->refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), tab);
    g_signal_connect(tab->reconnect_btn, "clicked", G_CALLBACK(on_reconnect_clicked), tab);
    g_signal_connect(tab->new_folder_btn, "clicked", G_CALLBACK(on_new_folder_clicked), tab);
    g_signal_connect(tab->rename_btn, "clicked", G_CALLBACK(on_rename_clicked), tab);
    g_signal_connect(tab->delete_btn, "clicked", G_CALLBACK(on_delete_clicked), tab);
    g_signal_connect(tab->upload_btn, "clicked", G_CALLBACK(on_upload_clicked), tab);
    g_signal_connect(tab->download_btn, "clicked", G_CALLBACK(on_download_clicked), tab);
    g_signal_connect(tab->run_preset_btn, "clicked", G_CALLBACK(on_run_preset_clicked), tab);
    g_signal_connect(tab->path_entry, "activate", G_CALLBACK(on_path_activate), tab);
    g_signal_connect(tab->terminal, "child-exited", G_CALLBACK(on_terminal_child_exited), tab);

    gchar *root_uri = session_build_sftp_uri(tab->host);
    GFile *root_dir = g_file_new_for_uri(root_uri);

    session_open_terminal(tab);
    session_mount_and_load(tab, root_dir, FALSE);

    g_object_unref(root_dir);
    g_free(root_uri);

    session_update_nav_buttons(tab);

    gtk_widget_show_all(root);
    return tab;
}

GtkWidget *session_tab_get_widget(SessionTab *tab) {
    return tab->root;
}

GtkWidget *session_tab_create_tab_label(SessionTab *tab, GtkWidget *page, GtkWidget *notebook) {
    (void)notebook;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *label = gtk_label_new(tab->host->label);
    GtkWidget *close_btn = gtk_button_new_with_label("×");

    gtk_widget_set_can_focus(close_btn, FALSE);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);

    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), close_btn, FALSE, FALSE, 0);

    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_tab_close_clicked), page);

    gtk_widget_show_all(box);
    return box;
}

void session_tab_refresh(SessionTab *tab) {
    if (!tab->current_dir) return;

    GFile *dir = g_object_ref(tab->current_dir);
    session_mount_and_load(tab, dir, FALSE);
    g_object_unref(dir);
}

const gchar *session_tab_get_host_id(SessionTab *tab) {
    return (tab && tab->host) ? tab->host->id : NULL;
}

void session_tab_free(SessionTab *tab) {
    if (!tab) return;

    tab->destroying = TRUE;

    /*
     * Do not call session_tab_disconnect() here.
     * At this stage GTK may already be destroying child widgets.
     * Updating labels, entries, buttons, or list stores here can cause:
     * GTK_IS_LABEL / GTK_IS_ENTRY / GTK_IS_WIDGET critical warnings.
     */
    session_tab_cleanup_resources(tab, FALSE);

    session_tab_unref(tab);
}