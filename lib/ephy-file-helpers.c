/*
 *  Copyright (C) 2002 Jorn Baayen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-exec.h>

#include "ephy-file-helpers.h"

static GHashTable *files = NULL;

static char *dot_dir = NULL;

char *
ephy_file_tmp_filename (const char *base,
			const char *extension)
{
	int fd;
	char *name = g_strdup (base);

	fd = mkstemp (name);

	if (fd != -1)
	{
		unlink (name);
		close (fd);
	}
	else
	{
		return NULL;
	}

	if (extension)
	{
		char *tmp;
		tmp = g_strconcat (name, ".",
				   extension, NULL);
		g_free (name);
		name = tmp;
	}

	return name;
}

const char *
ephy_file (const char *filename)
{
	char *ret;
	int i;

	static char *paths[] =
	{
		SHARE_DIR "/",
		SHARE_DIR "/glade/",
		SHARE_DIR "/art/",
		SHARE_UNINSTALLED_DIR "/",
		SHARE_UNINSTALLED_DIR "/glade/",
		SHARE_UNINSTALLED_DIR "/art/"
	};

	g_assert (files != NULL);

	ret = g_hash_table_lookup (files, filename);
	if (ret != NULL)
		return ret;

	for (i = 0; i < (int) G_N_ELEMENTS (paths); i++)
	{
		ret = g_strconcat (paths[i], filename, NULL);
		if (g_file_test (ret, G_FILE_TEST_EXISTS) == TRUE)
		{
			g_hash_table_insert (files, g_strdup (filename), ret);
			return (const char *) ret;
		}
		g_free (ret);
	}

	g_warning (_("Failed to find %s"), filename);

	return NULL;
}

const char *
ephy_dot_dir (void)
{
	if (dot_dir == NULL)
	{
		dot_dir = g_build_filename (g_get_home_dir (),
					    GNOME_DOT_GNOME,
					    "epiphany",
					    NULL);
	}

	return dot_dir;
}

void
ephy_file_helpers_init (void)
{
	files = g_hash_table_new_full (g_str_hash,
				       g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_free);
}

void
ephy_file_helpers_shutdown (void)
{
	g_hash_table_destroy (files);

	g_free (dot_dir);
}

static void
gul_general_gnome_shell_execute (const char *command)
{
        GError *error = NULL;
        if (!g_spawn_command_line_async (command, &error)) {
                g_warning ("Error starting command '%s': %s\n", command, error->message);
                g_error_free (error);
        }
}

/* Return a command string containing the path to a terminal on this system. */

static char *
try_terminal_command (const char *program,
                      const char *args)
{
        char *program_in_path, *quoted, *result;

        if (program == NULL) {
                return NULL;
        }

        program_in_path = g_find_program_in_path (program);
        if (program_in_path == NULL) {
                return NULL;
        }

        quoted = g_shell_quote (program_in_path);
        if (args == NULL || args[0] == '\0') {
                return quoted;
        }
        result = g_strconcat (quoted, " ", args, NULL);
        g_free (quoted);
        return result;
}

static char *
try_terminal_command_argv (int argc,
                           char **argv)
{
        GString *string;
        int i;
        char *quoted, *result;

        if (argc == 0) {
                return NULL;
        }

        if (argc == 1) {
                return try_terminal_command (argv[0], NULL);
	}

	string = g_string_new (argv[1]);
        for (i = 2; i < argc; i++) {
                quoted = g_shell_quote (argv[i]);
                g_string_append_c (string, ' ');
                g_string_append (string, quoted);
                g_free (quoted);
        }
        result = try_terminal_command (argv[0], string->str);
        g_string_free (string, TRUE);

        return result;
}

static char *
get_terminal_command_prefix (gboolean for_command)
{
        int argc;
        char **argv;
        char *command;
        guint i;
        static const char *const commands[][3] = {
                { "gnome-terminal", "-x",                                      "" },
                { "dtterm",         "-e",                                      "-ls" },
                { "nxterm",         "-e",                                      "-ls" },
                { "color-xterm",    "-e",                                      "-ls" },
                { "rxvt",           "-e",                                      "-ls" },
                { "xterm",          "-e",                                      "-ls" },
        };

        /* Try the terminal from preferences. Use without any
         * arguments if we are just doing a standalone terminal.
         */
        argc = 0;
        argv = g_new0 (char *, 1);
        gnome_prepend_terminal_to_vector (&argc, &argv);

        command = NULL;
        if (argc != 0) {
                if (for_command) {
                        command = try_terminal_command_argv (argc, argv);
                } else {
                        /* Strip off the arguments in a lame attempt
                         * to make it be an interactive shell.
                         */
                        command = try_terminal_command (argv[0], NULL);
                }
        }

        while (argc != 0) {
                g_free (argv[--argc]);
        }
        g_free (argv);

        if (command != NULL) {
                return command;
        }

        /* Try well-known terminal applications in same order that gmc did. */
        for (i = 0; i < G_N_ELEMENTS (commands); i++) {
                command = try_terminal_command (commands[i][0],
                                                commands[i][for_command ? 1 : 2]);
                if (command != NULL) {
                        break;
                }
        }

        return command;
}

static char *
gul_general_gnome_make_terminal_command (const char *command)
{
        char *prefix, *quoted, *terminal_command;

        if (command == NULL) {
                return get_terminal_command_prefix (FALSE);
        }
        prefix = get_terminal_command_prefix (TRUE);
        quoted = g_shell_quote (command);
        terminal_command = g_strconcat (prefix, " /bin/sh -c ", quoted, NULL);
        g_free (prefix);
        g_free (quoted);
        return terminal_command;
}

static void
gul_general_gnome_open_terminal (const char *command)
{
        char *command_line;

        command_line = gul_general_gnome_make_terminal_command (command);
        if (command_line == NULL) {
                g_message ("Could not start a terminal");
                return;
        }
        gul_general_gnome_shell_execute (command_line);
        g_free (command_line);
}

void
ephy_file_launch_application (const char *command_string,
                              const char *parameter,
                              gboolean use_terminal)
{
        char *full_command;
        char *quoted_parameter;

        if (parameter != NULL) {
                quoted_parameter = g_shell_quote (parameter);
                full_command = g_strconcat (command_string, " ", quoted_parameter, NULL);
                g_free (quoted_parameter);
        } else {
                full_command = g_strdup (command_string);
        }

        if (use_terminal) {
                gul_general_gnome_open_terminal (full_command);
        } else {
                gul_general_gnome_shell_execute (full_command);
        }

        g_free (full_command);
}

void
ephy_ensure_dir_exists (const char *dir)
{
	if (g_file_test (dir, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (g_file_test (dir, G_FILE_TEST_EXISTS) == TRUE)
			g_error (_("%s exists, please move it out of the way."), dir);

		if (mkdir (dir, 488) != 0)
			g_error (_("Failed to create directory %s."), dir);
	}
}
