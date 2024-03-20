This document includes guidelines for submitting issue reports. This is kept
very short, so please read it before reporting your issue.

For tips for hacking on Epiphany, see the HACKING.md file.

# Software versions

Please include the following in your issue report:

 * Epiphany version
 * WebKitGTK version
 * Operating system and version

Check the About dialog if you're not sure what Epiphany or WebKitGTK versions
you have.

# Web Content Bugs

This is the **wrong place** to report bugs with web content (e.g. incorrect page
rendering, broken JavaScript, problems with video playback, font issues, network
issues, web inspector issues, or generally anything at all wrong with a
website). These problems should all be reported directly on
[WebKit Bugzilla](https://bugs.webkit.org/enter_bug.cgi?product=WebKit&component=WebKitGTK)
instead. Be careful to select the WebKitGTK Bugzilla component when reporting the
issue to ensure the right developers see your bug report. Don't forget! Please also
add the `[GTK]` prefix to the title of your bug.

In general, only problems with the GTK user interface around the web content
view (e.g. menus, preferences dialog, window chrome, history, bookmarks, tabs)
or Epiphany features (e.g. Firefox Sync, adblocker, password manager, web apps)
should be reported on Epiphany's GitLab issue tracker.

Don't worry if you accidentally submit your report in the wrong place. This
happens all the time, since it's sometimes difficult to guess whether Epiphany
or WebKit is responsible for a bug. If we suspect an issue reported on the
Epiphany issue tracker is actually a WebKit bug, we will close it and ask you to
report the issue on WebKit Bugzilla instead.

# Crashes

If Epiphany crashed, then we really need a backtrace taken in gdb with `bt full`
in order to solve the problem. Be sure to install the necessary debuginfo
packages for all frames that appear in the crashing thread.
[Learn how to include a good backtrace.](https://handbook.gnome.org/issues/stack-traces.html)

If you see the message "Oops! Something went wrong while displaying this page,"
that means WebKit has crashed. Please follow the steps above to take a quality
backtrace for the WebKitWebProcess and to report it on WebKit Bugzilla (not on
Epiphany's GitLab).

We always appreciate crash reports that include a quality backtrace. Crash
reports without a useful backtrace will be closed.
