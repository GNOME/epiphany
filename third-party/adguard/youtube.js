/**
 * This file is part of AdGuard's Block YouTube Ads (https://github.com/AdguardTeam/BlockYouTubeAdsShortcut).
 *
 * AdGuard's Block YouTube Ads is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * AdGuard's Block YouTube Ads is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with AdGuard's Block YouTube Ads.  If not, see <http://www.gnu.org/licenses/>.
 */

/* global Response, window, navigator, document, MutationObserver, completion */

/**
 * The function that implements all the logic.
 * Returns the run status.
 */
function runBlockYoutube() {
    const locales = {
        en: {
            logo: 'with&nbsp;AdGuard',
            alreadyExecuted: 'The shortcut has already been executed.',
            wrongDomain: 'This shortcut is supposed to be launched only on YouTube.',
            success: 'YouTube is now ad-free! Please note that you need to run this shortcut again if you reload the page.',
        },
        ru: {
            logo: 'с&nbsp;AdGuard',
            alreadyExecuted: 'Быстрая команда уже выполнена.',
            wrongDomain: 'Эта быстрая команда предназначена для использования только на YouTube.',
            success: 'Теперь YouTube без рекламы! Важно: при перезагрузке страницы вам нужно будет заново запустить команду.',
        },
        es: {
            logo: 'con&nbsp;AdGuard',
            alreadyExecuted: 'El atajo ya ha sido ejecutado.',
            wrongDomain: 'Se supone que este atajo se lanza sólo en YouTube.',
            success: '¡YouTube está ahora libre de anuncios! Ten en cuenta que tienes que volver a ejecutar este atajo si recargas la página.',
        },
        de: {
            logo: 'mit&nbsp;AdGuard',
            alreadyExecuted: 'Der Kurzbefehl wurde bereits ausgeführt.',
            wrongDomain: 'Dieser Kurzbefehl soll nur auf YouTube gestartet werden.',
            success: 'YouTube ist jetzt werbefrei! Bitte beachten Sie, dass Sie diesen Kurzbefehl erneut ausführen müssen, wenn Sie die Seite neu laden.',
        },
        fr: {
            logo: 'avec&nbsp;AdGuard',
            alreadyExecuted: 'Le raccourci a déjà été exécuté.',
            wrongDomain: 'Ce raccourci est censé d’être lancé uniquement sur YouTube.',
            success: 'YouTube est maintenant libre de pub ! Veuillez noter qu’il faudra rééxecuter le raccourci si vous rechargez la page.',
        },
        it: {
            logo: 'con&nbsp;AdGuard',
            alreadyExecuted: 'Il comando è già stato eseguito.',
            wrongDomain: 'Questa scorciatoia dovrebbe essere lanciata solo su YouTube.',
            success: 'YouTube è ora libero da pubblicità! Si prega di notare che è necessario eseguire nuovamente questa scorciatoia se ricarichi la pagina.',
        },
        'zh-cn': {
            logo: '使用&nbsp;AdGuard',
            alreadyExecuted: '快捷指令已在运行',
            wrongDomain: '快捷指令只能在 YouTube 上被启动。',
            success: '现在您的 YouTube 没有广告！请注意，若您重新加载页面，您需要再次启动快捷指令。',
        },
        'zh-tw': {
            logo: '偕同&nbsp;AdGuard',
            alreadyExecuted: '此捷徑已被執行。',
            wrongDomain: '此捷徑應該只於 YouTube 上被啟動。',
            success: '現在 YouTube 為無廣告的！請注意，若您重新載入該頁面，您需要再次執行此捷徑。',
        },
        ko: {
            logo: 'AdGuard&nbsp;사용',
            alreadyExecuted: '단축어가 이미 실행되었습니다.',
            wrongDomain: '이 단축어는 YouTube에서만 사용 가능합니다.',
            success: '이제 광고없이 YouTube를 시청할 수 있습니다. 페이지를 새로고침 할 경우, 이 단축어를 다시 실행해야 합니다.',
        },
        ja: {
            logo: 'AdGuard作動中',
            alreadyExecuted: 'ショートカットは既に実行されています。',
            wrongDomain: '※このショートカットは、YouTubeでのみ適用されることを想定しています。',
            success: 'YouTubeが広告なしになりました！※YouTubeページを再読み込みした場合は、このショートカットを再度実行する必要がありますのでご注意ください。',
        },
        uk: {
            logo: 'з&nbsp;AdGuard',
            alreadyExecuted: 'Ця швидка команда вже виконується.',
            wrongDomain: 'Цю швидку команду слід запускати лише на YouTube.',
            success: 'Тепер YouTube без реклами! Проте після перезавантаження сторінки необхідно знову запустити цю швидку команду.',
        },
    };

    /**
     * Gets a localized message for the specified key
     *
     * @param {string} key message key
     * @returns {string} message for that key
     */
    const getMessage = (key) => {
        try {
            let locale = locales[navigator.language.toLowerCase()];
            if (!locale) {
                const lang = navigator.language.split('-')[0];
                locale = locales[lang];
            }
            if (!locale) {
                locale = locales.en;
            }

            return locale[key];
        } catch (ex) {
            return locales.en[key];
        }
    };

    if (document.getElementById('block-youtube-ads-logo')) {
        return {
            success: false,
            status: 'alreadyExecuted',
            message: getMessage('alreadyExecuted'),
        };
    }

    if (window.location.hostname !== 'www.youtube.com'
        && window.location.hostname !== 'm.youtube.com'
        && window.location.hostname !== 'music.youtube.com') {
        return {
            success: false,
            status: 'wrongDomain',
            message: getMessage('wrongDomain'),
        };
    }

    /**
     * Note that Shortcut scripts are executed in their own context (window)
     * and we don't have direct access to the real page window.
     *
     * In order to overcome this, we add a "script" to the page which is
     * executed in the proper context. The script content is inside
     * the "pageScript" function.
     */
    const pageScript = () => {
        const LOGO_ID = 'block-youtube-ads-logo';

        const hiddenCSS = {
            'www.youtube.com': [
                '#__ffYoutube1',
                '#__ffYoutube2',
                '#__ffYoutube3',
                '#__ffYoutube4',
                '#feed-pyv-container',
                '#feedmodule-PRO',
                '#homepage-chrome-side-promo',
                '#merch-shelf',
                '#offer-module',
                '#pla-shelf > ytd-pla-shelf-renderer[class="style-scope ytd-watch"]',
                '#pla-shelf',
                '#premium-yva',
                '#promo-info',
                '#promo-list',
                '#promotion-shelf',
                '#related > ytd-watch-next-secondary-results-renderer > #items > ytd-compact-promoted-video-renderer.ytd-watch-next-secondary-results-renderer',
                '#search-pva',
                '#shelf-pyv-container',
                '#video-masthead',
                '#watch-branded-actions',
                '#watch-buy-urls',
                '#watch-channel-brand-div',
                '#watch7-branded-banner',
                '#YtKevlarVisibilityIdentifier',
                '#YtSparklesVisibilityIdentifier',
                '.carousel-offer-url-container',
                '.companion-ad-container',
                '.GoogleActiveViewElement',
                '.list-view[style="margin: 7px 0pt;"]',
                '.promoted-sparkles-text-search-root-container',
                '.promoted-videos',
                '.searchView.list-view',
                '.sparkles-light-cta',
                '.watch-extra-info-column',
                '.watch-extra-info-right',
                '.ytd-carousel-ad-renderer',
                '.ytd-compact-promoted-video-renderer',
                '.ytd-companion-slot-renderer',
                '.ytd-merch-shelf-renderer',
                '.ytd-player-legacy-desktop-watch-ads-renderer',
                '.ytd-promoted-sparkles-text-search-renderer',
                '.ytd-promoted-video-renderer',
                '.ytd-search-pyv-renderer',
                '.ytd-video-masthead-ad-v3-renderer',
                '.ytp-ad-action-interstitial-background-container',
                '.ytp-ad-action-interstitial-slot',
                '.ytp-ad-image-overlay',
                '.ytp-ad-overlay-container',
                '.ytp-ad-progress',
                '.ytp-ad-progress-list',
                '[class*="ytd-display-ad-"]',
                '[layout*="display-ad-"]',
                'a[href^="http://www.youtube.com/cthru?"]',
                'a[href^="https://www.youtube.com/cthru?"]',
                'ytd-action-companion-ad-renderer',
                'ytd-banner-promo-renderer',
                'ytd-compact-promoted-video-renderer',
                'ytd-companion-slot-renderer',
                'ytd-display-ad-renderer',
                'ytd-promoted-sparkles-text-search-renderer',
                'ytd-promoted-sparkles-web-renderer',
                'ytd-search-pyv-renderer',
                'ytd-single-option-survey-renderer',
                'ytd-video-masthead-ad-advertiser-info-renderer',
                'ytd-video-masthead-ad-v3-renderer',
                'YTM-PROMOTED-VIDEO-RENDERER',
            ],
            'm.youtube.com': [
                '.companion-ad-container',
                '.ytp-ad-action-interstitial',
                '.ytp-cued-thumbnail-overlay > div[style*="/sddefault.jpg"]',
                'a[href^="/watch?v="][onclick^="return koya.onEvent(arguments[0]||window.event,\'"]:not([role]):not([class]):not([id])',
                'a[onclick*=\'"ping_url":"http://www.google.com/aclk?\']',
                'ytm-companion-ad-renderer',
                'ytm-companion-slot',
                'ytm-promoted-sparkles-text-search-renderer',
                'ytm-promoted-sparkles-web-renderer',
                'ytm-promoted-video-renderer',
            ],
        };

        /**
         * Adds CSS to the page
         * @param {string} hostname hostname
         */
        const hideElements = (hostname) => {
            const selectors = hiddenCSS[hostname];
            if (!selectors) {
                return;
            }
            const rule = `${selectors.join(', ')} { display: none!important; }`;
            const style = document.createElement('style');
            style.innerHTML = rule;
            document.head.appendChild(style);
        };

        /**
         * Calls the "callback" function on every DOM change, but not for the tracked events
         * @param {Function} callback callback function
         */
        const observeDomChanges = (callback) => {
            const domMutationObserver = new MutationObserver((mutations) => {
                callback(mutations);
            });

            domMutationObserver.observe(document.documentElement, {
                childList: true,
                subtree: true,
            });
        };

        /**
         * This function is supposed to be called on every DOM change
         */
        const hideDynamicAds = () => {
            const elements = document.querySelectorAll('#contents > ytd-rich-item-renderer ytd-display-ad-renderer');
            if (elements.length === 0) {
                return;
            }
            elements.forEach((el) => {
                if (el.parentNode && el.parentNode.parentNode) {
                    const parent = el.parentNode.parentNode;
                    if (parent.localName === 'ytd-rich-item-renderer') {
                        parent.style.display = 'none';
                    }
                }
            });
        };

        /**
         * This function checks if the video ads are currently running
         * and auto-clicks the skip button.
         */
        const autoSkipAds = () => {
            // If there's a video that plays the ad at this moment, scroll this ad
            if (document.querySelector('.ad-showing')) {
                const video = document.querySelector('video');
                if (video && video.duration) {
                    video.currentTime = video.duration;
                    // Skip button should appear after that,
                    // now simply click it automatically
                    setTimeout(() => {
                        const skipBtn = document.querySelector('button.ytp-ad-skip-button');
                        if (skipBtn) {
                            skipBtn.click();
                        }
                    }, 100);
                }
            }
        };

        /**
         * This function overrides a property on the specified object.
         *
         * @param {object} obj object to look for properties in
         * @param {string} propertyName property to override
         * @param {*} overrideValue value to set
         */
        const overrideObject = (obj, propertyName, overrideValue) => {
            if (!obj) {
                return false;
            }
            let overriden = false;

            for (const key in obj) {
                if (obj.hasOwnProperty(key) && key === propertyName) {
                    obj[key] = overrideValue;
                    overriden = true;
                } else if (obj.hasOwnProperty(key) && typeof obj[key] === 'object') {
                    if (overrideObject(obj[key], propertyName, overrideValue)) {
                        overriden = true;
                    }
                }
            }

            if (overriden) {
                console.log(`found: ${propertyName}`);
            }

            return overriden;
        };

        /**
         * Overrides JSON.parse and Response.json functions.
         * Examines these functions arguments, looks for properties with the specified name there
         * and if it exists, changes it's value to what was specified.
         *
         * @param {string} propertyName name of the property
         * @param {*} overrideValue new value for the property
         */
        const jsonOverride = (propertyName, overrideValue) => {
            const nativeJSONParse = JSON.parse;
            JSON.parse = (...args) => {
                const obj = nativeJSONParse.apply(this, args);

                // Override it's props and return back to the caller
                overrideObject(obj, propertyName, overrideValue);
                return obj;
            };

            // Override Response.prototype.json
            const nativeResponseJson = Response.prototype.json;
            Response.prototype.json = new Proxy(nativeResponseJson, {
                apply(...args) {
                    // Call the target function, get the original Promise
                    const promise = Reflect.apply(args);

                    // Create a new one and override the JSON inside
                    return new Promise((resolve, reject) => {
                        promise.then((data) => {
                            overrideObject(data, propertyName, overrideValue);
                            resolve(data);
                        }).catch((error) => reject(error));
                    });
                },
            });
        };

        const addAdGuardLogoStyle = () => {
            const id = 'block-youtube-ads-logo-style';
            if (document.getElementById(id)) {
                return;
            }

            // Here is what these styles do:
            // 1. Change AG marker color depending on the page
            // 2. Hide Sign-in button on m.youtube.com otherwise it does not look good
            // It is still possible to sign in by clicking "three dots" button.
            // 3. Hide the marker when the user is searching for something
            // 4. On YT Music apply display:block to the logo element
            const style = document.createElement('style');
            style.innerHTML = `[data-mode="watch"] #${LOGO_ID} { color: #fff; }
[data-mode="searching"] #${LOGO_ID}, [data-mode="search"] #${LOGO_ID} { display: none; }
#${LOGO_ID} { white-space: nowrap; }
.mobile-topbar-header-sign-in-button { display: none; }
.ytmusic-nav-bar#left-content #${LOGO_ID} { display: block; }`;
            document.head.appendChild(style);
        };

        const addAdGuardLogo = () => {
            if (document.getElementById(LOGO_ID)) {
                return;
            }

            const logo = document.createElement('span');
            // logo.innerHTML = '__logo_text__';
            logo.setAttribute('id', LOGO_ID);

            if (window.location.hostname === 'm.youtube.com') {
                const btn = document.querySelector('header.mobile-topbar-header > button');
                if (btn) {
                    btn.parentNode.insertBefore(logo, btn.nextSibling);
                    addAdGuardLogoStyle();
                }
            } else if (window.location.hostname === 'www.youtube.com') {
                const code = document.getElementById('country-code');
                if (code) {
                    code.innerHTML = '';
                    code.appendChild(logo);
                    addAdGuardLogoStyle();
                }
            } else if (window.location.hostname === 'music.youtube.com') {
                const el = document.querySelector('.ytmusic-nav-bar#left-content');
                if (el) {
                    el.appendChild(logo);
                    addAdGuardLogoStyle();
                }
            }
        };

        // Removes ads metadata from YouTube XHR requests
        jsonOverride('adPlacements', []);
        jsonOverride('playerAds', []);

        // Applies CSS that hides YouTube ad elements
        hideElements(window.location.hostname);

        // Some changes should be re-evaluated on every page change
        addAdGuardLogo();
        hideDynamicAds();
        autoSkipAds();
        observeDomChanges(() => {
            addAdGuardLogo();
            hideDynamicAds();
            autoSkipAds();
        });
    };

    const script = document.createElement('script');
    const scriptText = pageScript.toString().replace('__logo_text__', getMessage('logo'));
    script.innerHTML = `(${scriptText})();`;
    document.head.appendChild(script);
    document.head.removeChild(script);

    return {
        success: true,
        status: 'success',
        message: getMessage('success'),
    };
}

/**
 * Runs the shortcut
 */
(() => {
    // "completion" function is only defined if this script is launched as Shortcut
    // in other cases we simply polyfill it.
    let finish = (m) => { console.log(m); };
    if (typeof completion !== 'undefined') {
        finish = completion;
    }

    try {
        const result = runBlockYoutube();
        finish(result.message);
    } catch (ex) {
        finish(ex.toString());
    }
})();
