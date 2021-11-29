/* dzl-suggestion.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef DZL_SUGGESTION_H
#define DZL_SUGGESTION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DZL_TYPE_SUGGESTION (dzl_suggestion_get_type())

G_DECLARE_DERIVABLE_TYPE (DzlSuggestion, dzl_suggestion, DZL, SUGGESTION, GObject)

struct _DzlSuggestionClass
{
  GObjectClass parent_class;

  gchar           *(*suggest_suffix)     (DzlSuggestion *self,
                                          const gchar   *typed_text);
  gchar           *(*replace_typed_text) (DzlSuggestion *self,
                                          const gchar   *typed_text);
  GIcon           *(*get_icon)           (DzlSuggestion *self);
  cairo_surface_t *(*get_icon_surface)   (DzlSuggestion *self,
                                          GtkWidget     *widget);
  GIcon           *(*get_secondary_icon) (DzlSuggestion *self);
  cairo_surface_t *(*get_secondary_icon_surface)   (DzlSuggestion *self,
                                                    GtkWidget     *widget);
};

DzlSuggestion   *dzl_suggestion_new                (void);
const gchar     *dzl_suggestion_get_id             (DzlSuggestion *self);
void             dzl_suggestion_set_id             (DzlSuggestion *self,
                                                    const gchar   *id);
const gchar     *dzl_suggestion_get_icon_name      (DzlSuggestion *self);
void             dzl_suggestion_set_icon_name      (DzlSuggestion *self,
                                                    const gchar   *icon_name);
const gchar     *dzl_suggestion_get_title          (DzlSuggestion *self);
void             dzl_suggestion_set_title          (DzlSuggestion *self,
                                                    const gchar   *title);
const gchar     *dzl_suggestion_get_subtitle       (DzlSuggestion *self);
void             dzl_suggestion_set_subtitle       (DzlSuggestion *self,
                                                    const gchar   *subtitle);
gchar           *dzl_suggestion_suggest_suffix     (DzlSuggestion *self,
                                                    const gchar   *typed_text);
gchar           *dzl_suggestion_replace_typed_text (DzlSuggestion *self,
                                                    const gchar   *typed_text);
GIcon           *dzl_suggestion_get_icon           (DzlSuggestion *self);
cairo_surface_t *dzl_suggestion_get_icon_surface   (DzlSuggestion *self,
                                                    GtkWidget     *widget);
const gchar     *dzl_suggestion_get_secondary_icon_name (DzlSuggestion *self);
void             dzl_suggestion_set_secondary_icon_name (DzlSuggestion *self,
                                                         const gchar   *icon_name);

GIcon           *dzl_suggestion_get_secondary_icon (DzlSuggestion *self);
cairo_surface_t *dzl_suggestion_get_secondary_icon_surface (DzlSuggestion *self,
                                                            GtkWidget     *widget);

G_END_DECLS

#endif /* DZL_SUGGESTION_H */
