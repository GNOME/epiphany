/*
 *  Copyright © 2009 Igalia S.L.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/* Portions of this file based on Chromium code.
 * License block as follows:
 *
 * Copyright (c) 2009 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The LICENSE file from Chromium can be found in the LICENSE.chromium
 * file.
 */

#include "config.h"

#include "ephy-nss-glue.h"

#include "ephy-file-helpers.h"

#include <nss.h>
#include <pk11pub.h>
#include <pk11sdr.h>
#include <glib/gi18n.h>

static gboolean nss_initialized = FALSE;
static PK11SlotInfo *db_slot = NULL;

static char*
ask_for_nss_password (PK11SlotInfo *slot,
                      PRBool retry,
                      void *arg)
{
  GtkWidget *dialog;
  GtkWidget *entry;
  gint result;
  char *password = NULL;

  if (retry)
    return NULL;

  dialog = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_OK_CANCEL,
                                   _("Master password needed"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("The passwords from the previous version are locked with a master password. If you want to import them, please enter your master password below."));
  entry = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                     entry);
  gtk_widget_show (entry);

  result = gtk_dialog_run (GTK_DIALOG (dialog));

  switch (result) {
  case GTK_RESPONSE_OK:
    password = PL_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
    break;
  default:
    break;
  }

  gtk_widget_destroy (dialog);
  return password;
}

gboolean ephy_nss_glue_init ()
{
  char *config_dir, *modspec;
  SECStatus rv;

  config_dir = g_build_filename (ephy_dot_dir (),
                                 "mozilla", "epiphany",
                                 NULL);
  rv = NSS_Init (config_dir);

  if (rv < 0) {
    g_free (config_dir);
    return FALSE;
  }

  modspec = g_strdup_printf ("configDir='%s' tokenDescription='Firefox NSS database' "
                             "flags=readOnly", config_dir);
  db_slot = SECMOD_OpenUserDB (modspec);
  g_free (config_dir);
  g_free (modspec);

  if (!db_slot)
    return FALSE;

  /* It's possibly to set a master password for NSS through the
     certificate manager extension, so we must support that too */
  PK11_SetPasswordFunc (ask_for_nss_password);

  nss_initialized = TRUE;

  return TRUE;
}

void ephy_nss_glue_close ()
{
  if (db_slot) {
    PK11_FreeSlot (db_slot);
    db_slot = NULL;
  }

  PK11_SetPasswordFunc (NULL);

  NSS_Shutdown ();

  nss_initialized = FALSE;
}

typedef struct SDRResult
{
  SECItem keyid;
  SECAlgorithmID alg;
  SECItem data;
} SDRResult;

static SEC_ASN1Template g_template[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof (SDRResult) },
  { SEC_ASN1_OCTET_STRING, offsetof(SDRResult, keyid) },
  { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(SDRResult, alg),
    SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
  { SEC_ASN1_OCTET_STRING, offsetof(SDRResult, data) },
  { 0 }
};

static SECStatus
unpadBlock(SECItem *data, int blockSize, SECItem *result)
{
  SECStatus rv = SECSuccess;
  int padLength;
  int i;

  result->data = 0;
  result->len = 0;

  /* Remove the padding from the end if the input data */
  if (data->len == 0 || data->len % blockSize  != 0) { rv = SECFailure; goto loser; }

  padLength = data->data[data->len-1];
  if (padLength > blockSize) { rv = SECFailure; goto loser; }

  /* verify padding */
  for (i=data->len - padLength; (uint32)i < data->len; i++) {
    if (data->data[i] != padLength) {
        rv = SECFailure;
        goto loser;
    }
  }

  result->len = data->len - padLength;
  result->data = (unsigned char *)PORT_Alloc(result->len);
  if (!result->data) { rv = SECFailure; goto loser; }

  PORT_Memcpy(result->data, data->data, result->len);

  if (padLength < 2) {
    /* Chromium returns an error here, but it seems to be harmless and
       if we continue we'll be able to import the password
       correctly */
    /* return SECWouldBlock; */
  }

loser:
  return rv;
}

static SECStatus
pk11Decrypt (PK11SlotInfo *slot, PLArenaPool *arena, 
             CK_MECHANISM_TYPE type, PK11SymKey *key, 
             SECItem *params, SECItem *in, SECItem *result)
{
  PK11Context *ctx = 0;
  SECItem paddedResult;
  SECStatus rv;

  paddedResult.len = 0;
  paddedResult.data = 0;

  ctx = PK11_CreateContextBySymKey (type, CKA_DECRYPT, key, params);
  if (!ctx) {
    rv = SECFailure;
    goto loser;
  }

  paddedResult.len = in->len;
  paddedResult.data = (unsigned char*)PORT_ArenaAlloc (arena, paddedResult.len);

  rv = PK11_CipherOp (ctx, paddedResult.data, 
                      (int*)&paddedResult.len, paddedResult.len,
                      in->data, in->len);
  if (rv != SECSuccess)
    goto loser;

  PK11_Finalize(ctx);

  /* Remove the padding */
  rv = unpadBlock (&paddedResult, PK11_GetBlockSize(type, 0), result);
  if (rv)
    goto loser;

loser:
  if (ctx)
    PK11_DestroyContext (ctx, PR_TRUE);

  return rv;
}

static SECStatus
PK11SDR_DecryptWithSlot (PK11SlotInfo *slot, SECItem *data, SECItem *result, void *cx)
{
  SECStatus rv = SECSuccess;
  PK11SymKey *key = NULL;
  CK_MECHANISM_TYPE type;
  SDRResult sdrResult;
  SECItem *params = NULL;
  SECItem possibleResult = { (SECItemType)0, NULL, 0 };
  PLArenaPool *arena = NULL;

  arena = PORT_NewArena (SEC_ASN1_DEFAULT_ARENA_SIZE);
  if (!arena) {
    rv = SECFailure;
    goto loser;
  }

  /* Decode the incoming data */
  memset (&sdrResult, 0, sizeof sdrResult);
  rv = SEC_QuickDERDecodeItem (arena, &sdrResult, g_template, data);
  if (rv != SECSuccess)
    goto loser; /* Invalid format */

  /* Get the parameter values from the data */
  params = PK11_ParamFromAlgid (&sdrResult.alg);
  if (!params) {
    rv = SECFailure;
    goto loser;
  }

  /* Use triple-DES (Should look up the algorithm) */
  type = CKM_DES3_CBC;
  key = PK11_FindFixedKey (slot, type, &sdrResult.keyid, cx);
  if (!key) { 
    rv = SECFailure;  
  } else {
    rv = pk11Decrypt (slot, arena, type, key, params, 
                      &sdrResult.data, result);
  }

 loser:
  if (arena)
    PORT_FreeArena (arena, PR_TRUE);

  if (key)
    PK11_FreeSymKey(key);

  if (params)
    SECITEM_ZfreeItem(params, PR_TRUE);

  if (possibleResult.data)
    SECITEM_ZfreeItem(&possibleResult, PR_FALSE);

  return rv;
}

char * ephy_nss_glue_decrypt (const unsigned char *data, gsize length)
{
  char *plain = NULL;
  SECItem request, reply;
  SECStatus result;

  result = PK11_Authenticate (db_slot, PR_TRUE, NULL);
  if (result != SECSuccess)
    return NULL;

  request.data = (unsigned char*)data;
  request.len = length;
  reply.data = NULL;
  reply.len = 0;

  result = PK11SDR_DecryptWithSlot (db_slot, &request, &reply, NULL);
  if (result == SECSuccess)
    plain = g_strndup ((const char*)reply.data, reply.len);

  SECITEM_FreeItem (&reply, PR_FALSE);

  return plain;
}

