/*
 *  Copyright © 2013 Red Hat, Inc.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PASSWORDS_DIALOG_H
#define PASSWORDS_DIALOG_H

G_BEGIN_DECLS

#define EPHY_TYPE_PASSWORDS_DIALOG (ephy_passwords_dialog_get_type ())
G_DECLARE_FINAL_TYPE (EphyPasswordsDialog, ephy_passwords_dialog, EPHY, PASSWORDS_DIALOG, GtkDialog);

EphyPasswordsDialog *ephy_passwords_dialog_new (void);

G_END_DECLS

#endif /* PASSWORDS_DIALOG_H */
