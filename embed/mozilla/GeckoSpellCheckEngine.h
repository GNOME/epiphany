/*
 *  Copyright © 2006 Christian Persch
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

#ifndef GECKO_SPELL_CHECK_ENGINE_H
#define GECKO_SPELL_CHECK_ENGINE_H

#include <nsCOMPtr.h>
#include <mozISpellCheckingEngine.h>

#include "ephy-spell-check.h"

class mozIPersonalDictionary;

/* 26948b8b-d136-4a78-a9c5-3a145812b649 */
#define GECKO_SPELL_CHECK_ENGINE_IID \
{ 0x26948b8b, 0xd136, 0x4a78, { 0xa9, 0xc5, 0x3a, 0x14, 0x58, 0x12, 0xb6, 0x49 } }

#define GECKO_SPELL_CHECK_ENGINE_CONTRACTID "@mozilla.org/spellchecker/myspell;1"
#define GECKO_SPELL_CHECK_ENGINE_CLASSNAME "Gecko Print Settings"

class GeckoSpellCheckEngine : public mozISpellCheckingEngine
{
  public:
    GeckoSpellCheckEngine();
    virtual ~GeckoSpellCheckEngine();

    NS_DECL_ISUPPORTS
    NS_DECL_MOZISPELLCHECKINGENGINE

  private:
    nsCOMPtr<mozIPersonalDictionary> mPersonalDictionary;
    EphySpellCheck *mSpeller;
};

#endif /* GECKO_SPELL_CHECK_ENGINE_H */
