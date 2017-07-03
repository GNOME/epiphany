Epiphany is a GNOME web browser based on the WebKit rendering engine.
The name meaning:
"An intuitive grasp of reality through
something (as an event) usually simple and striking"

# MANIFESTO

A web browser is more than an application, it is a way of thinking, it is
a way of seeing the world. Epiphany's principles are simplicity and standards
compliance.

# Simplicity:

Epiphany aims to utilize the simplest interface possible for a browser.  Keep
in mind that simple does not necessarily mean less powerful.  We believe
the commonly used browsers of today are too big, buggy, and bloated. Epiphany
addresses simplicity with a small browser designed for the web -- not mail,
newsgroups, file management, instant messenging or coffee making.  The UNIX
philosophy is to design small tools that do one thing, and do it well.

Epiphany also address simplicity with modularity to make a light and powerful
application.  If something can be implemented using external applications
or components, we use it rather than wasting resources in the web browser.
Integration with other desktop applications can also be achieved using DBus
and the ever popular command line.

Other tasks like mail can be handled with your favorite e-mail application
(Evolution, pine, mutt, balsa, pronto, whatever).

# Standards compliance:

The introduction of non-standard features in browsers could make it difficult
or impossible to use alternative products like Epiphany if developers embrace
them. Alternative (standards complying) browsers could not be able to
fully access web sites making use of these features.  The success of
non-standard features can ultimately lead to forcing one browser, on
one platform to dominate the market.

Standards compliance ensures the freedom of choice.  Epiphany aims to achieve
this.

# USER INTERFACE LINES

- HIG compliance

Epiphany is going to follow latest version of the GNOME Human Interface
Guidelines. Unless there are very serious reasons to make an exception not
following it will be considered a bug.
"I follow the HIG only when I like it" is not a legitimate approach.
Any areas where we diverge from the HIG will communicated
to the HIG team for future consideration.

- GNOME integration

Epiphany's main goal is to be integrated with the GNOME desktop.
We dont aim to make epiphany usable outside GNOME. If someone will like
to use it anyway, it's just a plus. Ex: Making people happy that
don't have control center installed is not a good reason
to have mime configuration in epiphany itself.

- Simple design

Feature bloat and user interface clutter is evil :)

- Preferences

We will follow the new GNOME policy about preferences.
I think Havoc Pennington already explained it a lot
better than I could ever do.
http://www106.pair.com/rhp/free-software-ui.html

- User target

We target non-technical users by design.
This happens to be 90% of the user population.
(Technical details should not exposed in the interface)
We target web users, we dont directly target web developers.
A few geek-oriented feautures can be kept as
long as they are non-obtrusive.

# REQUIREMENTS

You will need a complete installation of GNOME 2.x desktop.
The required WebKit version will be specified in the release notes.

# HOW TO HELP

You can report new bugs at
 http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany
And feel free to send patches.

About new feautures I'll just quote Metacity FAQ.

Q: Will you add my feature?

A: If it makes sense to turn on unconditionally,
   or is genuinely a harmless preference that I would not
   be embarrassed to put in a simple, uncluttered, user-friendly
   configuration dialog.

   If the only rationale for your feature is that other
   [browsers] have it, or that you are personally used to it, or something
   like that, then I will not be impressed. [Epiphany] is firmly in the
   "choose good defaults" camp rather than the "offer 6 equally broken
   ways to do it, and let the user pick one" camp.

   Don't let this discourage patches and fixes - I love those. ;-)
   Just be prepared to hear the above objections if your patch
   adds some crack-ridden configuration option.

# CONTACTS

Epiphany Mailing List <epiphany-list@gnome.org>
