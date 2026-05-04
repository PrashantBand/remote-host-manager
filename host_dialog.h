/*
 * File: host_dialog.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Declares dialog functions used to create or edit remote host entries.
 * Provides a small UI layer around HostConfig input and validation.
 *
 * License: Apache License Version 2.0
 */
 
 #ifndef HOST_DIALOG_H
#define HOST_DIALOG_H

#include <gtk/gtk.h>
#include "host_config.h"

HostConfig *host_dialog_run_new(GtkWindow *parent);
gboolean host_dialog_run_edit(GtkWindow *parent, HostConfig *cfg);

#endif