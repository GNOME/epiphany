// mouse wheel
pref("mousewheel.withcontrolkey.action", 1);
pref("mousewheel.withcontrolkey.numlines", 1);
pref("mousewheel.withcontrolkey.sysnumlines", false);

// allow opening file:/// URLs on pages from network sources (http, etc.)
pref("security.checkloaduri", false);

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
pref("helpers.global_mime_types_file", null);
pref("helpers.private_mime_types_file", null);

// use google for keywords
pref("keyword.URL", "http://www.google.com/search?q=");
pref("keyword.enabled", true);

// disable usless security warnings
pref("security.warn_entering_secure", false);
pref("security.warn_leaving_secure", false);
pref("security.warn_submit_insecure", false);
