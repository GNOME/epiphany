/*
 *  Copyright (C) 2005, 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_PROMPT_SERVICE_H
#define EPHY_PROMPT_SERVICE_H

#include <nsIPromptService.h>

#if HAVE_NSINONBLOCKINGALERTSERVICE_H
#include <nsINonBlockingAlertService.h>
#endif

#define EPHY_PROMPT_SERVICE_IID				\
{ /* 6e8b90d4-78a6-41c5-98da-b1559a40d30d */		\
  0x6e8b90d4, 0x78a6, 0x41c5,				\
  { 0x98, 0xda, 0xb1, 0x55, 0x9a, 0x40, 0xd3, 0x0d } }

#define EPHY_PROMPT_SERVICE_CLASSNAME	"Epiphany Prompt Service"

class EphyPromptService : public nsIPromptService
#if HAVE_NSINONBLOCKINGALERTSERVICE_H
			, public nsINonBlockingAlertService
#endif
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIPROMPTSERVICE
#if HAVE_NSINONBLOCKINGALERTSERVICE_H
	NS_DECL_NSINONBLOCKINGALERTSERVICE
#endif

	EphyPromptService();
	virtual ~EphyPromptService();
};

#endif /* EPHY_PROMPT_SERVICE_H */
