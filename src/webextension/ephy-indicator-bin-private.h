/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alexander Mikhaylenko <alexander.mikhaylenko@puri.sm>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_INDICATOR_BIN (ephy_indicator_bin_get_type())

G_DECLARE_FINAL_TYPE (EphyIndicatorBin, ephy_indicator_bin, EPHY, INDICATOR_BIN, GtkWidget)

GtkWidget *ephy_indicator_bin_new (void) G_GNUC_WARN_UNUSED_RESULT;

GtkWidget *ephy_indicator_bin_get_child (EphyIndicatorBin *self);
void       ephy_indicator_bin_set_child (EphyIndicatorBin *self,
                                        GtkWidget       *child);

const char *ephy_indicator_bin_get_badge (EphyIndicatorBin *self);
void        ephy_indicator_bin_set_badge (EphyIndicatorBin *self,
                                         const char      *badge);

void        ephy_indicator_bin_set_badge_color (EphyIndicatorBin *self,
                                                GdkRGBA          *color);

G_END_DECLS

