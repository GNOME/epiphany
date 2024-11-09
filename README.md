# GNOME Web

GNOME Web (codename: Epiphany) is a GNOME web browser based on
[the WebKit rendering engine](https://webkit.org/). The codename means "a
usually sudden manifestation or perception of the essential nature or meaning of
something" ([Merriam-Webster](https://www.merriam-webster.com/dictionary/epiphany)).

Epiphany is opinionated.

## Download and Install

Epiphany is designed for Linux systems. The recommended way to install Epiphany
is via [Flatpak](https://www.flatpak.org/). You may:

 * [Download the latest stable version from Flathub](https://flathub.org/apps/details/org.gnome.Epiphany)
   (recommended).
 * [Download Epiphany Technology Preview](https://nightly.gnome.org/repo/appstream/org.gnome.Epiphany.Devel.flatpakref)
   if you are adventurous and want to help test tomorrow's Epiphany today. It
   is **not stable**.

 * Download Epiphany Canary if you are even more adventurous and want to help
   test the most recent development versions of WebKitGTK and Epiphany. This
   flavor of Epiphany is more likely to be **very unstable** because the code
   being built comes directly from WebKit's git main branch and Epiphany's git
   master branch. Epiphany Canary can be installed by executing the following
   flatpak command in a terminal:

   ```shell
   flatpak --user install https://nightly.gnome.org/repo/appstream/org.gnome.Epiphany.Canary.flatpakref
   ```

Epiphany is probably also available via your operating system's package manager,
but such packages are often outdated and insecure. Flatpak is the best
application distribution mechanism for Linux.

## Building from Source

### The Easy Way

The recommended way to build Epiphany locally is using the flatpak-builder
manifest with GNOME Builder. After installing Builder and launching it, you'll
see the Select a Project page. Select Open, then select the toplevel Epiphany
directory. Builder will detect the org.gnome.Epiphany.json flatpak-builder
manifest and you will be able to build the project in Builder. All required
dependencies will be provided by the manifest.

### Building Manually

Epiphany uses the [Meson build system](http://mesonbuild.com/). You can build
Epiphany the same way you would any software that uses Meson. For example:

```
$ mkdir build && cd build
$ meson ..
$ ninja
$ sudo ninja install
```

You will have to install several pkg-config dependencies. If you are missing a
dependency, meson will present an error that looks like this:

```
meson.build:84:0: ERROR:  Native dependency 'hogweed' not found
```

In RPM-based distributions, you can install the missing dependencies
automatically. For example, in Fedora:

```
$ sudo dnf install 'pkgconfig(hogweed)'
```

In deb-based distributions:

```
$ sudo apt install $(apt-file search --package-only hogweed)
```

In other distributions, you must research each dependency to determine which
package provides the required pkg-config file.

### Rebuilding Dependencies

Newcomers should try to avoid bugs that require making changes in WebKit or
other dependencies of Epiphany. Once you've contributed a few easier patches to
Epiphany and are ready to start making code changes in WebKit or other
dependencies, then your development setup becomes a bit more complicated. You
are welcome to use whatever method you prefer for development, but the
recommended method is to use either [Toolbox](https://containertoolbx.org/) or
[JHBuild](https://gnome.pages.gitlab.gnome.org/jhbuild/index.html). Here is an
example of how to build your own WebKit using JHBuild, assuming your JHBuild
installation prefix is `$HOME/jhbuild install/`:

```
$ git clone https://github.com/WebKit/WebKit.git WebKit
$ mkdir -p WebKit/WebKitBuild/GNOME
$ cd WebKit/WebKitBuild/GNOME
$ jhbuild run cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPORT=GTK -DDEVELOPER_MODE=ON -DCMAKE_INSTALL_PREFIX=$HOME/jhbuild/install/ -DCMAKE_INSTALL_LIBDIR=lib -GNinja ../..
$ jhbuild run ninja
$ jhbuild run cmake -P cmake_install.cmake
```

## Manifesto

A web browser is more than an application: it is a way of thinking, a way of
seeing the world. Epiphany's principles are simplicity, standards compliance,
and software freedom.

### Simplicity

Feature bloat and user interface clutter is evil.

Epiphany aims to present the simplest interface possible for a browser. Simple
does not necessarily mean less-powerful. The commonly-used browsers of today are
too big, buggy, and bloated. Epiphany is a small browser designed for the web:
not for mail, newsgroups, file management, instant messaging, or coffeemaking.
The UNIX philosophy is to design small tools that do one thing and do it well.

### Standards Compliance

The introduction of nonstandard features in browsers could make it difficult
or impossible to use alternative products like Epiphany if developers embrace
them. Alternative standards-complying browsers might not be able to fully access
websites making use of these features. The success of nonstandard features can
ultimately lead one browser to dominate the market.

Standards compliance ensures the freedom of choice. Epiphany aims to achieve
this.

### Software Freedom

Epiphany is not just free of cost; more importantly, the source code is made
available to you under a license that [respects your freedom](https://www.gnu.org/philosophy/philosophy.html).

Just as GNOME exists to oppose proprietary desktop software, Epiphany opposes
the dominance of the web by proprietary software web browsers. Today's chief
offender is Google Chrome, a browser that purports to be open source, yet
actually includes several proprietary components. In contrast, Epiphany is fully
free software.

## Human Interface

Epiphany follows the [GNOME Human Interface Guidelines](https://developer.gnome.org/hig).
Unless there are serious reasons to make an exception, not following the
guidelines will be considered a bug.

### GNOME Integration

Epiphany's main goal is to be integrated with GNOME, as well as similar
desktops (notably elementary OS). We don't aim to make Epiphany usable outside
these environments.

### Preferences

We are cautious about adding new preferences. Preferences can be added when they
make sense, but they should always be carefully-considered.
[Preferences come with a cost](https://ometer.com/preferences.html).

### Target Audience

We target nontechnical users by design. This happens to be 90% of the user
population. Technical details should not be exposed in the interface.

We target web users, not web developers. A few geek-oriented features, like the
web inspector, are welcome so long as they are non-obtrusive.

## Website

[Epiphany has a website,](https://apps.gnome.org/Epiphany) though there is not
very much content there.

## Contact Us

The recommended way to contact us is via the
[Epiphany discussion forum](https://discourse.gnome.org/tag/epiphany).
