/*
 * File: util.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Implements common utility functions for file operations, dialogs,
 * and human-readable formatting used across the application.
 *
 * License: Apache License Version 2.0
 */
 
#include "util.h"

gchar *util_human_readable_size(goffset size) {
    if (size < 1024) return g_strdup_printf("%" G_GOFFSET_FORMAT " B", size);
    if (size < 1024 * 1024) return g_strdup_printf("%.1f KB", (double) size / 1024.0);
    if (size < 1024LL * 1024LL * 1024LL) return g_strdup_printf("%.1f MB", (double) size / (1024.0 * 1024.0));
    return g_strdup_printf("%.1f GB", (double) size / (1024.0 * 1024.0 * 1024.0));
}

void util_show_error(GtkWindow *parent, const gchar *message) {
    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_CLOSE,
        "%s",
        message
    );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void util_show_info(GtkWindow *parent, const gchar *message) {
    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_CLOSE,
        "%s",
        message
    );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean util_copy_gfile(GFile *src, GFile *dest, GtkWindow *parent, GError **error) {
    gboolean ok = g_file_copy(
        src, dest,
        G_FILE_COPY_OVERWRITE,
        NULL, NULL, NULL,
        error
    );
    if (!ok && error && *error && parent) {
        util_show_error(parent, (*error)->message);
    }
    return ok;
}

gboolean util_delete_gfile(GFile *file, GtkWindow *parent, GError **error) {
    gboolean ok = g_file_delete(file, NULL, error);
    if (!ok && error && *error && parent) {
        util_show_error(parent, (*error)->message);
    }
    return ok;
}

gboolean util_make_directory(GFile *dir, GtkWindow *parent, GError **error) {
    gboolean ok = g_file_make_directory(dir, NULL, error);
    if (!ok && error && *error && parent) {
        util_show_error(parent, (*error)->message);
    }
    return ok;
}

gboolean util_move_gfile(GFile *src, GFile *dest, GtkWindow *parent, GError **error) {
    gboolean ok = g_file_move(
        src, dest,
        G_FILE_COPY_OVERWRITE,
        NULL, NULL, NULL,
        error
    );
    if (!ok && error && *error && parent) {
        util_show_error(parent, (*error)->message);
    }
    return ok;
}