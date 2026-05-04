/*
 * File: host_config.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Implements saved host configuration creation, loading, saving, cloning,
 * and cleanup using GLib key-file based local configuration storage.
 *
 * License: Apache License Version 2.0
 */
 
#include "host_config.h"

static gchar *make_host_id(void) {
    return g_uuid_string_random();
}

HostConfig *host_config_new(const gchar *label, const gchar *host, const gchar *user, gint port) {
    HostConfig *cfg = g_new0(HostConfig, 1);
    cfg->id = make_host_id();
    cfg->label = g_strdup(label);
    cfg->host = g_strdup(host);
    cfg->user = g_strdup(user);
    cfg->port = port > 0 ? port : 22;
    return cfg;
}

HostConfig *host_config_clone(const HostConfig *src) {
    if (!src) return NULL;
    HostConfig *cfg = g_new0(HostConfig, 1);
    cfg->id = g_strdup(src->id);
    cfg->label = g_strdup(src->label);
    cfg->host = g_strdup(src->host);
    cfg->user = g_strdup(src->user);
    cfg->port = src->port;
    return cfg;
}

void host_config_free(HostConfig *cfg) {
    if (!cfg) return;
    g_free(cfg->id);
    g_free(cfg->label);
    g_free(cfg->host);
    g_free(cfg->user);
    g_free(cfg);
}

gchar *host_config_default_path(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "remote_host_manager", NULL);
    g_mkdir_with_parents(dir, 0700);
    gchar *path = g_build_filename(dir, "hosts.ini", NULL);
    g_free(dir);
    return path;
}

GPtrArray *host_config_load_all(const gchar *config_path, GError **error) {
    GKeyFile *kf = g_key_file_new();
    GPtrArray *hosts = g_ptr_array_new_with_free_func((GDestroyNotify)host_config_free);

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_key_file_free(kf);
        return hosts;
    }

    if (!g_key_file_load_from_file(kf, config_path, G_KEY_FILE_NONE, error)) {
        g_key_file_free(kf);
        g_ptr_array_free(hosts, TRUE);
        return NULL;
    }

    gsize len = 0;
    gchar **groups = g_key_file_get_groups(kf, &len);

    for (gsize i = 0; i < len; i++) {
        const gchar *group = groups[i];

        gchar *label = g_key_file_get_string(kf, group, "label", NULL);
        gchar *host = g_key_file_get_string(kf, group, "host", NULL);
        gchar *user = g_key_file_get_string(kf, group, "user", NULL);
        gint port = g_key_file_get_integer(kf, group, "port", NULL);

        if (label && host && user) {
            HostConfig *cfg = g_new0(HostConfig, 1);
            cfg->id = g_strdup(group);
            cfg->label = label;
            cfg->host = host;
            cfg->user = user;
            cfg->port = port > 0 ? port : 22;
            g_ptr_array_add(hosts, cfg);
        } else {
            g_free(label);
            g_free(host);
            g_free(user);
        }
    }

    g_strfreev(groups);
    g_key_file_free(kf);
    return hosts;
}

gboolean host_config_save_all(const gchar *config_path, GPtrArray *hosts, GError **error) {
    GKeyFile *kf = g_key_file_new();

    for (guint i = 0; i < hosts->len; i++) {
        HostConfig *cfg = g_ptr_array_index(hosts, i);
        if (!cfg || !cfg->id) continue;

        g_key_file_set_string(kf, cfg->id, "label", cfg->label ? cfg->label : "");
        g_key_file_set_string(kf, cfg->id, "host", cfg->host ? cfg->host : "");
        g_key_file_set_string(kf, cfg->id, "user", cfg->user ? cfg->user : "");
        g_key_file_set_integer(kf, cfg->id, "port", cfg->port);
    }

    gsize data_len = 0;
    gchar *data = g_key_file_to_data(kf, &data_len, error);
    if (!data) {
        g_key_file_free(kf);
        return FALSE;
    }

    gboolean ok = g_file_set_contents(config_path, data, (gssize)data_len, error);
    g_free(data);
    g_key_file_free(kf);
    return ok;
}