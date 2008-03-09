/*
 *  Copyright © 2005, 2006, 2008 Christian Persch
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
 */

#ifndef EPHY_LOGIN_PROMPTER_H
#define EPHY_LOGIN_PROMPTER_H

#include <nsILoginManagerPrompter.h>
#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>

#define EPHY_LOGIN_PROMPTER_CLASSNAME "Epiphany Login Prompter"

/* b005b95e-ef31-4214-bd0e-1dd4f2d3083a */
#define EPHY_LOGIN_PROMPTER_CID { 0xb005b95e, 0xef31, 0x4214, { 0xbd, 0x0e, 0x1d, 0xd4, 0xf2, 0xd3, 0x08, 0x3a } }

class EphyLoginPrompter : public nsILoginManagerPrompter
{
public:
	EphyLoginPrompter ();
	virtual ~EphyLoginPrompter ();

	NS_DECL_ISUPPORTS
	NS_DECL_NSILOGINMANAGERPROMPTER

private:
	nsCOMPtr<nsIDOMWindow> mWindow;
};

#endif /* !EPHY_LOGIN_PROMPTER_H */
