#include <string.h>
#include "egg-markup.h"
#include "eggtoolbar.h"

#ifndef _
#  define _(String) (String)
#  define N_(String) (String)
#endif

typedef enum {
  STATE_START,
  STATE_ROOT,
  STATE_MENU,
  STATE_TOOLBAR,
  STATE_POPUPS,
  STATE_ITEM,
  STATE_END
} ParseState;

typedef struct _ParseContext ParseContext;
struct _ParseContext
{
  /* parser state information */
  ParseState state;
  ParseState prev_state;

  /* function to call when we finish off a toplevel widget */
  EggWidgetFunc widget_func;
  gpointer user_data;

  /* GdkAccelGroup to use for menus */
  GtkAccelGroup *accel_group;

  /* info about the widget we are constructing at the moment */
  GtkWidget *top;
  gchar *type;
  gchar *name;

  /* the current container we are working on */
  GtkWidget *current;

  /* the ActionGroup used to create menu items */
  EggActionGroup *action_group;
};

static void
start_element_handler (GMarkupParseContext *context,
		       const gchar         *element_name,
		       const gchar        **attribute_names,
		       const gchar        **attribute_values,
		       gpointer             user_data,
		       GError             **error)
{
  ParseContext *ctx = user_data;
  gboolean raise_error = TRUE;
  gchar *error_attr = NULL;

  switch (element_name[0])
    {
    case 'R':
      if (ctx->state == STATE_START && !strcmp(element_name, "Root"))
	{
	  ctx->state = STATE_ROOT;
	  raise_error = FALSE;
	}
      break;
    case 'm':
      if (ctx->state == STATE_ROOT && !strcmp(element_name, "menu"))
	{
	  ctx->state = STATE_MENU;

	  ctx->top = ctx->current = gtk_menu_bar_new();
	  ctx->type = "menu";
	  ctx->name = NULL;

	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_MENU && !strcmp(element_name, "menuitem"))
	{
	  gint i;
	  const gchar *action_name = NULL;
	  EggAction *action = NULL;

	  ctx->state = STATE_ITEM;

	  for (i = 0; attribute_names[i] != NULL; i++)
	    {
	      if (!strcmp(attribute_names[i], "verb"))
		{
		  action_name = attribute_values[i];
		  action = egg_action_group_get_action(ctx->action_group,
							 action_name);
		}
	    }

	  if (action)
	    {
	      GtkWidget *widget = egg_action_create_menu_item(action);

	      gtk_container_add(GTK_CONTAINER(ctx->current), widget);
	      gtk_widget_show(widget);
	    }
	  else
	    {
	      g_warning("could not find action '%s'",
			action_name ? action_name : "(null)");
	    }

	  raise_error = FALSE;
	}
      break;
    case 'd':
      if (ctx->state == STATE_ROOT && !strcmp(element_name, "dockitem"))
	{
	  gint i;

	  ctx->state = STATE_TOOLBAR;

	  ctx->top = ctx->current = egg_toolbar_new();
	  ctx->type = "toolbar";
	  for (i = 0; attribute_names[i] != NULL; i++)
	    {
	      if (!strcmp(attribute_names[i], "name"))
		ctx->name = g_strdup(attribute_values[i]);
	    }

	  raise_error = FALSE;
	}
      break;
    case 'p':
      if (ctx->state == STATE_ROOT && !strcmp(element_name, "popups"))
	{
	  ctx->state = STATE_POPUPS;
	  raise_error = FALSE;
	}
      else if (ctx->state == STATE_POPUPS &&!strcmp(element_name, "popup"))
	{
	  gint i;

	  ctx->state = STATE_MENU;

	  ctx->top = ctx->current = gtk_menu_new();
	  gtk_menu_set_accel_group(GTK_MENU(ctx->current), ctx->accel_group);
	  ctx->type = "popup";
	  for (i = 0; attribute_names[i] != NULL; i++)
	    {
	      if (!strcmp(attribute_names[i], "name"))
		{
		  ctx->name = g_strdup(attribute_values[i]);
		  gtk_menu_set_title(GTK_MENU(ctx->current), ctx->name);
		}
	      else if (!strcmp(attribute_names[i], "tearoff"))
		{
		  GtkWidget *tearoff = gtk_tearoff_menu_item_new();

		  gtk_container_add(GTK_CONTAINER(ctx->current), tearoff);
		  gtk_widget_show(tearoff);
		}
	    }

	  raise_error = FALSE;
	}
      break;
    case 's':
      if (ctx->state == STATE_MENU && !strcmp(element_name, "submenu"))
	{
	  gint i;
	  const gchar *label = NULL;
	  gboolean tearoff = FALSE;
	  GtkWidget *widget;

	  ctx->state = STATE_MENU;
	  for (i = 0; attribute_names[i] != NULL; i++)
	    {
	      if (!strcmp(attribute_names[i], "label"))
		label = g_strdup(attribute_values[i]);
	      else if (!strcmp(attribute_names[i], "tearoff"))
		tearoff = TRUE;
	    }
	  widget = gtk_menu_item_new_with_label(label);
	  gtk_label_set_use_underline(GTK_LABEL(GTK_BIN(widget)->child), TRUE);
	  gtk_container_add(GTK_CONTAINER(ctx->current), widget);
	  gtk_widget_show(widget);

	  ctx->current = gtk_menu_new();
	  gtk_menu_set_accel_group(GTK_MENU(ctx->current), ctx->accel_group);
	  gtk_menu_set_title(GTK_MENU(ctx->current), label);
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget), ctx->current);

	  if (tearoff)
	    {
	      GtkWidget *tearoff = gtk_tearoff_menu_item_new();

	      gtk_container_add(GTK_CONTAINER(ctx->current), tearoff);
	      gtk_widget_show(tearoff);
	    }
	  
	  raise_error = FALSE;
	}
      else if ((ctx->state == STATE_MENU || ctx->state == STATE_TOOLBAR) &&
	       !strcmp(element_name, "separator"))
	{
	  ctx->state = STATE_ITEM;

	  if (GTK_IS_MENU_SHELL(ctx->current))
	    {
	      GtkWidget *widget = gtk_separator_menu_item_new();
	      gtk_container_add(GTK_CONTAINER(ctx->current), widget);
	      gtk_widget_show(widget);
	    }
	  else /* toolbar */
	    {
	      EggToolItem *item = egg_tool_item_new ();
	      egg_toolbar_insert_tool_item (EGG_TOOLBAR(ctx->current), item, -1);
	      gtk_widget_show (GTK_WIDGET (item));
	    }

	  raise_error = FALSE;
	}
      break;
    case 't':
      if (ctx->state == STATE_TOOLBAR && !strcmp(element_name, "toolitem"))
	{
	  gint i;
	  const gchar *action_name = NULL;
	  EggAction *action = NULL;

	  ctx->state = STATE_ITEM;

	  for (i = 0; attribute_names[i] != NULL; i++)
	    {
	      if (!strcmp(attribute_names[i], "verb"))
		{
		  action_name = attribute_values[i];
		  action = egg_action_group_get_action(ctx->action_group,
							 action_name);
		}
	    }

	  if (action)
	    {
	      GtkWidget *widget = egg_action_create_tool_item (action);

	      gtk_container_add (GTK_CONTAINER (ctx->current), widget);
	    }
	  else
	    {
	      g_warning("could not find action '%s'",
			action_name ? action_name : "(null)");
	    }

	  raise_error = FALSE;
	}
      break;
    };

  if (raise_error)
    {
      gint line_number, char_number;

      g_markup_parse_context_get_position (context,
                                           &line_number, &char_number);

      if (error_attr)
	g_set_error (error,
		     G_MARKUP_ERROR,
		     G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
		     _("Unknown attribute '%s' on line %d char %d"),
		     error_attr,
		     line_number, char_number);
      else
	g_set_error (error,
		     G_MARKUP_ERROR,
		     G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		     _("Unknown tag '%s' on line %d char %d"),
		     element_name,
		     line_number, char_number);
    }
}

static void
end_element_handler (GMarkupParseContext *context,
		     const gchar         *element_name,
		     gpointer             user_data,
		     GError             **error)
{
  ParseContext *ctx = user_data;
  GtkWidget *widget;

  switch (ctx->state)
    {
    case STATE_START:
      g_warning("shouldn't get any end tags at this point");
      /* should do a GError here */
      break;
    case STATE_ROOT:
      ctx->state = STATE_END;
      break;
    case STATE_MENU:
      widget = GTK_IS_MENU(ctx->current) ?
	gtk_menu_get_attach_widget(GTK_MENU(ctx->current)) : NULL;
      if (widget) /* not back to the toplevel ... */
	{
	  ctx->current = widget->parent;
	  ctx->state = STATE_MENU;
	}
      else
	{
	  if (GTK_IS_MENU(ctx->current)) /* must be a popup */
	    ctx->state = STATE_POPUPS;
	  else
	    ctx->state = STATE_ROOT;

	  /* notify */
	  (* ctx->widget_func)(ctx->top, ctx->type, ctx->name, ctx->user_data);
	  ctx->top = NULL;
	  ctx->type = NULL;
	  g_free(ctx->name);
	  ctx->name = NULL;
	  ctx->current = NULL;
	}
      break;
    case STATE_TOOLBAR:
      ctx->state = STATE_ROOT;

      /* notify */
      (* ctx->widget_func)(ctx->top, ctx->type, ctx->name, ctx->user_data);
      ctx->top = NULL;
      ctx->type = NULL;
      g_free(ctx->name);
      ctx->name = NULL;
      ctx->current = NULL;
      break;
    case STATE_POPUPS:
      ctx->state = STATE_ROOT;
      break;
    case STATE_ITEM:
      if (GTK_IS_MENU_SHELL(ctx->current))
	ctx->state = STATE_MENU;
      else
	ctx->state = STATE_TOOLBAR;
      break;
    case STATE_END:
      g_warning("shouldn't get any end tags at this point");
      /* should do a GError here */
      break;
    }
}

static void
cleanup (GMarkupParseContext *context,
	 GError              *error,
	 gpointer             user_data)
{
  ParseContext *ctx = user_data;

  gtk_widget_destroy(ctx->top);
  ctx->top = NULL;
  ctx->type = NULL;
  g_free(ctx->name);
  ctx->name = NULL;
  ctx->current = NULL;
}


static GMarkupParser ui_parser = {
  start_element_handler,
  end_element_handler,
  NULL,
  NULL,
  cleanup
};


gboolean
egg_create_from_string (EggActionGroup *action_group,
			EggWidgetFunc widget_func, gpointer user_data,
			GtkAccelGroup *accel_group,
			const gchar *buffer, guint length,
			GError **error)
{
  ParseContext ctx = { 0 };
  GMarkupParseContext *context;
  gboolean res = TRUE;

  g_return_val_if_fail(EGG_IS_ACTION_GROUP(action_group), FALSE);
  g_return_val_if_fail(widget_func != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_ACCEL_GROUP(accel_group), FALSE);
  g_return_val_if_fail(buffer != NULL, FALSE);

  ctx.state = STATE_START;
  ctx.widget_func = widget_func;
  ctx.user_data = user_data;
  ctx.accel_group = accel_group;
  ctx.top = NULL;
  ctx.type = NULL;
  ctx.name = NULL;
  ctx.current = NULL;
  ctx.action_group = action_group;

  context = g_markup_parse_context_new(&ui_parser, 0, &ctx, NULL);
  if (length < 0)
    length = strlen(buffer);

  if (g_markup_parse_context_parse(context, buffer, length, error))
    {
      if (!g_markup_parse_context_end_parse(context, error))
	res = FALSE;
    }
  else
    res = FALSE;

  g_markup_parse_context_free (context);

  return res;
}

gboolean
egg_create_from_file (EggActionGroup *action_group,
		      EggWidgetFunc widget_func,
		      gpointer user_data,
		      GtkAccelGroup *accel_group,
		      const gchar *filename,
		      GError **error)
{
  gchar *buffer;
  gint length;
  gboolean res;

  if (!g_file_get_contents (filename, &buffer, &length, error))
    return FALSE;

  res = egg_create_from_string(action_group, widget_func, user_data,
			       accel_group, buffer, length, error);
  g_free(buffer);

  return res;
}
