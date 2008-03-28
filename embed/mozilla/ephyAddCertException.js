/*
   1 # -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   2 # ***** BEGIN LICENSE BLOCK *****
   3 # Version: MPL 1.1/GPL 2.0/LGPL 2.1
   4 #
   5 # The contents of this file are subject to the Mozilla Public License Version
   6 # 1.1 (the "License"); you may not use this file except in compliance with
   7 # the License. You may obtain a copy of the License at
   8 # http://www.mozilla.org/MPL/
   9 #
  10 # Software distributed under the License is distributed on an "AS IS" basis,
  11 # WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  12 # for the specific language governing rights and limitations under the
  13 # License.
  14 #
  15 # The Original Code is mozilla.org code.
  16 #
  17 # The Initial Developer of the Original Code is
  18 # Netscape Communications Corporation.
  19 # Portions created by the Initial Developer are Copyright (C) 1998
  20 # the Initial Developer. All Rights Reserved.
  21 #
  22 # Contributor(s):
  23 #   Blake Ross <blake@cs.stanford.edu>
  24 #   David Hyatt <hyatt@mozilla.org>
  25 #   Peter Annema <disttsc@bart.nl>
  26 #   Dean Tessman <dean_tessman@hotmail.com>
  27 #   Kevin Puetz <puetzk@iastate.edu>
  28 #   Ben Goodger <ben@netscape.com>
  29 #   Pierre Chanial <chanial@noos.fr>
  30 #   Jason Eager <jce2@po.cwru.edu>
  31 #   Joe Hewitt <hewitt@netscape.com>
  32 #   Alec Flett <alecf@netscape.com>
  33 #   Asaf Romano <mozilla.mano@sent.com>
  34 #   Jason Barnabe <jason_barnabe@fastmail.fm>
  35 #   Peter Parente <parente@cs.unc.edu>
  36 #   Giorgio Maone <g.maone@informaction.com>
  37 #   Tom Germeau <tom.germeau@epigoon.com>
  38 #   Jesse Ruderman <jruderman@gmail.com>
  39 #   Joe Hughes <joe@retrovirus.com>
  40 #   Pamela Greene <pamg.bugs@gmail.com>
  41 #   Michael Ventnor <m.ventnor@gmail.com>
  42 #   Simon Bünzli <zeniko@gmail.com>
  43 #   Johnathan Nightingale <johnath@mozilla.com>
  44 #   Ehsan Akhgari <ehsan.akhgari@gmail.com>
  45 #   Dão Gottwald <dao@mozilla.com>
  46 #
  47 # Alternatively, the contents of this file may be used under the terms of
  48 # either the GNU General Public License Version 2 or later (the "GPL"), or
  49 # the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  50 # in which case the provisions of the GPL or the LGPL are applicable instead
  51 # of those above. If you wish to allow use of your version of this file only
  52 # under the terms of either the GPL or the LGPL, and not to allow others to
  53 # use your version of this file under the terms of the MPL, indicate your
  54 # decision by deleting the provisions above and replace them with the notice
  55 # and other provisions required by the GPL or the LGPL. If you do not delete
  56 # the provisions above, a recipient may use your version of this file under
  57 # the terms of any one of the MPL, the GPL or the LGPL.
*/

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function ephyAddCertExceptionService() { }

ephyAddCertExceptionService.prototype = {

    classDescription: "ephyAddCertException dialogue",
    contractID: "@gnome.org/epiphany/add-cert-exception;1",
    classID: Components.ID("{f32ede25-4135-4896-834a-303326c553d4}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.ephyAddCertException]),

    showAddCertExceptionDialog : function(aDocument) {
        var params = { exceptionAdded : false };

        try {
            /*
            switch (gPrefService.getIntPref("browser.ssl_override_behavior")) {
                case 2 : // Pre-fetch & pre-populate
                    params.prefetchCert = true;
                case 1 : // Pre-populate
                    params.location = aLocation.href;
            }*/
            params.location = aDocument.location.href;
            params.prefetchCert = true;
        } catch (e) {
            Cu.reportError("Couldn't get ssl_override pref: " + e);
        }

        window.openDialog('chrome://pippki/content/exceptionDialog.xul',
                          '','chrome,centerscreen,modal', params);

        // If the user added the exception cert, attempt to reload the page
        if (params.exceptionAdded)
            aDocument.location.reload();
    }
};

var component = [ephyAddCertExceptionService];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
