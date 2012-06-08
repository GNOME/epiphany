/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (C) 2012, Igalia S.L.
 */

#ifndef EPHY_ABOUT_HANDLER_H
#define EPHY_ABOUT_HANDLER_H

#include <glib.h>

#define EPHY_ABOUT_SCHEME "ephy-about"
#define EPHY_ABOUT_SCHEME_LEN 10

GString *ephy_about_handler_handle (const char *about);

void _ephy_about_handler_handle_plugins (GString *data_str,
                                         GList   *plugin_list);

#endif /* EPHY_ABOUT_HANDLER_H */
