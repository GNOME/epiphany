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
