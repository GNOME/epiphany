#include <nsIPrintSettings.h>
#include <nsIDOMWindow.h>

#include "ephy-embed.h"

GtkWidget *MozillaFindEmbed	(nsIDOMWindow *aDOMWindow);

GtkWidget *MozillaFindGtkParent (nsIDOMWindow *aDOMWindow);

NS_METHOD MozillaCollatePrintSettings (const EmbedPrintInfo *info,
				       nsIPrintSettings *settings);
