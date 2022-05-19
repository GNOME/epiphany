'use strict';

/* exported runtimeSendMessage, runtimeOnConnect, ephy_message */

window.browser = {};

const promises = [];
let last_promise = 0;

let runtime_onmessage_listeners = [];
let runtime_onconnect_listeners = [];

const ephy_message = function (fn, args, cb) {
    const promise = new Promise (function (resolve, reject) {
        window.webkit.messageHandlers.epiphany.postMessage ({fn: fn, args: args, promise: last_promise});
        last_promise = promises.push({resolve: resolve, reject: reject});
    });
    return promise;
};

const runtimeSendMessage = function(x) {
  for (const listener of runtime_onmessage_listeners)
    listener.callback(x);
};

const runtimeOnConnect = function(x) {
  for (const listener of runtime_onconnect_listeners)
    listener.callback(x);
};

window.browser.runtime = {
    getURL: function (args, cb) { return window.browser.extension.getURL(args, cb); },
    getManifest: function (args, cb) { return '[]'; },
    onMessage: {
        addListener: function (cb) {
            runtime_onmessage_listeners.push({callback: cb});
        },
        removeListener: function (cb) {
            runtime_onmessage_listeners = runtime_onmessage_listeners.filter(l => l.callback !== cb);
        },
        hasListener: function (cb) {
            return !!runtime_onmessage_listeners.find(l => l.callback === cb);
        }
    },
    onConnect: {
        addListener: function (cb) {
            runtime_onconnect_listeners.push({callback: cb});
        },
        removeListener: function (cb) {
            runtime_onconnect_listeners = runtime_onconnect_listeners.filter(l => l.callback !== cb);
        },
        hasListener: function (cb) {
            return !!runtime_onconnect_listeners.find(l => l.callback === cb);
        }
    },
    sendMessage: function (args, cb) {
        return ephy_message ('runtime.sendMessage', args, cb);
    },
};

// Compatibility with Chrome
window.chrome = window.browser;
