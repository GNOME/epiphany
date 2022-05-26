'use strict';

/* exported ephy_message */
/* global ephy_send_message */

window.browser = {};

class EphyEventListener {
    constructor () {
        this._listeners = [];
    }

    addListener (cb) {
        this._listeners.push({callback: cb});
    }

    removeListener (cb) {
        this._listeners = this._listeners.filter(l => l.callback !== cb);
    }

    hasListener (cb) {
        return !!this._listeners.find(l => l.callback === cb);
    }

    _emit (data) {
        for (const listener of this._listeners)
            listener.callback (data);
    }
}

const ephy_message = function (fn, args) {
    let callback;

    // This is a `chrome` callback based API.
    if (args.length > 0 && typeof args[args.length - 1] === 'function')
        callback = args.pop ();

    return new Promise (function (resolve, reject) {
        const resolve_wrapper = function (x) {
            if (callback !== undefined)
                callback (x);
            resolve (x);
        };
        ephy_send_message (fn, args, resolve_wrapper, reject);
    });
};

window.browser.runtime = {
    getURL: function (args) { return window.browser.extension.getURL(args); },
    getManifest: function () { return {}; },
    onMessage: new EphyEventListener (),
    onConnect: new EphyEventListener (),
    sendMessage: function (...args) {
        return ephy_message ('runtime.sendMessage', args);
    },
};


window.browser.storage = {
    local: {
        get: function (...args) {
            return ephy_message ('storage.local.get', args);
        },
        set: function (...args) {
            return ephy_message ('storage.local.set', args);
        },
        remove: function (...args) {
            return ephy_message ('storage.local.remove', args);
        },
        clear: function () {
            return ephy_message ('storage.local.clear');
        }
    }
};

// Compatibility with Chrome
window.chrome = window.browser;
