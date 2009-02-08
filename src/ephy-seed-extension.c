/*
 *  Copyright © 2009, Robert Carr
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>

#include <seed.h>

#include "ephy-seed-extension.h"

#include "ephy-extension.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

SeedEngine * global_eng = NULL;

static void ephy_seed_extension_iface_init (EphyExtensionIface *iface);

#define EPHY_SEED_EXTENSION_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SEED_EXTENSION, EphySeedExtensionPrivate))

struct _EphySeedExtensionPrivate
{
  char *filename;
	
  SeedContext ctx;
  SeedObject obj;
};

enum
  {
    PROP_0,
    PROP_FILENAME
  };

static void
ephy_seed_extension_init (EphySeedExtension *extension)
{
  LOG ("EphySeedExtension initialising");

  extension->priv = EPHY_SEED_EXTENSION_GET_PRIVATE (extension);
}

static void
call_seed_func (EphyExtension *extension,
		const char *func_name,
		EphyWindow *window,
		EphyEmbed *embed) /* HACK: tab may be NULL */
{
  EphySeedExtension *seed_ext;
  EphySeedExtensionPrivate *priv;
  SeedObject function;
  SeedException exception = NULL;
  SeedValue args[2];
	
  seed_ext = EPHY_SEED_EXTENSION (extension);
  priv = seed_ext->priv;

  if (priv->obj == NULL || seed_value_is_null (priv->ctx, priv->obj))
    return;
	
  function = seed_object_get_property (priv->ctx, priv->obj, func_name);
  
  if (!seed_value_is_function (priv->ctx, function))
    return;
	
  args[0] = seed_value_from_object (priv->ctx, G_OBJECT(window), exception);
  if (embed != NULL)
    args[1] = seed_value_from_object (priv->ctx, G_OBJECT(embed), exception);
	
  seed_object_call (global_eng->context, function, NULL, embed == NULL ? 1 : 2,
                    args, &exception);
  if (exception)
    g_warning ("seed_exception: %s \n", seed_exception_to_string (priv->ctx, exception));
	
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
  call_seed_func (extension, "attach_tab", window, embed);
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
  call_seed_func (extension, "detach_tab", window, embed);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
  call_seed_func (extension, "attach_window", window, NULL);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
  call_seed_func (extension, "detach_window", window, NULL);
}

static void
ephy_seed_extension_iface_init (EphyExtensionIface *iface)
{
  iface->attach_tab = impl_attach_tab;
  iface->detach_tab = impl_detach_tab;
  iface->attach_window = impl_attach_window;
  iface->detach_window = impl_detach_window;
}

G_DEFINE_TYPE_WITH_CODE (EphySeedExtension, ephy_seed_extension, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
                                                ephy_seed_extension_iface_init))
static gchar *
ephy_seed_extension_get_file (const gchar * name)
{
  gchar *dot_dir, *dot_path, *system_path, *dirname;

  dot_dir = g_strconcat (ephy_dot_dir (), "/extensions", NULL);
  dot_path = g_strconcat (dot_dir, "/", name, ".js", NULL);
  g_free (dot_dir);

  if (g_file_test (dot_path, G_FILE_TEST_EXISTS))
    {
      return dot_path;
    }

  system_path = g_strconcat (EXTENSIONS_DIR, name, NULL);
  if (g_file_test (system_path, G_FILE_TEST_EXISTS))
    {
      return system_path;
    }
  g_free (system_path);
  
  dirname = g_path_get_dirname (name);
  if (g_path_is_absolute (dirname))
    {
      g_free (dirname);
      return g_strdup (name);
    }
  g_free (dirname);

  return NULL;
}

static GObject *
ephy_seed_extension_constructor (GType type,
				 guint n_construct_properties,
				 GObjectConstructParam *construct_params)
{
  SeedScript *script = NULL;
  GObject *object;
  EphySeedExtension *ext;

  object =
    G_OBJECT_CLASS (ephy_seed_extension_parent_class)->constructor (type,
                                                                    n_construct_properties,
                                                                    construct_params);

  ext = EPHY_SEED_EXTENSION (object);
  
  if (ext->priv->filename)
    script = seed_script_new_from_file (global_eng->context,
                                        ext->priv->filename);
	
  ext->priv->ctx = seed_context_create (global_eng->group, NULL);
  ext->priv->obj = seed_evaluate (global_eng->context,
                                  script,
                                  NULL);
  
  if (seed_script_exception (script))
    g_warning ("seed_exception: %s", 
               seed_exception_to_string (global_eng->context,
                                         seed_script_exception (script)));
	

  return object;
}

static void
ephy_seed_extension_finalize (GObject *object)
{
  EphySeedExtension *extension =
    EPHY_SEED_EXTENSION (object);

  seed_value_unprotect (extension->priv->ctx,
                        extension->priv->obj);
  seed_context_unref (extension->priv->ctx);

  G_OBJECT_CLASS (ephy_seed_extension_parent_class)->finalize (object);
}

static void
ephy_seed_extension_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
  /* no readable properties */
  g_return_if_reached ();
}

static void
ephy_seed_extension_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
  EphySeedExtension *ext = EPHY_SEED_EXTENSION (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      ext->priv->filename = 
	ephy_seed_extension_get_file (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec); 
    }
}

static void
ephy_seed_extension_class_init (EphySeedExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_seed_extension_finalize;
  object_class->constructor = ephy_seed_extension_constructor;
  object_class->get_property = ephy_seed_extension_get_property;
  object_class->set_property = ephy_seed_extension_set_property;

  g_object_class_install_property
    (object_class,
     PROP_FILENAME,
     g_param_spec_string ("filename",
			  "Filename",
			  "Filename",
			  NULL,
			  G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | 
			  G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
			  G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (EphySeedExtensionPrivate));

  if (global_eng == NULL)
    {
      global_eng = seed_init (NULL, NULL);
      seed_simple_evaluate (global_eng->context,
                            "Seed.import_namespace('Gtk');"
                            "Seed.import_namespace('Epiphany');");
    }
}

