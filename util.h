/*
 * File: util.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Declares common helper functions for GTK dialogs, GFile operations,
 * and display formatting shared by multiple application modules.
 *
 * License: Apache License Version 2.0
 */
 
#ifndef UTIL_H
#define UTIL_H

#include <gtk/gtk.h>
#include <gio/gio.h>

gchar *util_human_readable_size(goffset size);
void util_show_error(GtkWindow *parent, const gchar *message);
void util_show_info(GtkWindow *parent, const gchar *message);

gboolean util_copy_gfile(GFile *src, GFile *dest, GtkWindow *parent, GError **error);
gboolean util_delete_gfile(GFile *file, GtkWindow *parent, GError **error);
gboolean util_make_directory(GFile *dir, GtkWindow *parent, GError **error);
gboolean util_move_gfile(GFile *src, GFile *dest, GtkWindow *parent, GError **error);

#endif