/*
 * File: app.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Defines the main App structure and public application-level functions.
 * Provides shared application state for host management and session tabs.
 *
 * License: Apache License Version 2.0
 */
 
#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include "host_config.h"
#include "preset_config.h"

typedef struct _SessionTab SessionTab;

typedef struct _App {
    GtkApplication *gtk_app;
    GtkWidget *window;

    GtkWidget *main_paned;
    GtkWidget *sidebar_revealer;
    GtkWidget *host_listbox;
    GtkWidget *notebook;

    GtkWidget *menu_bar;

    GtkWidget *toggle_sidebar_item;
    GtkWidget *add_host_item;
    GtkWidget *edit_host_item;
    GtkWidget *remove_host_item;
    GtkWidget *quit_item;
    GtkWidget *about_item;

    GtkWidget *host_search_entry;

    GPtrArray *hosts;
    gchar *config_path;

    GPtrArray *presets;            /* PresetCommand* */
    gchar *preset_config_path;
} App;

App *app_new(GtkApplication *gtk_app);
void app_build_ui(App *app);
void app_load_hosts(App *app);
void app_refresh_host_list(App *app);
void app_open_host_in_tab(App *app, HostConfig *host);
void app_save_hosts(App *app);
void app_remove_selected_host(App *app);
void app_edit_selected_host(App *app);
void app_load_presets(App *app);

#endif