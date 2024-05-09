/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2024 Roland Chamuel (pxrb).
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-web-extension.h"

static const struct {
  const char *before;
  const char *after;
} parse_command_tests[] = {
  { "Alt+Shift+Space", "<Alt><Shift>space" },
  { "Command+Period", "<Ctrl>period" },
  { "MacCtrl+Shift+Comma", "<Ctrl><Shift>comma" },
  { "Ctrl+Alt+PageUp", "<Ctrl><Alt>Page_Up" },
  { "Alt+Shift+PageDown", "<Alt><Shift>Page_Down" },
  { "Command+F5", "<Ctrl>F5" },
  { "Alt+7", "<Alt>7" },
  { "MacCtrl+G", "<Ctrl>G" },
  { "Ctrl+Shift+Z", "<Ctrl><Shift>Z" },
  { "Ctrl+Home", "<Ctrl>Home" },
  { "MediaNextTrack", "XF86AudioNext" },
  { "MediaPlayPause", "XF86AudioPlay" },
  { "MediaPrevTrack", "XF86AudioPrev" },
  { "MediaStop", "XF86AudioStop" }
};

static void
test_ephy_invalid_command_parse (void)
{
  for (gulong i = 0; i < G_N_ELEMENTS (parse_command_tests); i++) {
    g_autofree char *result = ephy_web_extension_parse_command_key (parse_command_tests[i].before);
    g_assert_cmpstr (result, ==, parse_command_tests[i].after);
  }
}

int
main (int   argc,
      char *argv[])
{
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/lib/ephy-web-extension/invalid_command_parse", test_ephy_invalid_command_parse);

  ret = g_test_run ();

  return ret;
}
