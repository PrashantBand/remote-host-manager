/*
 * File: session_tab.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Defines the SessionTab structure and public session management functions.
 * Represents one remote host tab with terminal, file browser, and state.
 *
 * License: Apache License Version 2.0
 */
 
#ifndef SESSION_TAB_H
#define SESSION_TAB_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <vte/vte.h>
#include "host_config.h"

typedef struct _App App;

typedef enum {
    SESSION_STATUS_CONNECTING = 0,
    SESSION_STATUS_CONNECTED,
    SESSION_STATUS_MOUNT_FAILED,
    SESSION_STATUS_DISCONNECTED
} SessionStatus;

typedef struct _SessionTab {
    App *app;
    HostConfig *host;

    GtkWidget *root;
    GtkWidget *path_entry;
    GtkWidget *treeview;
    GtkListStore *store;
    VteTerminal *terminal;

    GtkWidget *preset_combo;
    GtkWidget *run_preset_btn;
    GtkWidget *reconnect_btn;
    GtkWidget *status_label;
    GtkWidget *refresh_btn;

    GtkWidget *back_btn;
    GtkWidget *up_btn;

    GtkWidget *new_folder_btn;
    GtkWidget *rename_btn;
    GtkWidget *delete_btn;
    GtkWidget *upload_btn;
    GtkWidget *download_btn;

    GFile *current_dir;
    GQueue *back_stack;

    SessionStatus status;
    GPid terminal_pid;

    gint ref_count;
    gboolean destroying;
} SessionTab;

SessionTab *session_tab_new(App *app, const HostConfig *host);
GtkWidget *session_tab_get_widget(SessionTab *tab);
void session_tab_refresh(SessionTab *tab);
GtkWidget *session_tab_create_tab_label(SessionTab *tab, GtkWidget *page, GtkWidget *notebook);
void session_tab_disconnect(SessionTab *tab);
void session_tab_free(SessionTab *tab);
const gchar *session_tab_get_host_id(SessionTab *tab);

#endif