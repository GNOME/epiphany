#include <nsIPrintSettings.h>
#include <nsIDOMWindow.h>

#include "print-dialog.h"

GtkWidget *MozillaFindEmbed	(nsIDOMWindow *aDOMWindow);

GtkWidget *MozillaFindGtkParent (nsIDOMWindow *aDOMWindow);

NS_METHOD MozillaCollatePrintSettings (const EmbedPrintInfo *info,
				       nsIPrintSettings *settings,
				       gboolean preview);
