'use strict';

window.browser = {};

class EphyEventListener {
    #listeners = [];

    addListener (cb) {
        this.#listeners.push({callback: cb});
    }

    removeListener (cb) {
        this.#listeners = this.#listeners.filter(l => l.callback !== cb);
    }

    hasListener (cb) {
        return !!this.#listeners.find(l => l.callback === cb);
    }

    #emit (...data) {
        for (const listener of this.#listeners)
            listener.callback (...data);
    }

    #emit_with_reply (message, sender, message_guid) {
        let handled = false;
        const reply_callback = function (reply_message) {
            ephy_message ('runtime._sendMessageReply', [message_guid, reply_message]).catch(error_message => {
                console.error(error_message);
            });
        };

        for (const listener of this.#listeners) {
            const ret = listener.callback (message, sender, reply_callback);
            if (typeof ret === 'object' && typeof ret.then === 'function') {
                /* FIXME: I'm very unsure about this behavior. Extensions such as Dark Reader
                 * will have multiple handlers and by listening to extra promises they will
                 * complete the response early. */
                if (handled)
                    continue;
                ret.then(x => { reply_callback(x); }).catch(x => { reply_callback(); });
                handled = true;
            } else if (ret === true) {
                // We expect listener.callback to call `reply_callback`.
                handled = true;
            }
        }

        return handled;
    }
}

const ephy_message = function (fn, args) {
    let callback;

    // This is a `chrome` callback based API.
    if (args.length > 0 && typeof args[args.length - 1] === 'function')
        callback = args.pop ();

    return new Promise (function (resolve, reject) {
        const resolve_wrapper = function (x) {
            window.browser.extension.lastError = null;
            if (callback !== undefined)
                callback (x);
            resolve (x);
        };
        const reject_wrapper = function (x) {
            if (callback !== undefined) {
                window.browser.extension.lastError = new Error(x);
                callback ();
                return;
            }
            reject(x);
        };

        ephy_send_message (fn, args, resolve_wrapper, reject_wrapper);
    });
};

window.browser.runtime = {
    getURL: function (args) { return window.browser.extension.getURL(args); },
    getManifest: function () { return window.browser.extension.getManifest(); },
    onMessage: new EphyEventListener (),
    onConnect: new EphyEventListener (),
    sendMessage: function (...args) {
        return ephy_message ('runtime.sendMessage', args);
    },
    lastError: null,
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
    },
    sync: {
        get: function (...args) {
            return ephy_message ('storage.sync.get', args);
        },
        set: function (...args) {
            return ephy_message ('storage.sync.set', args);
        },
        remove: function (...args) {
            return ephy_message ('storage.sync.remove', args);
        },
        clear: function () {
            return ephy_message ('storage.sync.clear');
        }
    }
};

// Compatibility with Chrome
window.chrome = window.browser;
