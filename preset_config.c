/*
 * File: preset_config.c
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Implements command preset storage, default preset creation, loading,
 * and cleanup for reusable remote terminal command shortcuts.
 *
 * License: Apache License Version 2.0
 */
 
#include "preset_config.h"

PresetCommand *preset_command_new(const gchar *label, const gchar *command) {
    PresetCommand *p = g_new0(PresetCommand, 1);
    p->label = g_strdup(label);
    p->command = g_strdup(command);
    return p;
}

void preset_command_free(PresetCommand *p) {
    if (!p) return;
    g_free(p->label);
    g_free(p->command);
    g_free(p);
}

gchar *preset_config_default_path(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), "remote_host_manager", NULL);
    g_mkdir_with_parents(dir, 0700);
    gchar *path = g_build_filename(dir, "presets.ini", NULL);
    g_free(dir);
    return path;
}

gboolean preset_config_save_defaults_if_missing(const gchar *config_path, GError **error) {
    if (g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        return TRUE;
    }

    GKeyFile *kf = g_key_file_new();

    g_key_file_set_string(kf, "preset1", "label", "Check disk");
    g_key_file_set_string(kf, "preset1", "command", "df -h");

    g_key_file_set_string(kf, "preset2", "label", "Check memory");
    g_key_file_set_string(kf, "preset2", "command", "free -h");

    g_key_file_set_string(kf, "preset3", "label", "Kernel info");
    g_key_file_set_string(kf, "preset3", "command", "uname -a");

    g_key_file_set_string(kf, "preset4", "label", "Uptime");
    g_key_file_set_string(kf, "preset4", "command", "uptime");

    gsize len = 0;
    gchar *data = g_key_file_to_data(kf, &len, error);
    if (!data) {
        g_key_file_free(kf);
        return FALSE;
    }

    gboolean ok = g_file_set_contents(config_path, data, (gssize)len, error);
    g_free(data);
    g_key_file_free(kf);
    return ok;
}

GPtrArray *preset_config_load_all(const gchar *config_path, GError **error) {
    if (!preset_config_save_defaults_if_missing(config_path, error)) {
        return NULL;
    }

    GKeyFile *kf = g_key_file_new();
    GPtrArray *arr = g_ptr_array_new_with_free_func((GDestroyNotify)preset_command_free);

    if (!g_key_file_load_from_file(kf, config_path, G_KEY_FILE_NONE, error)) {
        g_key_file_free(kf);
        g_ptr_array_free(arr, TRUE);
        return NULL;
    }

    gsize len = 0;
    gchar **groups = g_key_file_get_groups(kf, &len);

    for (gsize i = 0; i < len; i++) {
        gchar *label = g_key_file_get_string(kf, groups[i], "label", NULL);
        gchar *command = g_key_file_get_string(kf, groups[i], "command", NULL);

        if (label && *label && command && *command) {
            g_ptr_array_add(arr, preset_command_new(label, command));
        }

        g_free(label);
        g_free(command);
    }

    g_strfreev(groups);
    g_key_file_free(kf);

    return arr;
}