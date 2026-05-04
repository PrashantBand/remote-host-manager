/*
 * File: host_config.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Defines the HostConfig model used for storing SSH/SFTP host details.
 * Exposes host configuration load, save, clone, and cleanup functions.
 *
 * License: Apache License Version 2.0
 */
 
#ifndef HOST_CONFIG_H
#define HOST_CONFIG_H

#include <glib.h>

typedef struct {
    gchar *id;
    gchar *label;
    gchar *host;
    gchar *user;
    gint port;
} HostConfig;

HostConfig *host_config_new(const gchar *label, const gchar *host, const gchar *user, gint port);
HostConfig *host_config_clone(const HostConfig *src);
void host_config_free(HostConfig *cfg);

gchar *host_config_default_path(void);

GPtrArray *host_config_load_all(const gchar *config_path, GError **error);
gboolean host_config_save_all(const gchar *config_path, GPtrArray *hosts, GError **error);

#endif