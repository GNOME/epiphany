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
 *
 *  $Id$
 */

#ifndef __PrintingPromptService_h
#define __PrintingPromptService_h

#include "print-dialog.h"

#include <nscore.h>
#include <nsIPrintingPromptService.h>

#define G_PRINTINGPROMPTSERVICE_CID		     \
{ /* 5998a2d3-88ea-4c52-b4bb-4e7abd0d35e0 */         \
    0x5998a2d3,                                      \
    0x88ea,                                          \
    0x4c52,                                          \
    {0xb4, 0xbb, 0x4e, 0x7a, 0xbd, 0x0d, 0x35, 0xe0} \
}

#define G_PRINTINGPROMPTSERVICE_CLASSNAME "Epiphany's Printing Prompt Service"
#define G_PRINTINGPROMPTSERVICE_CONTRACTID "@mozilla.org/embedcomp/printingprompt-service;1"

class GPrintingPromptService : public nsIPrintingPromptService,
			       nsIWebProgressListener,
			       nsIPrintProgressParams
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTINGPROMPTSERVICE
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIPRINTPROGRESSPARAMS

  GPrintingPromptService();
  virtual ~GPrintingPromptService();
  /* additional members */

protected:
  EmbedPrintInfo *mPrintInfo;
};

#endif
