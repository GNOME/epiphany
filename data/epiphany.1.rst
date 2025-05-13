========
epiphany
========

------------------------------------------
A simple, clean, beautiful view of the web
------------------------------------------

:Date: May 2025
:Copyright: Copyright 2006-2025 GNOME Foundation
:Manual section: 1

SYNOPSIS
--------

|  **epiphany** [*OPTION*...] [*URL*...]

DESCRIPTION
-----------

**epiphany** is the codename of GNOME Web, the web browser designed for the
GNOME desktop. It offers first-class GNOME integration, using WebKitGTK for web
rendering.

OPTIONS
-------

``--new-window``

  Open a new browser window instead of a new tab

``-l``, ``--load-session``\ =\ *FILE*

  Load the given session state file

``-i``, ``--incognito-mode``

  Start an instance with read-only user data, for browsing without storing history on your computer

``-p``, ``--private-instance``

  Start a temporary instance without any user data, for testing

``-a``, ``--application-mode``\ =\ *BASENAME*

  Start a private instance in web application mode (requires passing either desktop file basename or ``--profile``)

``--automation-mode``

  Start a private instance for WebDriver control

``--profile``\ =\ *FILE*

  Start using a custom profile directory for user data

``--search``\ =\ *TERM*

  Initiate search with given search term

``-?``, ``--help``

  Show help options

``--version``

  Show version
  
Epiphany has a comprehensive help system.  Run the browser
and select ``Help`` from the menu.

AUTHOR
------

Epiphany has been developed by Marco Pesenti Gritti (in memoriam),
Christian Persch, Xan Lopez, Carlos Garcia Campos, Claudio Saavedra,
Michael Catanzaro, Jan-Michael Brummer, Alice Mikhaylenko,
Patrick Griffis, and many others.
