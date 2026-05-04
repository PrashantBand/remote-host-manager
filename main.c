/*
 * File: main.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Application entry point for Remote Host Manager.
 * Initializes the GTK application and starts the main event loop.
 *
 * License: Apache License Version 2.0
 */

#include <gtk/gtk.h>
#include "app.h"

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)user_data;
    App *app = app_new(gtk_app);
    app_build_ui(app);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.remotehostmanager", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}