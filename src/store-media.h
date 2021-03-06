/*
 * Copyright (C) 2019 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (StoreMedia, store_media, STORE, MEDIA, GObject)

StoreMedia  *store_media_new           (void);

StoreMedia  *store_media_new_from_json (JsonNode *node);

JsonNode    *store_media_to_json       (StoreMedia *media);

void         store_media_set_height    (StoreMedia *media, guint height);

guint        store_media_get_height    (StoreMedia *media);

void         store_media_set_width     (StoreMedia *media, guint width);

guint        store_media_get_width     (StoreMedia *media);

void         store_media_set_uri       (StoreMedia *media, const gchar *uri);

const gchar *store_media_get_uri       (StoreMedia *media);

G_END_DECLS
