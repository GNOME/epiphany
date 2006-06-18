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
pref("security.warn_entering_weak", true);
pref("security.warn_entering_weak.show_once", false);

// always ask which client cert to use
pref("security.default_personal_cert", "Ask Every Time");

// fonts
pref("font.size.unit", "pt");

// protocols
pref("network.protocol-handler.external.ftp", false);
pref("network.protocol-handler.external.irc", true);
pref("network.protocol-handler.external.mailto", true);
pref("network.protocol-handler.external.news", true);
pref("network.protocol-handler.external.nntp", true);
pref("network.protocol-handler.external.snews", true);
pref("network.protocol-handler.external.webcal", true);
// but don't show warnings for these
pref("network.protocol-handler.warn-external.irc", false);
pref("network.protocol-handler.warn-external.mailto", false);
pref("network.protocol-handler.warn-external.news", false);
pref("network.protocol-handler.warn-external.nntp", false);
pref("network.protocol-handler.warn-external.snews", false);
pref("network.protocol-handler.warn-external.webcal", false);

// disable xpinstall
pref("xpinstall.enabled", false);

// enable plugin finder
pref("plugin.default_plugin_disabled", true);

// enable locale matching
pref("intl.locale.matchOS", true);

// enable fixed-up typeaheadfind extension
pref("accessibility.typeaheadfindsea", false);
pref("accessibility.typeaheadfindsea.autostart", true);
pref("accessibility.typeaheadfindsea.linksonly", true);

// disable image resizing
pref("browser.enable_automatic_image_resizing", false);

// enable password manager
// need to include those prefs since xulrunner doesn't include them
pref("signon.rememberSignons", true);
pref("signon.expireMasterPassword", false);
pref("signon.SignonFileName", "signons.txt");

// use system colours
pref("browser.display.use_system_colors", true);

// explicitly enable error pages (xulrunner is missing this pref)
pref("browser.xul.error_pages.enabled", true);

// unset weird xulrunner default UA string
pref("general.useragent.extra.simple", "");

// we don't want ping(uin)s
pref("browser.send_pings", false);
pref("browser.send_pings.require_same_host", true);

// disable blink tags
pref("browser.blink_allowed", false);

// enable spatial navigation (only works if the extension is built with gecko)
pref("snav.enabled", true);

// don't leak UI language
// pref("general.useragent.locale", "en")

// spellcheck
// pref("extensions.spellcheck.inline.max-misspellings", -1);
// 0: disabled, 1: only textareas, 2: check textareas and single-line input fields
pref("layout.spellcheckDefault", 1);

// print settings
pref("print.use_global_printsettings", false);
pref("print.save_print_settings", false);
pref("print.show_print_progress", true);
pref("print.printer_list", "");
pref("postscript.enabled", true);
pref("postscript.cups.enabled", false);
