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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_PROMPT_SERVICE_H
#define EPHY_PROMPT_SERVICE_H

#ifdef HAVE_GECKO_1_9
#include <nsIPromptService2.h>
#else
#include <nsIPromptService.h>
#endif

#if HAVE_NSINONBLOCKINGALERTSERVICE_H
#include <nsINonBlockingAlertService.h>
#endif

#define EPHY_PROMPT_SERVICE_IID				\
{ /* 6e8b90d4-78a6-41c5-98da-b1559a40d30d */		\
  0x6e8b90d4, 0x78a6, 0x41c5,				\
  { 0x98, 0xda, 0xb1, 0x55, 0x9a, 0x40, 0xd3, 0x0d } }

#define EPHY_PROMPT_SERVICE_CLASSNAME	"Epiphany Prompt Service"

class EphyPromptService :
#ifdef HAVE_GECKO_1_9
                          public nsIPromptService2
#else
                          public nsIPromptService
#endif
#if HAVE_NSINONBLOCKINGALERTSERVICE_H
			, public nsINonBlockingAlertService
#endif
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROMPTSERVICE
#ifdef HAVE_GECKO_1_9
        NS_DECL_NSIPROMPTSERVICE2
#endif
#if HAVE_NSINONBLOCKINGALERTSERVICE_H
	NS_DECL_NSINONBLOCKINGALERTSERVICE
#endif

	EphyPromptService();
	virtual ~EphyPromptService();

 protected:
#ifdef HAVE_GECKO_1_9
	static nsresult PromptPasswordAdapter(nsIPromptService* aService,
					      nsIDOMWindow* aParent,
					      nsIChannel* aChannel,
					      PRUint32 aLevel,
					      nsIAuthInformation* aAuthInfo,
					      const PRUnichar* aCheckLabel,
					      PRBool* aCheckValue,
					      PRBool* retval);
#endif /* HAVE_GECKO_1_9 */
};

#endif /* EPHY_PROMPT_SERVICE_H */
