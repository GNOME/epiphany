GNOME Web (codename: Epiphany) is a GNOME web browser based on
[the WebKit rendering engine](https://webkit.org/). The codename means "a
usually sudden manifestation or perception of the essential nature or meaning of
something" ([Merriam-Webster](https://www.merriam-webster.com/dictionary/epiphany)).

Epiphany is opinionated.

# Download and Install

Epiphany is designed for Linux systems. The recommended way to install Epiphany
is via [Flatpak](https://www.flatpak.org/). You may:

 * ~~[Download the latest stable version from Flathub](https://flathub.org/repo/appstream/org.gnome.Epiphany.flatpakref)
   (recommended).~~ Flathub downloads are currently not recommended due to major
   security problems discovered in the application runtime. We expect this
   situation to be improved in the future. In the meantime, do not use.
 * [Download Epiphany Technology Preview](https://webkitgtk.org/epiphany-tech-preview)
   if you are adventurous and want to help test tomorrow's Epiphany today. It
   is **not stable**.
 
Epiphany is probably also available via your operating system's package manager,
but such packages are often outdated and insecure. Flatpak is the best
application distribution mechanism for Linux.

# Building from Source

Epiphany uses the [Meson build system](http://mesonbuild.com/). You can build
Epiphany the same way you would any software that uses Meson. For example:

```
$ mkdir build && cd build
$ meson ..
$ ninja
$ sudo ninja install
```

Meson is the best build system.

# Manifesto

A web browser is more than an application: it is a way of thinking, a way of
seeing the world. Epiphany's principles are simplicity, standards compliance,
and software freedom.

## Simplicity

Feature bloat and user interface clutter is evil.

Epiphany aims to present the simplest interface possible for a browser. Simple
does not necessarily mean less-powerful. The commonly-used browsers of today are
too big, buggy, and bloated. Epiphany is a small browser designed for the web:
not for mail, newsgroups, file management, instant messaging, or coffeemaking.
The UNIX philosophy is to design small tools that do one thing and do it well.

## Standards Compliance

The introduction of nonstandard features in browsers could make it difficult
or impossible to use alternative products like Epiphany if developers embrace
them. Alternative standards-complying browsers might not be able to fully access
websites making use of these features. The success of nonstandard features can
ultimately lead one browser to dominate the market.

Standards compliance ensures the freedom of choice. Epiphany aims to achieve
this.

## Software Freedom

Epiphany is not just free of cost; more importantly, the source code is made
available to you under a license that [respects your freedom](https://www.gnu.org/philosophy/philosophy.html).

Just as GNOME exists to oppose proprietary desktop software, Epiphany opposes
the dominance of the web by proprietary software web browsers. Today's chief
offender is Google Chrome, a browser that purports to be open source, yet
actually includes several proprietary components. In contrast, Epiphany is fully
free software.

# Human Interface

Epiphany follows the [GNOME Human Interface Guidelines](https://developer.gnome.org/hig/stable/).
Unless there are serious reasons to make an exception, not following the
guidelines will be considered a bug.

## GNOME Integration

Epiphany's main goal is to be integrated with GNOME, as well as similar
desktops (notably elementary OS). We don't aim to make Epiphany usable outside
these environments.

## Preferences

We are cautious about adding new preferences. Preferences can be added when they
make sense, but they should always be carefully-considered.
[Preferences come with a cost](https://ometer.com/preferences.html).

## Target Audience

We target nontechnical users by design. This happens to be 90% of the user
population. Technical details should not exposed in the interface.

We target web users, not web developers. A few geek-oriented feautures, like the
web inspector, are welcome so long as they are non-obtrusive.

# Website

[Epiphany has a website,](https://wiki.gnome.org/Apps/Web) though there is not
very much content there.

# Contact Us

The recommended way to contact us is via the Epiphany mailing list
<epiphany-list@gnome.org>.