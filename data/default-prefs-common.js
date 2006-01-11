// mouse wheel: disable mozilla's ctrl+wheel zooming, and enable our own
// set numlines to -1 to reverse directions, see bug #306110
pref("mousewheel.withcontrolkey.action", 0);
pref("mousewheel.withcontrolkey.numlines", -1);
pref("mousewheel.withcontrolkey.sysnumlines", false);

// fix horizontal scroll with 2nd wheel, see bug #148557
pref("mousewheel.horizscroll.withnokey.action", 0);
pref("mousewheel.horizscroll.withnokey.sysnumlines", true);

// don't allow opening file:/// URLs on pages from network sources (http, etc.)
pref("security.checkloaduri", true);

// enable line wrapping in View Source
pref("view_source.wrap_long_lines", true);

// disable sidebar What's Related, we don't use it
pref("browser.related.enabled", false);

// Work around for mozilla focus bugs
pref("mozilla.widget.raise-on-setfocus", false);

// disable sucky XUL ftp view, have nice ns4-like html page instead
pref("network.dir.generate_html", true);

// deactivate mailcap support, it breaks Gnome-based helper apps
pref("helpers.global_mailcap_file", "");
pref("helpers.private_mailcap_file", "");

// use the mozilla defaults for mime.types files to let mozilla guess proper
// Content-Type for file uploads instead of always falling back to
// application/octet-stream
pref("helpers.global_mime_types_file", "");
pref("helpers.private_mime_types_file", "");

// enable keyword search
pref("keyword.enabled", true);

// disable usless security warnings
pref("security.warn_entering_secure", false);
pref("security.warn_entering_secure.show_once", true);
pref("security.warn_leaving_secure", false);
pref("security.warn_leaving_secure.show_once", false);
pref("security.warn_submit_insecure", false);
pref("security.warn_submit_insecure.show_once", false);
pref("security.warn_viewing_mixed", true);
pref("security.warn_viewing_mixed.show_once", false);

// fonts
pref("font.size.unit", "pt");

// protocols
pref("network.protocol-handler.external.ftp", false);
pref("network.protocol-handler.external.news", true);
pref("network.protocol-handler.external.mailto", true);
pref("network.protocol-handler.external.irc", true);
pref("network.protocol-handler.external.webcal", true);

// disable xpinstall
pref("xpinstall.enabled", false);

// enable typeahead find
pref("accessibility.typeaheadfind", false);

// disable pings
pref("browser.send_pings", false);
