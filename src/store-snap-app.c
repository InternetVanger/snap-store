/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "store-snap-app.h"

struct _StoreSnapApp
{
    StoreApp parent_instance;
};

G_DEFINE_TYPE (StoreSnapApp, store_snap_app, store_app_get_type ())

static void
find_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;

    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) snaps = snapd_client_find_finish (SNAPD_CLIENT (object), result, NULL, &error);
    if (snaps == NULL) {
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to get snap information: %s", error->message);
        return;
    }

    if (snaps->len != 1) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Snap find returned %d results, expected 1", snaps->len);
        return;
    }

    SnapdSnap *snap = g_ptr_array_index (snaps, 0);

    // FIXME: Merge in updated data
    // FIXME: Save in cache

    g_task_return_boolean (task, TRUE);
}

void
store_snap_app_refresh_async (StoreApp *self, GCancellable *cancellable, GAsyncReadyCallback callback, gpointer callback_data)
{
    g_return_if_fail (STORE_IS_SNAP_APP (self));

    g_autoptr(SnapdClient) client = snapd_client_new ();
    GTask *task = g_task_new (self, cancellable, callback, callback_data); // FIXME: Need to combine cancellables?
    snapd_client_find_async (client, SNAPD_FIND_FLAGS_MATCH_NAME, store_app_get_name (self), cancellable, find_cb, task);
}

gboolean
store_snap_app_refresh_finish (StoreApp *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail (STORE_IS_SNAP_APP (self), FALSE);
    g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

    return g_task_propagate_boolean (G_TASK (result), error);
}

static void
store_snap_app_class_init (StoreSnapAppClass *klass)
{
    STORE_APP_CLASS (klass)->refresh_async = store_snap_app_refresh_async;
    STORE_APP_CLASS (klass)->refresh_finish = store_snap_app_refresh_finish;
}

static void
store_snap_app_init (StoreSnapApp *self G_GNUC_UNUSED)
{
}

StoreSnapApp *
store_snap_app_new (void)
{
    return g_object_new (store_snap_app_get_type (), NULL);
}

static gboolean
is_screenshot (SnapdMedia *media)
{
    if (g_strcmp0 (snapd_media_get_media_type (media), "screenshot") != 0)
        return FALSE;

    /* Hide special legacy promotion screenshots */
    const gchar *url = snapd_media_get_url (media);
    g_autofree gchar *basename = g_path_get_basename (url);
    if (g_regex_match_simple ("^banner(?:_[a-zA-Z0-9]{7})?\\.(?:png|jpg)$", basename, 0, 0))
        return FALSE;
    if (g_regex_match_simple ("^banner-icon(?:_[a-zA-Z0-9]{7})?\\.(?:png|jpg)$", basename, 0, 0))
        return FALSE;

    return TRUE;
}

void
store_snap_app_update_from_search (StoreSnapApp *self, SnapdSnap *snap)
{
    store_app_set_name (STORE_APP (self), snapd_snap_get_name (snap));
    if (snapd_snap_get_title (snap) != NULL)
        store_app_set_title (STORE_APP (self), snapd_snap_get_title (snap));
    else
        store_app_set_title (STORE_APP (self), snapd_snap_get_name (snap));
    if (snapd_snap_get_publisher_display_name (snap) != NULL)
        store_app_set_publisher (STORE_APP (self), snapd_snap_get_publisher_display_name (snap));
    else
        store_app_set_publisher (STORE_APP (self), snapd_snap_get_publisher_username (snap));
    store_app_set_publisher_validated (STORE_APP (self), snapd_snap_get_publisher_validation (snap) == SNAPD_PUBLISHER_VALIDATION_VERIFIED);
    store_app_set_summary (STORE_APP (self), snapd_snap_get_summary (snap));
    store_app_set_description (STORE_APP (self), snapd_snap_get_description (snap));

    GPtrArray *media = snapd_snap_get_media (snap);
    GPtrArray *screenshots = g_ptr_array_new_with_free_func (g_object_unref);
    for (guint i = 0; i < media->len; i++) {
        SnapdMedia *m = g_ptr_array_index (media, i);
        if (g_strcmp0 (snapd_media_get_media_type (m), "icon") == 0 && store_app_get_icon (STORE_APP (self)) == NULL) {
            g_autoptr(StoreMedia) icon = store_media_new ();
            store_media_set_url (icon, snapd_media_get_url (m));
            store_media_set_width (icon, snapd_media_get_width (m));
            store_media_set_height (icon, snapd_media_get_height (m));
            store_app_set_icon (STORE_APP (self), icon);
        }
        else if (is_screenshot (m)) {
            g_autoptr(StoreMedia) screenshot = store_media_new ();
            store_media_set_url (screenshot, snapd_media_get_url (m));
            store_media_set_width (screenshot, snapd_media_get_width (m));
            store_media_set_height (screenshot, snapd_media_get_height (m));
            g_ptr_array_add (screenshots, g_steal_pointer (&screenshot));
        }
    }
    store_app_set_screenshots (STORE_APP (self), screenshots);

    g_autofree gchar *appstream_id = g_strdup_printf ("io.snapcraft.%s-%s", snapd_snap_get_name (snap), snapd_snap_get_id (snap));
    store_app_set_appstream_id (STORE_APP (self), appstream_id);
}