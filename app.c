/*
 * File: app.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Builds and manages the main GTK application window, menu bar, sidebar,
 * host list, and notebook-based remote host sessions.
 *
 * License: Apache License Version 2.0
 */
 
#include "app.h"
#include "host_dialog.h"
#include "session_tab.h"
#include "util.h"

#include <gdk/gdkkeysyms.h>

static void on_toggle_sidebar_activate(GtkMenuItem *item, gpointer user_data);
static void on_add_host_activate(GtkMenuItem *item, gpointer user_data);
static void on_edit_host_activate(GtkMenuItem *item, gpointer user_data);
static void on_remove_host_activate(GtkMenuItem *item, gpointer user_data);
static void on_quit_activate(GtkMenuItem *item, gpointer user_data);
static void on_about_activate(GtkMenuItem *item, gpointer user_data);

static void on_host_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
static void on_host_search_changed(GtkEditable *editable, gpointer user_data);

App *app_new(GtkApplication *gtk_app) {
    App *app = g_new0(App, 1);
    app->gtk_app = gtk_app;
    app->config_path = host_config_default_path();
    app->preset_config_path = preset_config_default_path();
    app->hosts = g_ptr_array_new_with_free_func((GDestroyNotify)host_config_free);
    app->presets = g_ptr_array_new_with_free_func((GDestroyNotify)preset_command_free);
    return app;
}

void app_load_hosts(App *app) {
    GError *error = NULL;
    GPtrArray *loaded = host_config_load_all(app->config_path, &error);

    if (!loaded) {
        if (app->window && error) util_show_error(GTK_WINDOW(app->window), error->message);
        g_clear_error(&error);
        return;
    }

    g_ptr_array_free(app->hosts, TRUE);
    app->hosts = loaded;
}

void app_load_presets(App *app) {
    GError *error = NULL;
    GPtrArray *loaded = preset_config_load_all(app->preset_config_path, &error);

    if (!loaded) {
        if (app->window && error) util_show_error(GTK_WINDOW(app->window), error->message);
        g_clear_error(&error);
        return;
    }

    g_ptr_array_free(app->presets, TRUE);
    app->presets = loaded;
}

void app_save_hosts(App *app) {
    GError *error = NULL;
    if (!host_config_save_all(app->config_path, app->hosts, &error)) {
        if (app->window && error) util_show_error(GTK_WINDOW(app->window), error->message);
        g_clear_error(&error);
    }
}

static GtkWidget *create_menu_item_with_accel(
    const gchar *label,
    GtkAccelGroup *accel_group,
    guint key,
    GdkModifierType modifiers
) {
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);

    if (key != 0 && accel_group) {
        gtk_widget_add_accelerator(
            item,
            "activate",
            accel_group,
            key,
            modifiers,
            GTK_ACCEL_VISIBLE
        );
    }

    return item;
}

static GtkWidget *create_menu_bar(App *app, GtkAccelGroup *accel_group) {
    GtkWidget *menubar = gtk_menu_bar_new();

    /*
     * Hosts menu
     */
    GtkWidget *hosts_root = gtk_menu_item_new_with_mnemonic("_Hosts");
    GtkWidget *hosts_menu = gtk_menu_new();

    app->add_host_item = create_menu_item_with_accel(
        "_Add Host",
        accel_group,
        GDK_KEY_n,
        GDK_CONTROL_MASK
    );

    app->edit_host_item = create_menu_item_with_accel(
        "_Edit Selected Host",
        accel_group,
        GDK_KEY_e,
        GDK_CONTROL_MASK
    );

    app->remove_host_item = create_menu_item_with_accel(
        "_Remove Selected Host",
        accel_group,
        GDK_KEY_Delete,
        0
    );

    GtkWidget *hosts_sep1 = gtk_separator_menu_item_new();

    app->quit_item = create_menu_item_with_accel(
        "_Quit",
        accel_group,
        GDK_KEY_q,
        GDK_CONTROL_MASK
    );

    gtk_menu_shell_append(GTK_MENU_SHELL(hosts_menu), app->add_host_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(hosts_menu), app->edit_host_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(hosts_menu), app->remove_host_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(hosts_menu), hosts_sep1);
    gtk_menu_shell_append(GTK_MENU_SHELL(hosts_menu), app->quit_item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(hosts_root), hosts_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), hosts_root);

    /*
     * View menu
     */
    GtkWidget *view_root = gtk_menu_item_new_with_mnemonic("_View");
    GtkWidget *view_menu = gtk_menu_new();

    app->toggle_sidebar_item = create_menu_item_with_accel(
        "_Toggle Host Sidebar",
        accel_group,
        GDK_KEY_b,
        GDK_CONTROL_MASK
    );

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), app->toggle_sidebar_item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_root), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_root);

    /*
     * Help menu
     */
    GtkWidget *help_root = gtk_menu_item_new_with_mnemonic("_Help");
    GtkWidget *help_menu = gtk_menu_new();

    app->about_item = create_menu_item_with_accel(
        "_About Remote Host Manager",
        accel_group,
        GDK_KEY_F1,
        0
    );

    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), app->about_item);

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_root), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_root);

    /*
     * Signals
     */
    g_signal_connect(app->add_host_item, "activate", G_CALLBACK(on_add_host_activate), app);
    g_signal_connect(app->edit_host_item, "activate", G_CALLBACK(on_edit_host_activate), app);
    g_signal_connect(app->remove_host_item, "activate", G_CALLBACK(on_remove_host_activate), app);
    g_signal_connect(app->toggle_sidebar_item, "activate", G_CALLBACK(on_toggle_sidebar_activate), app);
    g_signal_connect(app->quit_item, "activate", G_CALLBACK(on_quit_activate), app);
    g_signal_connect(app->about_item, "activate", G_CALLBACK(on_about_activate), app);

    return menubar;
}

// static GtkWidget *create_host_row(const HostConfig *host) {
//     GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

//     GtkWidget *label1 = gtk_label_new(NULL);
//     gchar *markup = g_markup_printf_escaped("<b>%s</b>", host->label ? host->label : "");
//     gtk_label_set_markup(GTK_LABEL(label1), markup);
//     gtk_label_set_xalign(GTK_LABEL(label1), 0.0f);

//     GtkWidget *label2 = gtk_label_new(NULL);
//     gchar *sub = g_strdup_printf("%s@%s:%d",
//                                  host->user ? host->user : "",
//                                  host->host ? host->host : "",
//                                  host->port);
//     gtk_label_set_text(GTK_LABEL(label2), sub);
//     gtk_label_set_xalign(GTK_LABEL(label2), 0.0f);

//     gtk_box_pack_start(GTK_BOX(row_box), label1, FALSE, FALSE, 0);
//     gtk_box_pack_start(GTK_BOX(row_box), label2, FALSE, FALSE, 0);

//     g_free(markup);
//     g_free(sub);
//     gtk_widget_show_all(row_box);
//     return row_box;
// }

static GtkWidget *create_host_row(const HostConfig *host) {
    GtkWidget *outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    gtk_widget_set_margin_top(content_box, 6);
    gtk_widget_set_margin_bottom(content_box, 6);
    gtk_widget_set_margin_start(content_box, 6);
    gtk_widget_set_margin_end(content_box, 6);

    GtkWidget *label1 = gtk_label_new(NULL);
    gchar *markup = g_markup_printf_escaped("<b>%s</b>", host->label ? host->label : "");
    gtk_label_set_markup(GTK_LABEL(label1), markup);
    gtk_label_set_xalign(GTK_LABEL(label1), 0.0f);

    GtkWidget *label2 = gtk_label_new(NULL);
    gchar *sub = g_strdup_printf("%s@%s:%d",
                                 host->user ? host->user : "",
                                 host->host ? host->host : "",
                                 host->port);
    gtk_label_set_text(GTK_LABEL(label2), sub);
    gtk_label_set_xalign(GTK_LABEL(label2), 0.0f);
    gtk_widget_set_margin_top(label2, 2);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_box_pack_start(GTK_BOX(content_box), label1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), label2, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer_box), content_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer_box), separator, FALSE, FALSE, 0);

    g_free(markup);
    g_free(sub);

    gtk_widget_show_all(outer_box);
    return outer_box;
}

static gboolean host_matches_query(const HostConfig *host, const gchar *query) {
    if (!query || !*query) return TRUE;

    gchar *q = g_ascii_strdown(query, -1);
    gchar *label = g_ascii_strdown(host->label ? host->label : "", -1);
    gchar *user = g_ascii_strdown(host->user ? host->user : "", -1);
    gchar *addr = g_ascii_strdown(host->host ? host->host : "", -1);

    gboolean match =
        (strstr(label, q) != NULL) ||
        (strstr(user, q) != NULL) ||
        (strstr(addr, q) != NULL);

    g_free(q);
    g_free(label);
    g_free(user);
    g_free(addr);

    return match;
}

void app_refresh_host_list(App *app) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(app->host_listbox));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    const gchar *query = gtk_entry_get_text(GTK_ENTRY(app->host_search_entry));

    for (guint i = 0; i < app->hosts->len; i++) {
        HostConfig *host = g_ptr_array_index(app->hosts, i);
        if (!host_matches_query(host, query)) continue;

        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *content = create_host_row(host);

        gtk_container_add(GTK_CONTAINER(row), content);
        g_object_set_data(G_OBJECT(row), "host-config", host);
        gtk_container_add(GTK_CONTAINER(app->host_listbox), row);
    }

    gtk_widget_show_all(app->host_listbox);
}

static gint app_find_existing_tab(App *app, const gchar *host_id) {
    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

    for (gint i = 0; i < n; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), i);
        SessionTab *tab = g_object_get_data(G_OBJECT(page), "session-tab");
        if (!tab) continue;

        const gchar *tab_host_id = session_tab_get_host_id(tab);
        if (tab_host_id && g_strcmp0(tab_host_id, host_id) == 0) {
            return i;
        }
    }

    return -1;
}

void app_open_host_in_tab(App *app, HostConfig *host) {
    gint existing = app_find_existing_tab(app, host->id);
    if (existing >= 0) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), existing);
        return;
    }

    SessionTab *tab = session_tab_new(app, host);
    GtkWidget *page = session_tab_get_widget(tab);
    GtkWidget *tab_label = session_tab_create_tab_label(tab, page, app->notebook);

    gint page_num = gtk_notebook_append_page(GTK_NOTEBOOK(app->notebook), page, tab_label);
    g_object_set_data_full(G_OBJECT(page), "session-tab", tab, (GDestroyNotify)session_tab_free);

    gtk_widget_show_all(page);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page_num);
}

void app_remove_selected_host(App *app) {
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->host_listbox));
    if (!row) return;

    HostConfig *selected = g_object_get_data(G_OBJECT(row), "host-config");
    if (!selected) return;

    for (guint i = 0; i < app->hosts->len; i++) {
        HostConfig *cfg = g_ptr_array_index(app->hosts, i);
        if (cfg == selected) {
            g_ptr_array_remove_index(app->hosts, i);
            app_save_hosts(app);
            app_refresh_host_list(app);
            return;
        }
    }
}

void app_edit_selected_host(App *app) {
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->host_listbox));
    if (!row) return;

    HostConfig *selected = g_object_get_data(G_OBJECT(row), "host-config");
    if (!selected) return;

    if (host_dialog_run_edit(GTK_WINDOW(app->window), selected)) {
        app_save_hosts(app);
        app_refresh_host_list(app);
    }
}

static void on_toggle_sidebar_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;

    gboolean reveal = gtk_revealer_get_reveal_child(GTK_REVEALER(app->sidebar_revealer));
    gtk_revealer_set_reveal_child(GTK_REVEALER(app->sidebar_revealer), !reveal);
}

static void on_add_host_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;

    HostConfig *cfg = host_dialog_run_new(GTK_WINDOW(app->window));
    if (!cfg) return;

    g_ptr_array_add(app->hosts, cfg);
    app_save_hosts(app);
    app_refresh_host_list(app);
}

static void on_edit_host_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;
    app_edit_selected_host(app);
}

static void on_remove_host_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;
    app_remove_selected_host(app);
}

static void on_quit_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;

    if (app->window) {
        gtk_widget_destroy(app->window);
    }
}

static void on_about_activate(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    App *app = user_data;

    const gchar *authors[] = {
        "Prashant Band",
        NULL
    };

    gtk_show_about_dialog(
        GTK_WINDOW(app->window),
        "program-name", "Remote Host Manager",
        "version", "1.0.0",
        "comments", "A GTK3 remote host management utility with saved SSH hosts, SFTP file browsing, and integrated VTE terminal sessions.",
        "authors", authors,
        "copyright", "© 2026 Prashant Band",
        "logo-icon-name", "network-server",
        "license-type", GTK_LICENSE_MIT_X11,
        NULL
    );
}

static void on_host_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    App *app = user_data;
    HostConfig *host = g_object_get_data(G_OBJECT(row), "host-config");
    if (host) app_open_host_in_tab(app, host);
}

static void on_host_search_changed(GtkEditable *editable, gpointer user_data) {
    (void)editable;
    App *app = user_data;
    app_refresh_host_list(app);
}

void app_build_ui(App *app) {
    app->window = gtk_application_window_new(app->gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "Remote Host Manager");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1400, 850);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), root);

    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);

    app->menu_bar = create_menu_bar(app, accel_group);
    gtk_box_pack_start(GTK_BOX(root), app->menu_bar, FALSE, FALSE, 0);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(content), 6);
    gtk_box_pack_start(GTK_BOX(root), content, TRUE, TRUE, 0);

    app->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(content), app->main_paned, TRUE, TRUE, 0);

    app->sidebar_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(app->sidebar_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_reveal_child(GTK_REVEALER(app->sidebar_revealer), TRUE);

    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    // GtkWidget *sidebar_label = gtk_label_new("Saved Hosts");
    // gtk_label_set_xalign(GTK_LABEL(sidebar_label), 0.0f);
    // gtk_box_pack_start(GTK_BOX(sidebar_box), sidebar_label, FALSE, FALSE, 0);

    app->host_search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->host_search_entry), "Search hosts...");
    gtk_box_pack_start(GTK_BOX(sidebar_box), app->host_search_entry, FALSE, FALSE, 0);

    GtkWidget *sidebar_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(sidebar_scrolled, 260, -1);
    gtk_box_pack_start(GTK_BOX(sidebar_box), sidebar_scrolled, TRUE, TRUE, 0);

    app->host_listbox = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(app->host_listbox), FALSE);
    gtk_container_add(GTK_CONTAINER(sidebar_scrolled), app->host_listbox);

    gtk_container_add(GTK_CONTAINER(app->sidebar_revealer), sidebar_box);

    app->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(app->notebook), TRUE);
    gtk_notebook_popup_enable(GTK_NOTEBOOK(app->notebook));

    gtk_paned_pack1(GTK_PANED(app->main_paned), app->sidebar_revealer, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(app->main_paned), app->notebook, TRUE, FALSE);

    g_signal_connect(app->host_listbox, "row-activated", G_CALLBACK(on_host_row_activated), app);
    g_signal_connect(app->host_search_entry, "changed", G_CALLBACK(on_host_search_changed), app);

    app_load_hosts(app);
    app_load_presets(app);
    app_refresh_host_list(app);

    gtk_widget_show_all(app->window);
}