/*
 *  Copyright (C) 2003 Tommi Komulainen <tommi.komulainen@iki.fi>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
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

#ifndef COOKIEPROMPTSERVICE_H
#define COOKIEPROMPTSERVICE_H

#include <nsICookiePromptService.h>

/* 50766a18-0b34-41d9-8f6c-4612200e6556 */
#define EPHY_COOKIEPROMPTSERVICE_CID \
  { 0x50766a18, 0x0b34, 0x41d9, { 0x8f, 0x6c, 0x46, 0x12, 0x20, 0x0e, 0x65, 0x56 } }

#define EPHY_COOKIEPROMPTSERVICE_CLASSNAME 	"Epiphany Cookie Prompt Service"
#define EPHY_COOKIEPROMPTSERVICE_CONTRACTID	"@mozilla.org/embedcomp/cookieprompt-service;1"

class GeckoCookiePromptService : public nsICookiePromptService {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICOOKIEPROMPTSERVICE

    GeckoCookiePromptService();

  private:
    ~GeckoCookiePromptService();
};

#endif /* COOKIEPROMPTSERVICE_H */
