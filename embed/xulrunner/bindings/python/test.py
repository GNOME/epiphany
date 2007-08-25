#! /usr/bin/python
import pygtk
pygtk.require("2.0")
import gtk
import gnomegeckoembed
import os

class Base:
    def destroy(self, widget, data=None):
        gtk.main_quit()
        
    def __init__(self):
        gnomegeckoembed.gecko_embed_single_set_comp_path("/usr/lib/firefox")
        gnomegeckoembed.gecko_embed_single_push_startup()
        self.browser = gnomegeckoembed.Embed()
        self.browser.load_url("www.gnome.org")
        
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.connect("destroy", self.destroy)
        self.window.set_default_size(800, 600)
        self.window.add(self.browser)
        self.browser.show()
        self.window.show()

    def main(self):
        gtk.main()

print __name__
if __name__ == "__main__":
    base = Base()
    base.main()

