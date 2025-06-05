'use strict';

var Ephy = {};

Ephy.getOpenSearchLinks = function()
{
    const nodes_list = document.querySelectorAll('link[rel="search"][type="application/opensearchdescription+xml"][href][title]');
    return Array.from(nodes_list).map(node => ({'href': node.href, 'title': node.title}));
};

Ephy.getAppleMobileWebAppCapable = function()
{
    for (const meta of document.getElementsByTagName('meta')) {
        // https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
        if (meta.name === 'apple-mobile-web-app-capable' && meta.getAttribute('content') === 'yes')
            return true;
    }

    return false;
};

Ephy.getWebAppManifestURL = function()
{
    const manifest = document.head.querySelector('link[rel=manifest]');
    return manifest ? manifest.href : null;
};

Ephy.getWebAppTitle = function()
{
    for (const meta of document.getElementsByTagName('meta')) {
        // https://developer.mozilla.org/en-US/docs/Web/HTML/Element/meta/name#standard_metadata_names_defined_in_the_html_specification
        if (meta.name === 'application-name')
            return meta.content;

        // https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariWebContent/ConfiguringWebApplications/ConfiguringWebApplications.html
        if (meta.name === 'apple-mobile-web-app-title')
            return meta.content;

        // https://ogp.me/
        // og:site_name is read from the property attribute (standard), but is
        // commonly seen on the web in the name attribute. Both are supported.
        if (meta.getAttribute('property') === 'og:site_name' || meta.name === 'og:site_name')
            return meta.content;
    }

    // document title
    // Prefer HTML tag over dynamic updates
    const titles = document.head.getElementsByTagName('title');
    if (titles.length > 0) {
      const title = titles[titles.length - 1];

      if (title && title.innerText)
          return title.innerText;
    }
    if (document.title)
        return document.title;

    // set_default_application_title will fallback to the formatted hostname
    return null;
};

Ephy.getWebAppIcon = function(baseURL)
{
    let htmlIconURL = null;
    let msIconURL = null;
    let largestIconSize = 0;
    let iconColor = null;
    const links = document.getElementsByTagName('link');
    const metas = document.getElementsByTagName('meta');

    for (const link of links) {
        if (link.rel === 'icon' || link.rel === 'shortcut icon' || link.rel === 'icon shortcut' || link.rel === 'shortcut-icon' || link.rel === 'apple-touch-icon' || link.rel === 'apple-touch-icon-precomposed') {
            const sizes = link.getAttribute('sizes');

            if (!sizes) {
                if (largestIconSize === 0 && (!htmlIconURL || link.rel === 'apple-touch-icon' || link.rel === 'apple-touch-icon-precomposed'))
                  htmlIconURL = link.href;
                continue;
            }

            if (sizes === 'any') {
                // "any" means a vector, and thus it will always be the largest icon.
                htmlIconURL = link.href;
                break;
            }

            const sizesList = sizes.split(' ');
            for (let size of sizesList) {
                size = size.toLowerCase().split('x');

                // Only accept square icons.
                if (size.length !== 2 || size[0] !== size[1])
                    continue;

                // Only accept icons of 96 px (smallest GNOME HIG app icon) or larger.
                // It's better to defer to other icon discovery methods if smaller
                // icons are returned here.
                if (size[0] >= 92 && size[0] > largestIconSize) {
                    htmlIconURL = link.href;
                    largestIconSize = size[0];
                }
            }
        }
    }

    if (largestIconSize !== 0 && htmlIconURL)
        return { 'url' : new URL(htmlIconURL, baseURL).href, 'color' : null };

    for (const meta of metas) {
        if (meta.name === 'msapplication-TileImage')
            msIconURL = meta.content;
        else if (meta.name === 'msapplication-TileColor')
            iconColor = meta.content;
    }

    // msapplication icon.
    if (msIconURL)
        return { 'url' : new URL(msIconURL, baseURL).href, 'color' : iconColor };

    // html icon without known size
    if (htmlIconURL)
        return { 'url' : new URL(htmlIconURL, baseURL).href, 'color' : null };

    // Last ditch effort: just fallback to the default favicon location.
    return { 'url' : new URL('/favicon.ico', baseURL).href, 'color' : null };
};

Ephy.PreFillUserMenu = class PreFillUserMenu
{
    #manager;
    #formAuth;
    #userElement;
    #users;
    #passwordElement;
    #selected = null;
    #wasEdited = false;

    constructor(manager, formAuth, users)
    {
        this.#manager = manager;
        this.#formAuth = formAuth;
        this.#userElement = formAuth.usernameNode;
        this.#users = users;
        this.#passwordElement = formAuth.passwordNode;

        this.#userElement.addEventListener('input', this.#onInput.bind(this), true);
        this.#userElement.addEventListener('mouseup', this.#onMouseUp.bind(this), false);
        this.#userElement.addEventListener('keydown', this.#onKeyDown.bind(this), false);
        this.#userElement.addEventListener('change', this.#removeMenu, false);
        this.#userElement.addEventListener('blur', this.#removeMenu, false);
    }

    #onInput(event)
    {
        if (this.#manager.isAutoFilling(this.#userElement))
            return;

        this.#wasEdited = true;
        this.#removeMenu();
        this.#showMenu(false);
    }

    #onMouseUp(event)
    {
        if (document.getElementById('ephy-user-choices-container'))
            return;

        this.#showMenu(!this.#wasEdited);
    }

    #onKeyDown(event)
    {
        if (event.key === 'Escape') {
            this.#removeMenu();
            return;
        }

        if (event.key !== 'ArrowDown' && event.key !== 'ArrowUp')
            return;

        const container = document.getElementById('ephy-user-choices-container');
        if (!container) {
            this.#showMenu(!this.#wasEdited);
            return;
        }

        let newSelect = null;
        if (this.#selected)
            newSelect = event.key !== 'ArrowUp' ? this.#selected.previousSibling : this.#selected.nextSibling;

        if (!newSelect)
            newSelect = event.key !== 'ArrowUp' ? container.firstElementChild.lastElementChild : container.firstElementChild.firstElementChild;

        if (newSelect) {
            this.#selected = newSelect;
            this.#userElement.value = this.#selected.firstElementChild.textContent;
            this.#usernameSelected();
        } else {
            this.#passwordElement.value = '';
        }

        event.preventDefault();
    }

    #showMenu(showAll)
    {
        const mainDiv = document.createElement('div');
        const styles = `
          .ephy-dropdown {
            position: absolute;
            display: inline-block;
          }

          .ephy-dropdown-content {
            display: block;
            position: absolute;
            padding: 0;
            background-color: #ffffff;
            background-clip: padding-box;
            border-style: solid;
            border-color: #e0e0e0;
            border-radius: 9px;
            border-width: 1px;
            min-width: 160px;
            box-shadow: 0 1px 2px transparentize(black, 0.7);
            z-index: 1;
          }

          /* Links inside the dropdown */
          .ephy-dropdown-content a {
            color: black;
            padding: 12px 16px;
            text-decoration: none;
            display: block;
          }

          /* Change color of dropdown links on hover */
          .ephy-dropdown-content a:hover {background-color: #f1f1f1}
        `;

        let styleSheet = document.createElement("style")
        styleSheet.textContent = styles;
        document.head.appendChild(styleSheet);

        mainDiv.id = 'ephy-user-choices-container';

        const elementRect = this.#userElement.getBoundingClientRect();

        mainDiv.className = 'ephy-dropdown';
        mainDiv.style.width = this.#userElement.offsetWidth + 'px';
        mainDiv.style.left = elementRect.left + document.body.scrollLeft + 'px';
        mainDiv.style.top = elementRect.top + elementRect.height + document.body.scrollTop + 'px';

        const innerDiv = document.createElement('div');
        innerDiv.className = 'ephy-dropdown-content';
        mainDiv.appendChild(innerDiv);

        this.#selected = null;
        for (const user of this.#users) {
            if (!showAll && !user.startsWith(this.#userElement.value))
                continue;

            const link = document.createElement('a');
            innerDiv.appendChild(link);

            if (user === this.#userElement.value)
                this.#selected = link;

            link.textContent = user;

            link.addEventListener('mousedown', event => {
                this.#userElement.value = user;
                this.#selected = link;
                this.#removeMenu();
                this.#usernameSelected();
            }, true);
        }

        document.body.appendChild(mainDiv);

        if (!this.#selected)
            this.#passwordElement.value = '';
    }

    #removeMenu()
    {
        const menu = document.getElementById('ephy-user-choices-container');
        if (menu)
            menu.parentNode.removeChild(menu);
    }

    #usernameSelected()
    {
        this.#formAuth.username = this.#userElement.value;
        this.#passwordElement.value = '';
        this.#manager.preFill(this.#formAuth);
    }
};

Ephy.formControlsAssociated = function(pageID, frameID, elements, serializer)
{
    let formElements = [];

    for (const element of elements) {
        if (!(element instanceof HTMLFormElement))
            continue;

        const formManager = new Ephy.FormManager(pageID, frameID, element, serializer);
        formManager.preFillForms();
        Ephy.FormManager.managers.push(formManager);

        formElements.push(element);
    }

    // This function is called in two scenarios:
    //
    // 1) Form is created. One of the elements will be an HTMLFormElement. We
    //    would have found it above and created a FormManager.
    // 2) Elements are moved between existing forms. The FormManager should
    //    already exist from a previous call to this function.
    //
    // WebKit may batch updates together before eventually firing the form
    // controls associated event later on, so possibly there could be multiple
    // forms here, and both scenarios could be happening at the same time.
    for (const element of elements) {
        if (element instanceof HTMLFormElement)
            continue;

        // We want to find each parent form element and process it only once.
        // Anything in formElements has already been processed.
        const formElement = element.closest('form');
        if (formElement && !formElements.includes(formElement)) {
            if (!formElement instanceof HTMLFormElement) {
                Ephy.log('Attempted to find parent HTMLFormElement, but found something else instead; this is probably an Epiphany bug');
                continue;
            }
            formElements.push(formElement);

            const manager = Ephy.FormManager.managerForForm(formElement);
            if (!manager) {
                Ephy.log('Missing form manager for a form element that should have one already; this is probably an Epiphany bug');
                continue;
            }
            manager.preFillForms();
        }
    }
};

Ephy.handleFormSubmission = function(pageID, frameID, form)
{
    // FIXME: Find out: is it really possible to have multiple frames with same window object???
    let formManager = null;
    for (const manager of Ephy.FormManager.managers) {
        if (manager.frameID() === frameID && manager.form() === form) {
            formManager = manager;
            break;
        }
    }

    if (!formManager) {
        formManager = new Ephy.FormManager(pageID, frameID, form);
        Ephy.FormManager.managers.push(formManager);
    }

    formManager.handleFormSubmission();
};

Ephy.hasModifiedForms = function()
{
    for (const form of document.forms) {
        let modifiedInputElement = false;
        for (const element of form.elements) {
            if (!Ephy.isEdited(element))
                continue;

            if (element instanceof HTMLTextAreaElement)
                return !!element.value;

            if (element instanceof HTMLInputElement) {
                // A small heuristic here. If there's only one input element
                // modified and it does not have a lot of text the user is
                // likely not very interested in saving this work, so do
                // nothing (e.g. google search input).
                if (element.value.length > 50)
                    return true;
                if (modifiedInputElement)
                    return true;
                modifiedInputElement = true;
            }
        }
    }
};

Ephy.isSandboxedWebContent = function()
{
    // https://github.com/google/security-research/security/advisories/GHSA-mhhf-w9xw-pp9x
    return self.origin === null || self.origin === 'null';
};

Ephy.PasswordManager = class PasswordManager
{
    #pageID;
    #frameID;
    #pendingPromises = [];
    #promiseCounter = 0;

    constructor(pageID, frameID)
    {
        this.#pageID = pageID;
        this.#frameID = frameID;
    }

    #takePendingPromise(id)
    {
        const element = this.#pendingPromises.find(element => element.promiseID === id);
        if (element)
            this.#pendingPromises = this.#pendingPromises.filter(element => element.promiseID !== id);
        return element;
    }

    // Called by ephy-web-process-extension.c
    onQueryResponse(username, password, id)
    {
        Ephy.log(`Received password query response for username=${username}`);

        const element = this.#takePendingPromise(id);
        if (element) {
            if (password)
                element.resolver({username, password});
            else
                element.resolver(null);
        }
    }

    query(origin, targetOrigin, username, usernameField, passwordField)
    {
        if (Ephy.isSandboxedWebContent()) {
            Ephy.log(`Not querying passwords for origin=${origin} because web content is sandboxed`);
            return Promise.resolve(null);
        }

        Ephy.log(`Querying passwords for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}`);

        return new Promise((resolver, reject) => {
            const promiseID = this.#promiseCounter++;
            Ephy.queryPassword(origin, targetOrigin, username, usernameField, passwordField, promiseID, this.#pageID, this.#frameID);
            this.#pendingPromises.push({promiseID, resolver});
        });
    }

    save(origin, targetOrigin, username, password, usernameField, passwordField, isNew)
    {
        if (Ephy.isSandboxedWebContent()) {
            Ephy.log(`Not saving password for origin=${origin} because web content is sandboxed`);
            return;
        }

        Ephy.log(`Saving password for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}, isNew=${isNew}`);

        window.webkit.messageHandlers.passwordManagerSave.postMessage({
            origin, targetOrigin, username, password, usernameField, passwordField, isNew,
            pageID: this.#pageID
        });
    }

    requestSave(origin, targetOrigin, username, password, usernameField, passwordField, isNew)
    {
        if (Ephy.isSandboxedWebContent()) {
            Ephy.log(`Not requesting to save password for origin=${origin} because web content is sandboxed`);
            return;
        }

        Ephy.log(`Requesting to save password for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}, isNew=${isNew}`);

        window.webkit.messageHandlers.passwordManagerRequestSave.postMessage({
            origin, targetOrigin, username, password, usernameField, passwordField, isNew,
            pageID: this.#pageID
        });
    }

    // Called by ephy-web-process-extension.c
    onQueryUsernamesResponse(users, id)
    {
        Ephy.log(`Received query usernames response with users=${users}`);

        const element = this.#takePendingPromise(id);
        if (element)
            element.resolver(users);
    }

    queryUsernames(origin)
    {
        if (Ephy.isSandboxedWebContent()) {
            Ephy.log(`Not querying usernames for origin=${origin} because web content is sandboxed`);
            return Promise.resolve(null);
        }

        Ephy.log(`Requesting usernames for origin=${origin}`);

        return new Promise((resolver, reject) => {
            const promiseID = this.#promiseCounter++;
            Ephy.queryUsernames(origin, promiseID, this.#pageID, this.#frameID);
            this.#pendingPromises.push({promiseID, resolver});
        });
    }
};

Ephy.FormManager = class FormManager
{
    static managers = [];

    #pageID;
    #frameID;
    #form;
    #passwordFormMessageSerializer;
    #preFillUserMenu = null;
    #elementBeingAutoFilled = null;
    #submissionHandled = false;

    constructor(pageID, frameID, form, serializer)
    {
        this.#pageID = pageID;
        this.#frameID = frameID;
        this.#form = form;
        this.#passwordFormMessageSerializer = serializer;

        this.#form.addEventListener('focus', this.#formFocused.bind(this), true);

        Ephy.FormManager.managers.push(this);
    }

    static managerForForm(element)
    {
        return Ephy.FormManager.managers.find((manager) => manager.#form === element);
    }

    frameID()
    {
        return this.#frameID;
    }

    form()
    {
        return this.#form;
    }

    isAutoFilling(element)
    {
        return this.#elementBeingAutoFilled === element;
    }

    preFillForms()
    {
        if (!Ephy.shouldRememberPasswords())
            return;

        const formAuth = this.#generateFormAuth(true);
        if (!formAuth) {
            Ephy.log('No pre-fillable/hookable form found');
            return;
        }

        Ephy.log('Hooking and pre-filling a form');

        if (formAuth.usernameNode) {
            Ephy.passwordManager.queryUsernames(formAuth.origin).then(users => {
                if (users.length > 1) {
                    Ephy.log('More than one saved username, hooking menu for choosing which one to select');
                    this.#preFillUserMenu = new Ephy.PreFillUserMenu(this, formAuth, users);
                } else if (users.length === 1) {
                    formAuth.username = users[0];
                }
                this.preFill(formAuth);
            });
        } else {
            this.preFill(formAuth);
        }
    }

    preFill(formAuth)
    {
        if (!Ephy.shouldRememberPasswords())
            return;

        Ephy.passwordManager.query(
            formAuth.origin,
            formAuth.targetOrigin,
            formAuth.username,
            formAuth.usernameField,
            formAuth.passwordField).then(authInfo => {
                if (!authInfo) {
                    Ephy.log('No passwords found for user ' + formAuth.username + ' at origin ' + formAuth.origin);
                    Ephy.log('Retrying password search with username and password fields as null');
                    Ephy.passwordManager.query(
                        formAuth.origin,
                        formAuth.targetOrigin,
                        formAuth.username,
                        null,
                        null).then(authInfo => {
                            if (!authInfo) {
                                Ephy.log('No passwords found for user ' + formAuth.username + ' at origin ' + formAuth.origin);
                                return;
                            }

                            this.#handlePasswordQuerySuccessResponse(formAuth, authInfo);
                        }
                    );
                    return;
                }

                this.#handlePasswordQuerySuccessResponse(formAuth, authInfo);
            }
        );
    }

    handleFormSubmission()
    {
        if (!Ephy.shouldRememberPasswords())
            return;

        if (this.#submissionHandled)
            return;

        this.#submissionHandled = true;

        const formAuth = this.#generateFormAuth(false);
        if (!formAuth || !formAuth.password)
            return;

        let permission = Ephy.permissionsManager.permission(Ephy.PermissionType.SAVE_PASSWORD, formAuth.origin);
        if (permission === Ephy.Permission.DENY) {
            Ephy.log('User/password storage permission previously denied. Not asking about storing.');
            return;
        }

        if (permission === Ephy.Permission.UNDECIDED && Ephy.isWebApplication())
            permission = Ephy.Permission.PERMIT;

        Ephy.passwordManager.query(
            formAuth.origin,
            formAuth.targetOrigin,
            formAuth.username,
            formAuth.usernameField,
            formAuth.passwordField).then(authInfo => {
                if (authInfo) {
                    if (authInfo.username === formAuth.username && authInfo.password === formAuth.password) {
                        Ephy.log('User/password already stored. Not asking about storing.');
                        return;
                    }

                    if (permission === Ephy.Permission.PERMIT) {
                        Ephy.log('User/password not yet stored. Storing.');
                        Ephy.passwordManager.save(formAuth.origin,
                                                  formAuth.targetOrigin,
                                                  formAuth.username,
                                                  formAuth.password,
                                                  formAuth.usernameField,
                                                  formAuth.passwordField,
                                                  false);
                        return;
                    }

                    Ephy.log('User/password not yet stored. Asking about storing.');
                } else {
                    Ephy.log('No result on query; asking whether we should store.');
                }

                Ephy.passwordManager.requestSave(formAuth.origin,
                                                 formAuth.targetOrigin,
                                                 formAuth.username,
                                                 formAuth.password,
                                                 formAuth.usernameField,
                                                 formAuth.passwordField,
                                                 authInfo === null);
            }
        );
    }

    #getFormAction()
    {
        // We used to naively access this.#form.action to get the action
        // attribute of the form, which works 99% of the time, but fails if any
        // input element of the form is named "action". We don't want to get
        // that by mistake, so we can't rely on the JavaScript property. See:
        // https://gitlab.gnome.org/GNOME/epiphany/-/issues/2114
        // https://stackoverflow.com/questions/18100630/
        //
        // One disadvantage of manually querying the attribute value rather
        // than directly accessing the property is the value may be a relative
        // path rather than a URL, so let's always convert to URL here.
        //
        // This may not actually be the real action of the form submission,
        // because the submitter element gets to override the action. However,
        // we don't know which element will be submitted yet, so we cannot
        // consider that for autofill purposes. It's OK because the target URL
        // we save into the password manager is only a heuristic.

        const action = this.#form.getAttribute('action');
        return action ? new URL(action, window.location) : null;
    }

    static #isNewPasswordElement(element)
    {
        return element.getAttribute('autocomplete').includes('new-password');
    }

    #containsPasswordElement()
    {
        for (const element of this.#form.elements) {
            if (element instanceof HTMLInputElement) {
                if (element.type === 'password' || element.type === 'adminpw')
                    return true;
            }
        }
        return false;
    }

    #formFocused(event)
    {
        if (!this.#containsPasswordElement())
            return;

        let isFormActionInsecure = false;
        const actionURL = this.#getFormAction();
        if (actionURL) {
            if (actionURL.protocol === 'http:') {
                // We trust localhost to be local since glib!616.
                const parts = actionURL.hostname.split('.');
                if (parts.length > 0) {
                    const tld = parts[parts.length - 1];
                    isFormActionInsecure = tld !== '127.0.0.1' && tld !== '::1' && tld !== 'localhost';
                }
            }
        }
        window.webkit.messageHandlers.passwordFormFocused.postMessage(this.#passwordFormMessageSerializer(this.#pageID, isFormActionInsecure));
    }

    #findPasswordFields()
    {
        const passwordFields = [];
        for (let i = 0; i < this.#form.elements.length; i++) {
            const element = this.#form.elements[i];
            if (element instanceof HTMLInputElement && element.type === 'password') {
                // We only want to process forms with 1-3 fields. A common
                // case is to have a "change password" form with 3 fields:
                // Old password, New password, Confirm new password.
                // Forms with more than 3 password fields are unlikely,
                // and we don't know how to process them, so reject them
                if (passwordFields.length === 3)
                    return null;
                passwordFields.push({ 'element' : element, 'index' : i });
            }
        }
        return passwordFields;
    }

    // forAutofill is true if we are loading the page and autofilling saved
    // passwords, and false if we are submitting the page and prompting the
    // user to save submitted passwords.
    #findFormAuthElements(forAutofill)
    {
        const passwordNodes = this.#findPasswordFields();
        if (!passwordNodes || !passwordNodes.length)
            return null;

        // Start at the first found password field and search backwards.
        // Assume the first eligible field to contain username.
        let usernameNode = null;
        const firstPasswordNodeData = passwordNodes[0];
        for (let i = firstPasswordNodeData.index; i >= 0; i--) {
            const element = this.#form.elements[i];
            if (element instanceof HTMLInputElement) {
                if (element.type === 'text' || element.type === 'email' ||
                    element.type === 'tel' || element.type === 'url' ||
                    element.type === 'number') {
                    usernameNode = element;
                    break;
                }
            }
        }

        // Choose password field that contains the password that we want to store
        // To do that, we compare the field values. We can only do this when user
        // submits login data, because otherwise all the fields are empty. In that
        // case just pick the first field.
        let passwordNodeIndex = 0;
        if (!forAutofill && passwordNodes.length !== 1) {
            for (const node of passwordNodes) {
                if (Ephy.FormManager.#isNewPasswordElement(node.element))
                    return { 'usernameNode' : usernameNode, 'passwordNode' : node };
            }

            // Get values of all password fields.
            const passwords = [];
            for (let i = passwordNodes.length - 1; i >= 0; i--)
                passwords[i] = passwordNodes[i].element.value;

            if (passwordNodes.length === 2) {
                // If there are two password fields, assume that the form has either
                // Password and Confirm password fields, or Old password and New password.
                // That can be guessed by comparing values in the fields. If they are
                // different, we assume that the second password is "new" and use it.
                // If they match, then just take the first field.
                if (passwords[0] === passwords[1]) {
                    // Password / Confirm password.
                    passwordNodeIndex = 0;
                } else {
                    // Old password / New password.
                    passwordNodeIndex = 1;
                }
            } else if (passwordNodes.length === 3) {
                // This is probably a complete Old password, New password, Confirm
                // new password case. Here we assume that if two fields have the same
                // value, then it's the new password and we should take it. A special
                // case is when all 3 passwords are different. We don't know what to
                // do in this case, so just reject the form.
                if (passwords[0] === passwords[1] && passwords[1] === passwords[2]) {
                    // All values are same.
                    passwordNodeIndex = 0;
                } else if (passwords[0] === passwords[1]) {
                    // New password / Confirm new password / Old password.
                    passwordNodeIndex = 0;
                } else if (passwords[0] === passwords[2]) {
                    // New password / Old password / Confirm new password.
                    passwordNodeIndex = 0;
                } else if (passwords[1] === passwords[2]) {
                    // Old password / New password / Confirm new password.
                    passwordNodeIndex = 1;
                } else {
                    // All values are different. Reject the form.
                    passwordNodeIndex = -1;
                }
            }
        }

        let passwordNode = null;
        if (passwordNodeIndex >= 0)
            passwordNode = passwordNodes[passwordNodeIndex].element;

        if (!passwordNode)
            return null;

        return { 'usernameNode' : usernameNode, 'passwordNode' : passwordNode };
    }

    #generateFormAuth(forAutofill) {
        const formAuth = this.#findFormAuthElements(forAutofill);
        if (formAuth === null)
            return null;

        formAuth.origin = new URL(String(window.location)).origin;
        try {
            formAuth.targetOrigin = this.#getFormAction().origin;
        } catch {
            formAuth.targetOrigin = formAuth.origin;
        }

        formAuth.username = null;
        if (formAuth.usernameNode && formAuth.usernameNode.value)
            formAuth.username = formAuth.usernameNode.value;

        formAuth.usernameField = null;
        if (formAuth.usernameNode) {
            // The name attribute is obsoleted by ID, but lots of websites have
            // missed that memo, so we should check both. We'll check name
            // before ID for compatibility with passwords saved by old versions
            // of Epiphany.
            if (formAuth.usernameNode.name)
                formAuth.usernameField = formAuth.usernameNode.name;
            else if (formAuth.usernameNode.id)
                formAuth.usernameField = formAuth.usernameNode.id;
        }

        formAuth.password = formAuth.passwordNode.value;

        if (formAuth.passwordNode.name)
            formAuth.passwordField = formAuth.passwordNode.name;
        else if (formAuth.passwordNode.id)
            formAuth.passwordField = formAuth.passwordNode.id;
        else
            return null;

        return formAuth;
    }

    #protectFormElement(element, value) {
        // Some misguided websites attempt to subvert password managers by
        // deleting autofilled values. If any FormManager has autofilled into an
        // input element, we should block the website from clearing it.
        //
        // Unfortunately this cannot be detected via an input or change event,
        // because HTMLInputElement only fires these events when the form values
        // are modified by the user, not when modified programmatically by
        // JavaScript.
        //
        // I tried using Object.defineProperty() to override the input element's
        // set function, as recommended by https://stackoverflow.com/a/55033939,
        // but this does not work. I'm not sure why.
        //
        // HTMLInputElement actually already has code to detect this scenario.
        // If the value is cleared by JavaScript rather than by the user, it
        // will call WebChromeClient::didProgrammaticallyClearTextFormControl.
        // Currently WebKit doesn't do anything with that event, but we could
        // pretty easily plumb it to InjectedBundlePageFormClient ->
        // APIInjectedBundleFormClient -> WebKitWebFormManager and create a
        // signal there for Epiphany to watch. This would probably be ideal.
        //
        // But it's even easier to just poll the element on an interval for a
        // few seconds. In practice, websites that try to clobber the autofilled
        // value will do so immediately. Wakeups are awful for battery life, so
        // stop after 5 seconds.
        let id = setInterval(() => {
            if (element.value === '') {
                Ephy.log(`Protected input ${element.id} has been cleared, re-autofilling...`);
                Ephy.autoFill(element, value);
            }
        }, 50); // 50 milliseconds

        setTimeout(() => {
            if (id !== 0) {
                clearInterval(id);
                id = 0;
            }
        }, 5000); // 5 seconds

        // Stop monitoring if the user edits the field, so we don't
        // re-autocomplete an autocompletion deleted by the user.
        element.addEventListener('input', (event) => {
            if (id !== 0) {
                clearInterval(id);
                id = 0;
            }
        });
    }

    #handlePasswordQuerySuccessResponse(formAuth, authInfo) {
        Ephy.log('Received success on password query for user ' + authInfo.username + ' with password (hidden)');

        if (formAuth.usernameNode && authInfo.username) {
            this.#elementBeingAutoFilled = formAuth.usernameNode;
            Ephy.autoFill(formAuth.usernameNode, authInfo.username);
            Ephy.log(`Autofilled usernameNode ${formAuth.usernameNode.id} with username ${authInfo.username}`);
            this.#elementBeingAutoFilled = null;
            this.#protectFormElement(formAuth.usernameNode, authInfo.username);
        }

        if (authInfo.password) {
            this.#elementBeingAutoFilled = formAuth.passwordNode;
            Ephy.autoFill(formAuth.passwordNode, authInfo.password);
            Ephy.log(`Autofilled passwordNode ${formAuth.passwordNode.id} with password (hidden)`);
            this.#elementBeingAutoFilled = null;
            this.#protectFormElement(formAuth.passwordNode, authInfo.password);
        }
    }
};

let contextMenuElementIsEditable = false;
let contextMenuElementIsPassword = false;

window.document.addEventListener('contextmenu', (event) => {
    // isContentEditable is always false, in practice this seems functional enough.
    contextMenuElementIsEditable = event.target.tagName.toLowerCase() === 'input';
    contextMenuElementIsPassword = event.target.type === 'password';
});
