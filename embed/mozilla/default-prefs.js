// Don't allow mozilla to raise window when setting focus (work around bugs)
user_pref("mozilla.widget.raise-on-setfocus", false);

// set default search engine
user_pref("keyword.URL", "http://www.google.com/search?q=");
user_pref("keyword.enabled", true);
user_pref("security.checkloaduri", false);

// dont allow xpi installs from epiphany, there are crashes
user_pref("xpinstall.enabled", false);

// deactivate mailcap and mime.types support
user_pref("helpers.global_mailcap_file", "");
user_pref("helpers.global_mime_types_file", "");
user_pref("helpers.private_mailcap_file", "");
user_pref("helpers.private_mime_types_file", "");

// disable sucky XUL ftp view, have nice ns4-like html page instead
user_pref("network.dir.generate_html", true);

// disable usless security warnings
user_pref("security.warn_entering_secure", false);
user_pref("security.warn_leaving_secure", false);
user_pref("security.warn_submit_insecure", false);
