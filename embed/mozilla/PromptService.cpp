/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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
 */

#include "ephy-glade.h"
#include "PromptService.h"

#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "MozillaPrivate.h"

#include "nsIPromptService.h"
#include "nsIUnicodeEncoder.h"

#include <gtk/gtkentry.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtklist.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-triggers.h>

/* local function prototypes */
static void set_title (GtkWidget *dialog, const PRUnichar *title);
static void set_label_text (GtkWidget *label, const PRUnichar *text);
static void set_check_button_text (GtkWidget *check_button, 
				   const PRUnichar *text);
static void set_check_button (GtkWidget *button, PRBool *value);
static void set_check_button_size_to_label (GtkWidget *check_button,
					    GtkWidget *label);
static void set_editable (GtkWidget *entry, PRUnichar **text);
static void get_check_button (GtkWidget *button, PRBool *value);
static void get_editable (GtkWidget *editable, PRUnichar **text);

/**
 * class CPromptService: an GNOME implementation of prompt dialogs for
 * Mozilla
 */
class CPromptService: public nsIPromptService
{
	public:
		CPromptService();
  		virtual ~CPromptService();

		NS_DECL_ISUPPORTS
		NS_DECL_NSIPROMPTSERVICE
	private:
		nsresult AddButton (GtkWidget *dialog, 
		   	   	    char type, 
		   	   	    const PRUnichar *title,
		   	   	    int id,
				    GtkWidget **widget);
};

NS_IMPL_ISUPPORTS1 (CPromptService, nsIPromptService)

/**
 * CPromptService::CPromptService: constructor
 */
CPromptService::CPromptService ()
{
	NS_INIT_ISUPPORTS();
}

/**
 * CPromptService::~CPromptService: destructor
 */
CPromptService::~CPromptService ()
{
}

/**
 * CPromptService::Alert: show an alert box
 */
NS_IMETHODIMP CPromptService::Alert (nsIDOMWindow *parent, 
				     const PRUnichar *dialogTitle,
				     const PRUnichar *text)
{    
	GtkWidget *dialog;
	GtkWidget *gparent;

	gparent = MozillaFindGtkParent (parent);
	const nsACString &msg = NS_ConvertUCS2toUTF8 (text);
	dialog = gtk_message_dialog_new (GTK_WINDOW(gparent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_OK,
					 "%s",
					 PromiseFlatCString(msg).get ());
	set_title (dialog, dialogTitle);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return NS_OK;
}

/**
 * CPromptService::AlertCheck: show an alert box with a checkbutton,
 * (typically for things like "dont show this warning again")
 */
NS_IMETHODIMP CPromptService::AlertCheck (nsIDOMWindow *parent, 
					  const PRUnichar *dialogTitle,
					  const PRUnichar *text,
					  const PRUnichar *checkMsg, 
					  PRBool *checkValue)
{
	GtkWidget *dialog;
	GtkWidget *gparent;
	GtkWidget *check_button;
	
	gparent = MozillaFindGtkParent (parent);
	const nsACString &msg = NS_ConvertUCS2toUTF8 (text);
	dialog = gtk_message_dialog_new (GTK_WINDOW(gparent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_OK,
					 "%s",
					 PromiseFlatCString(msg).get ());

	check_button = gtk_check_button_new_with_label ("");
	gtk_widget_show (check_button);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), 
			    check_button, FALSE, FALSE, 5);
	set_check_button_text (check_button, checkMsg);
	set_check_button (check_button, checkValue);

	set_title (dialog, dialogTitle);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_run (GTK_DIALOG (dialog));
	get_check_button (check_button, checkValue);
	gtk_widget_destroy (dialog);
	
	return NS_OK;
}

/**
 * CPromptService::Confirm: for simple yes/no dialogs
 */
NS_IMETHODIMP CPromptService::Confirm (nsIDOMWindow *parent, 
				       const PRUnichar *dialogTitle,
				       const PRUnichar *text,
				       PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *gparent;
	int res;

	gparent = MozillaFindGtkParent (parent);
	const nsACString &msg = NS_ConvertUCS2toUTF8 (text);
	dialog = gtk_message_dialog_new (GTK_WINDOW(gparent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 "%s", PromiseFlatCString(msg).get ());
	set_title (dialog, dialogTitle);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	*_retval = (res == GTK_RESPONSE_YES);
	gtk_widget_destroy (dialog);
	
	return NS_OK;
}

/**
 * CPromptService::Confirm: for simple yes/no dialogs, with an additional
 * check button
 */
NS_IMETHODIMP CPromptService::ConfirmCheck (nsIDOMWindow *parent, 
					    const PRUnichar *dialogTitle,
					    const PRUnichar *text,
					    const PRUnichar *checkMsg, 
					    PRBool *checkValue,
					    PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *gparent;
	GtkWidget *check_button;
	int res;

	gparent = MozillaFindGtkParent (parent);
	const nsACString &msg = NS_ConvertUCS2toUTF8 (text);
	dialog = gtk_message_dialog_new (GTK_WINDOW(gparent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO,
					 "%s", PromiseFlatCString(msg).get ());

	check_button = gtk_check_button_new_with_label ("");
	gtk_widget_show (check_button);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), 
			    check_button, FALSE, FALSE, 5);
	set_check_button_text (check_button, checkMsg);
	set_check_button (check_button, checkValue);
	
	set_title (dialog, dialogTitle);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	*_retval = (res == GTK_RESPONSE_YES);
	get_check_button (check_button, checkValue);
	gtk_widget_destroy (dialog);

	return NS_OK;
}

NS_IMETHODIMP CPromptService::AddButton (GtkWidget *dialog, 
		   			 char type, 
		   			 const PRUnichar *title,
		   			 int id,
					 GtkWidget **widget)
{
	const char *btitle;
	const nsACString &utf8string = NS_ConvertUCS2toUTF8 (title);

	switch (type)
	{
		case BUTTON_TITLE_OK:
			btitle = GTK_STOCK_OK;
			break;
		case BUTTON_TITLE_CANCEL:
			btitle = GTK_STOCK_CANCEL;
			break;
		case BUTTON_TITLE_YES:
			btitle = GTK_STOCK_YES;
			break;
		case BUTTON_TITLE_NO:
			btitle = GTK_STOCK_NO;
			break;
		case BUTTON_TITLE_SAVE:
			btitle = _("Save");
			break;
		case BUTTON_TITLE_REVERT:
			btitle = _("Revert");
			break;
		case BUTTON_TITLE_DONT_SAVE:
			btitle = _("Don't save");
			break;
		case BUTTON_TITLE_IS_STRING:
			btitle = NULL;
			break;

		default:
			return NS_ERROR_FAILURE;
	}

	*widget = gtk_dialog_add_button (GTK_DIALOG(dialog),
			                 btitle ? btitle : 
			                 PromiseFlatCString(utf8string).get(),
			                 id);

	return NS_OK;
}

/**
 * CPromptService::ConfirmEx: For fancy confirmation dialogs
 */
NS_IMETHODIMP CPromptService::ConfirmEx (nsIDOMWindow *parent, 
					 const PRUnichar *dialogTitle,
					 const PRUnichar *text,
					 PRUint32 buttonFlags, 
					 const PRUnichar *button0Title,
					 const PRUnichar *button1Title,
					 const PRUnichar *button2Title,
					 const PRUnichar *checkMsg, 
					 PRBool *checkValue,
					 PRInt32 *buttonPressed)
{
	GtkWidget *dialog;
	GtkWidget *gparent;
	GtkWidget *default_button = NULL;
	GtkWidget *check_button = NULL;
	int ret;

	gparent = MozillaFindGtkParent (parent);
	const nsACString &msg = NS_ConvertUCS2toUTF8 (text);
	dialog = gtk_message_dialog_new (GTK_WINDOW(gparent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 "%s", PromiseFlatCString(msg).get ());

	set_title (dialog, dialogTitle);

	if (checkMsg)
	{
		check_button = gtk_check_button_new_with_label ("");
		gtk_widget_show (check_button);
		gtk_box_pack_end (GTK_BOX (GTK_DIALOG(dialog)->vbox), 
				  check_button, FALSE, FALSE, 5);

		set_check_button_text (check_button, checkMsg);
		set_check_button (check_button, checkValue);
	}
	
	AddButton (dialog, (buttonFlags >> 16) & 0xFF, 
		   button2Title, 2, &default_button);
	AddButton (dialog, (buttonFlags >> 8) & 0xFF, 
		   button1Title, 1, &default_button);
	AddButton (dialog, buttonFlags & 0xFF, 
		   button0Title, 0, &default_button);
	
	gtk_dialog_set_default_response (GTK_DIALOG(dialog), 0);

	/* make a suitable sound */
	gnome_triggers_vdo ("", "generic", NULL);

	gtk_window_set_focus (GTK_WINDOW (dialog), default_button);
	
	/* run dialog and capture return values */
	ret = gtk_dialog_run (GTK_DIALOG (dialog));
	
	/* handle return values */
	if (checkMsg)
	{
		get_check_button (check_button, checkValue);
	}

	*buttonPressed = ret;
	
	/* done */
	gtk_widget_destroy (dialog);
	return NS_OK;
}

/**
 * CPromptService::Prompt: show a prompt for text, with a checkbutton
 */
NS_IMETHODIMP CPromptService::Prompt (nsIDOMWindow *parent, 
				      const PRUnichar *dialogTitle,
				      const PRUnichar *text, 
				      PRUnichar **value,
				      const PRUnichar *checkMsg, 
				      PRBool *checkValue,
				      PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *check_button;
	GtkWidget *gparent;
	GladeXML *gxml;
	gint ret;

	/* build and show the dialog */
	gxml = ephy_glade_widget_new ("prompts.glade", "prompt_dialog", 
				 &dialog, NULL);
	entry = glade_xml_get_widget (gxml, "entry");
	label = glade_xml_get_widget (gxml, "label");
	check_button = glade_xml_get_widget (gxml, "check_button");
	g_object_unref (G_OBJECT (gxml));

	/* parent the dialog */
	gparent = MozillaFindGtkParent (parent);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (gparent));
	
	/* set dynamic attributes */
	set_title (dialog, dialogTitle);
	set_label_text (label, text);
	set_check_button_text (check_button, checkMsg);
	set_editable (entry, value);
	set_check_button (check_button, checkValue);
	set_check_button_size_to_label (check_button, label);
	
	/* make a suitable sound */
	/* NB: should be question, but this is missing in many
	 * of the current gnome sound packages that I've tried... */
	gnome_triggers_vdo ("", "generic", NULL);

	/* run dialog and capture return values */
	ret = gtk_dialog_run (GTK_DIALOG (dialog));

	/* handle return values */
	get_check_button (check_button, checkValue);
	get_editable (entry, value);
	*_retval = (ret == GTK_RESPONSE_OK);

	/* done */
	gtk_widget_destroy (dialog);
	return NS_OK;
}

/**
 * CPromptService::PromptUsernameAndPassword: show a prompt for username
 * and password with an additional check button.
 */
NS_IMETHODIMP CPromptService::PromptUsernameAndPassword 
                                        (nsIDOMWindow *parent, 
					 const PRUnichar *dialogTitle,
					 const PRUnichar *text,
					 PRUnichar **username,
					 PRUnichar **password,
					 const PRUnichar *checkMsg,
					 PRBool *checkValue,
					 PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *check_button;
	GtkWidget *label;
	GtkWidget *username_entry;
	GtkWidget *password_entry;
	GtkWidget *gparent;
	GladeXML *gxml;
	gint ret;

	/* build and show the dialog */
	gxml = ephy_glade_widget_new ("prompts.glade", "prompt_user_pass_dialog", 
				 &dialog, NULL);
	check_button = glade_xml_get_widget (gxml, "check_button");
	label = glade_xml_get_widget (gxml, "label");
	username_entry = glade_xml_get_widget (gxml, "username_entry");
	password_entry = glade_xml_get_widget (gxml, "password_entry");
	g_object_unref (G_OBJECT (gxml));

	/* parent the dialog */
	gparent = MozillaFindGtkParent (parent);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (gparent));

	/* set dynamic attributes */
	set_title (dialog, dialogTitle);
	set_check_button_text (check_button, checkMsg);
	set_editable (username_entry, username);
	set_editable (password_entry, password);
	set_check_button (check_button, checkValue);
	set_label_text (label, text);
	set_check_button_size_to_label (check_button, label);
	
	/* make a suitable sound */
	gnome_triggers_vdo ("", "question", NULL);

	/* run dialog and capture return values */
	ret = gtk_dialog_run (GTK_DIALOG (dialog));

	/* handle return values */
	get_check_button (check_button, checkValue);
	get_editable (username_entry, username);
	get_editable (password_entry, password);
	*_retval = (ret == GTK_RESPONSE_OK);

	/* done */
	gtk_widget_destroy (dialog);
	return NS_OK;
}

/**
 * CPromptService::PromptPassword: show a prompt for just
 * a password with an additional check button.
 */
NS_IMETHODIMP CPromptService::PromptPassword (nsIDOMWindow *parent, 
					      const PRUnichar *dialogTitle,
					      const PRUnichar *text,
					      PRUnichar **password,
					      const PRUnichar *checkMsg, 
					      PRBool *checkValue,
					      PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *check_button;
	GtkWidget *password_entry;
	GtkWidget *gparent;
	GladeXML *gxml;
	gint ret;

	/* build and show the dialog */
	gxml = ephy_glade_widget_new ("prompts.glade", "prompt_pass_dialog", 
				 &dialog, NULL);
	check_button = glade_xml_get_widget (gxml, "check_button");
	label = glade_xml_get_widget (gxml, "label");
	password_entry = glade_xml_get_widget (gxml, "password_entry");
	g_object_unref (G_OBJECT (gxml));

	/* parent the dialog */
	gparent = MozillaFindGtkParent (parent);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (gparent));

	/* set dynamic attributes */
	set_title (dialog, dialogTitle);
	set_check_button_text (check_button, checkMsg);
	set_editable (password_entry, password);
	set_check_button (check_button, checkValue);
	set_label_text (label, text);
	set_check_button_size_to_label (check_button, label);

	/* make a suitable sound */
	gnome_triggers_vdo ("", "question", NULL);

	/* run dialog and capture return values */
	ret = gtk_dialog_run (GTK_DIALOG (dialog));

	/* handle return values */
	get_check_button (check_button, checkValue);
	get_editable (password_entry, password);
	*_retval = (ret == GTK_RESPONSE_OK);

	/* done */
	gtk_widget_destroy (dialog);
	return NS_OK;
}


/**
 * CPromptService::Select:
 */
NS_IMETHODIMP CPromptService::Select (nsIDOMWindow *parent, 
				      const PRUnichar *dialogTitle,
				      const PRUnichar *text, PRUint32 count,
				      const PRUnichar **selectList, 
				      PRInt32 *outSelection,
				      PRBool *_retval)
{
	GtkWidget *dialog;
	GtkWidget *gparent;
	GtkWidget *label;
	GladeXML *gxml;
	GtkWidget *treeview;
	gint ret;
	
	/* build and show the dialog */
	gxml = ephy_glade_widget_new ("prompts.glade", "select_dialog", 
				 &dialog, NULL);
	treeview = glade_xml_get_widget (gxml, "treeview");
	label   = glade_xml_get_widget (gxml, "label");
	g_object_unref (G_OBJECT (gxml));

	/* parent the dialog */
	gparent = MozillaFindGtkParent (parent);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (gparent));

	/* set dynamic attributes */
	set_title (dialog, dialogTitle);
	set_label_text (label, text);

	/* setup treeview */
        GtkCellRenderer *renderer;
        GtkListStore *liststore;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

        gtk_tree_view_set_reorderable (GTK_TREE_VIEW(treeview), TRUE);

        liststore = gtk_list_store_new (2,
                                        G_TYPE_STRING,
					G_TYPE_INT);

        model = GTK_TREE_MODEL (liststore);

        gtk_tree_view_set_model (GTK_TREE_VIEW(treeview), 
                                 model);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(treeview),
                                           FALSE);

        renderer = gtk_cell_renderer_text_new ();

        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW(treeview),
                                                     0, "Items",
                                                     renderer,
                                                     "text", 0,
						     NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeview));
	
	for (PRUint32 i = 0 ; i < count ; i++)
	{
		char *itemText = 
			ToNewCString(NS_ConvertUCS2toUTF8 (selectList[i])); 
        	gtk_list_store_append (GTK_LIST_STORE (model), 
                               	       &iter);
        	gtk_list_store_set (GTK_LIST_STORE (model),
                            	    &iter,
                            	    0, itemText,
				    1, i,
                            	    -1);
		nsMemory::Free(itemText);
	}

	gtk_tree_model_get_iter_first (model, &iter);
	gtk_tree_selection_select_iter (selection, &iter);

	/* make a suitable sound */
	gnome_triggers_vdo ("", "question", NULL);

	/* run dialog and capture return values */
	ret = gtk_dialog_run (GTK_DIALOG (dialog));

	/* handle return values */
	if (gtk_tree_selection_get_selected (selection,
					     &model,
					     &iter))
	{
		GValue val = {0, };
		gtk_tree_model_get_value (model, &iter, 1, &val);
        	*outSelection = g_value_get_int (&val);

		*_retval = (ret == GTK_RESPONSE_OK) ? PR_TRUE : PR_FALSE;
	}
	else
	{
		*_retval = PR_FALSE;
	}

	gtk_widget_destroy (dialog);

	return NS_OK;
}

NS_DEF_FACTORY (CPromptService, CPromptService);

/**
 * NS_NewPromptServiceFactory:
 */ 
nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
	NS_ENSURE_ARG_POINTER(aFactory);
	*aFactory = nsnull;

	nsCPromptServiceFactory *result = new nsCPromptServiceFactory;
	if (result == NULL)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}
    
	NS_ADDREF(result);
	*aFactory = result;
  
	return NS_OK;
}

/**
 * set_title: set a dialog title to a unicode string
 */
static void
set_title (GtkWidget *dialog, const PRUnichar *title)
{
	const nsACString &utf8string = NS_ConvertUCS2toUTF8 (title);

	/* set it */
	gtk_window_set_title (GTK_WINDOW (dialog), 
			      (title == NULL ? N_("Epiphany") :
			      PromiseFlatCString(utf8string).get()));
}

/**
 * set_label_text: set a labels text to a unicode string
 */
static void
set_label_text (GtkWidget *label, const PRUnichar *text)
{
	const nsACString &utf8string = NS_ConvertUCS2toUTF8 (text);
	
	/* set it */
	gtk_label_set_text (GTK_LABEL (label),
			    PromiseFlatCString(utf8string).get());
}

/**
 * set_check_button_text: set a check buttons text to a unicode string
 */
static void
set_check_button_text (GtkWidget *check_button, const PRUnichar *text)
{
	const nsACString &utf8string = NS_ConvertUCS2toUTF8 (text);
	
	/* set it */
	gtk_label_set_text (GTK_LABEL (GTK_BIN (check_button)->child),
			    PromiseFlatCString(utf8string).get());
}
	
/**
 * set_check_button: set a togglebutton to an initial state
 */
static void
set_check_button (GtkWidget *button, PRBool *value)
{
	/* check pointer is valid */
	if (value == NULL)
	{
		gtk_widget_hide (GTK_WIDGET (button));
		return;
	}

	/* set the value of the check button */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), *value);
}

/**
 * se_check_button_size_to_label: sync text widgets sizes
 */
static void
set_check_button_size_to_label (GtkWidget *check_button,
			        GtkWidget *label)
{
	GtkRequisition r, label_r;

	gtk_widget_size_request (check_button, &r);
	gtk_widget_size_request (label, &label_r);

	if (r.width <= label_r.width) return;
	
	gtk_widget_set_size_request (label, r.width, -1);
}

/**
 * set_editable: set an editable to a unicode string
 */
static void
set_editable (GtkWidget *entry, PRUnichar **text)
{
	const nsACString &utf8string = NS_ConvertUCS2toUTF8 (*text);

	/* set this string value in the widget */
	gtk_entry_set_text (GTK_ENTRY (entry),
			    PromiseFlatCString(utf8string).get());
}

/**
 * get_check_button: get value of a toggle button and store it in a PRBool
 */
static void
get_check_button (GtkWidget *button, PRBool *value)
{
	/* check we can write */
	if (value == NULL)
	{
		return;
	}

	/* set the value from the check button */
	*value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
}

/**
 * get_editable: get a string from an editable and store it as unicode
 */
static void
get_editable (GtkWidget *editable, PRUnichar **text)
{
	char *edited;
	
	/* check we can write */
	if (text == NULL)
	{
		return;
	}

	/* get the text */
	edited = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);

	/* decode and set it as the return value */
	*text = ToNewUnicode(NS_ConvertUTF8toUCS2(edited));
}
