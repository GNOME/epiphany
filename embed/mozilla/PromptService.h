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

#ifndef __PromptService_h
#define __PromptService_h

#include "nsError.h"

#define G_PROMPTSERVICE_CID			     \
{ /* c5a77759-a07a-4025-8f74-ae89153ee6c2 */         \
    0xc5a77759,                                      \
    0xa07a,                                          \
    0x4025,                                          \
    {0x8f, 0x74, 0xae, 0x89, 0x15, 0x3e, 0xe6, 0xc2} \
}

#define G_PROMPTSERVICE_CLASSNAME "Galeon's Prompt Service"
#define G_PROMPTSERVICE_CONTRACTID "@mozilla.org/embedcomp/prompt-service;1"

class nsIFactory;

extern nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory);

#endif
