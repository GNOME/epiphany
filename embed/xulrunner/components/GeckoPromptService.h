/*
 *  Copyright © 2005, 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef GECKO_PROMPT_SERVICE_H
#define GECKO_PROMPT_SERVICE_H

#include <nsIPromptService.h>
#include <nsIPromptService2.h>
#include <nsINonBlockingAlertService.h>

#define GECKO_PROMPT_SERVICE_CID				\
{ /* cadc6035-7c53-4039-823b-004a289d5eb2 */			\
  0xcadc6035, 0x7c53, 0x4039,					\
  { 0x82, 0x3b, 0x00, 0x4a, 0x28, 0x9d, 0x5e, 0xb2 } }

#define GECKO_PROMPT_SERVICE_CLASSNAME	"Gecko Prompt Service"

class GeckoPromptService : public nsIPromptService2,
                           public nsINonBlockingAlertService
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROMPTSERVICE
    NS_DECL_NSIPROMPTSERVICE2
    NS_DECL_NSINONBLOCKINGALERTSERVICE

    GeckoPromptService();
    virtual ~GeckoPromptService();
};

#endif /* GECKO_PROMPT_SERVICE_H */
