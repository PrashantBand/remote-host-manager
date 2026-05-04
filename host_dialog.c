/*
 * File: host_dialog.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Implements GTK dialogs for adding and editing saved remote hosts.
 * Handles user input for host label, address, username, and SSH port.
 *
 * License: Apache License Version 2.0
 */
 
#include "host_dialog.h"

static gboolean run_host_dialog(GtkWindow *parent, const gchar *title, HostConfig *cfg, gboolean is_edit) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        title,
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        is_edit ? "_Save" : "_Add", GTK_RESPONSE_ACCEPT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *label_lbl = gtk_label_new("Label:");
    GtkWidget *host_lbl  = gtk_label_new("Host/IP:");
    GtkWidget *user_lbl  = gtk_label_new("Username:");
    GtkWidget *port_lbl  = gtk_label_new("Port:");

    GtkWidget *label_entry = gtk_entry_new();
    GtkWidget *host_entry  = gtk_entry_new();
    GtkWidget *user_entry  = gtk_entry_new();
    GtkWidget *port_spin   = gtk_spin_button_new_with_range(1, 65535, 1);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spin), cfg->port > 0 ? cfg->port : 22);

    if (cfg->label) gtk_entry_set_text(GTK_ENTRY(label_entry), cfg->label);
    if (cfg->host)  gtk_entry_set_text(GTK_ENTRY(host_entry), cfg->host);
    if (cfg->user)  gtk_entry_set_text(GTK_ENTRY(user_entry), cfg->user);

    gtk_grid_attach(GTK_GRID(grid), label_lbl,    0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_entry,  1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), host_lbl,     0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), host_entry,   1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), user_lbl,     0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), user_entry,   1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), port_lbl,     0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), port_spin,    1, 3, 1, 1);

    gtk_widget_show_all(dialog);

    gboolean ok = FALSE;

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const gchar *label = gtk_entry_get_text(GTK_ENTRY(label_entry));
        const gchar *host  = gtk_entry_get_text(GTK_ENTRY(host_entry));
        const gchar *user  = gtk_entry_get_text(GTK_ENTRY(user_entry));
        gint port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(port_spin));

        if (label && *label && host && *host && user && *user) {
            g_free(cfg->label);
            g_free(cfg->host);
            g_free(cfg->user);

            cfg->label = g_strdup(label);
            cfg->host  = g_strdup(host);
            cfg->user  = g_strdup(user);
            cfg->port  = port;
            ok = TRUE;
        }
    }

    gtk_widget_destroy(dialog);
    return ok;
}

HostConfig *host_dialog_run_new(GtkWindow *parent) {
    HostConfig *cfg = host_config_new("", "", "", 22);
    if (run_host_dialog(parent, "Add New Server", cfg, FALSE)) {
        return cfg;
    }
    host_config_free(cfg);
    return NULL;
}

gboolean host_dialog_run_edit(GtkWindow *parent, HostConfig *cfg) {
    return run_host_dialog(parent, "Edit Server", cfg, TRUE);
}