/*
 * File: preset_config.h
 * Project: Remote Host Manager
 * Author: Prashant Band
 * Version: 1.0.0
 *
 * Defines the PresetCommand model and public preset configuration API.
 * Used for storing reusable commands such as disk, memory, and uptime checks.
 *
 * License: Apache License Version 2.0
 */
 
#ifndef PRESET_CONFIG_H
#define PRESET_CONFIG_H

#include <glib.h>

typedef struct {
    gchar *label;
    gchar *command;
} PresetCommand;

PresetCommand *preset_command_new(const gchar *label, const gchar *command);
void preset_command_free(PresetCommand *p);

gchar *preset_config_default_path(void);
GPtrArray *preset_config_load_all(const gchar *config_path, GError **error);
gboolean preset_config_save_defaults_if_missing(const gchar *config_path, GError **error);

#endif