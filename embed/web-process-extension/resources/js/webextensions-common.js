'use strict';

/* exported runtimeSendMessage, runtimeOnConnect, ephy_message */

window.browser = {};

const promises = [];
let last_promise = 0;

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

const ephy_message = function (fn, args, cb) {
    const promise = new Promise (function (resolve, reject) {
        window.webkit.messageHandlers.epiphany.postMessage ({fn: fn, args: args, promise: last_promise});
        last_promise = promises.push({resolve: resolve, reject: reject});
    });
    return promise;
};

window.browser.runtime = {
    getURL: function (args, cb) { return window.browser.extension.getURL(args, cb); },
    getManifest: function (args, cb) { return '[]'; },
    onMessage: new EphyEventListener (),
    onConnect: new EphyEventListener (),
    sendMessage: function (args, cb) {
        return ephy_message ('runtime.sendMessage', args, cb);
    },
};


window.browser.storage = {
    local: {
        get: function (keys) {
            return ephy_message ('storage.local.get', keys);
        },
        set: function (keys) {
            return ephy_message ('storage.local.set', keys);
        },
        remove: function (keys) {
            return ephy_message ('storage.local.remove', keys);
        },
        clear: function () {
            return ephy_message ('storage.local.clear');
        }
    }
};

// Compatibility with Chrome
window.chrome = window.browser;
