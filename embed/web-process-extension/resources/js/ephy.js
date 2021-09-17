'use strict';

// eslint-disable-next-line no-var
var Ephy = {};

Ephy.getAppleMobileWebAppCapable = function()
{
    const metas = document.getElementsByTagName('meta');

    for (let i = 0; i < metas.length; i++) {
        const meta = metas[i];

        // https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
        if (meta.name === 'apple-mobile-web-app-capable' && meta.getAttribute('content') === 'yes')
            return true;
    }

    return false;
};

Ephy.getWebAppTitle = function()
{
    const metas = document.getElementsByTagName('meta');

    for (let i = 0; i < metas.length; i++) {
        const meta = metas[i];
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
    let ogpIcon = null;
    const links = document.getElementsByTagName('link');
    const metas = document.getElementsByTagName('meta');

    for (let i = 0; i < links.length; i++) {
        const link = links[i];

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
            for (let j = 0; j < sizesList.length; j++) {
                const size = sizesList[j].toLowerCase().split('x');

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

    for (let i = 0; i < metas.length; i++) {
        const meta = metas[i];

        if (meta.name === 'msapplication-TileImage')
            msIconURL = meta.content;
        else if (meta.name === 'msapplication-TileColor')
            iconColor = meta.content;
        else if (meta.getAttribute('property') === 'og:image' || meta.getAttribute('itemprop') === 'image')
            ogpIcon = meta.content;
    }

    // msapplication icon.
    if (msIconURL)
        return { 'url' : new URL(msIconURL, baseURL).href, 'color' : iconColor };

    // ogp icon.
    if (ogpIcon)
        return { 'url' : new URL(ogpIcon, baseURL).href, 'color' : null };

    // html icon without known size
    if (htmlIconURL)
        return { 'url' : new URL(htmlIconURL, baseURL).href, 'color' : null };

    // Last ditch effort: just fallback to the default favicon location.
    return { 'url' : new URL('/favicon.ico', baseURL).href, 'color' : null };
};

Ephy.PreFillUserMenu = class PreFillUserMenu
{
    constructor(manager, formAuth, users)
    {
        this._manager = manager;
        this._formAuth = formAuth;
        this._userElement = formAuth.usernameNode;
        this._users = users;
        this._passwordElement = formAuth.passwordNode;
        this._selected = null;
        this._wasEdited = false;

        this._userElement.addEventListener('input', this._onInput.bind(this), true);
        this._userElement.addEventListener('mouseup', this._onMouseUp.bind(this), false);
        this._userElement.addEventListener('keydown', this._onKeyDown.bind(this), false);
        this._userElement.addEventListener('change', this._removeMenu, false);
        this._userElement.addEventListener('blur', this._removeMenu, false);
    }

    // Private

    _onInput(event)
    {
        if (this._manager.isAutoFilling(this._userElement))
            return;

        this._wasEdited = true;
        this._removeMenu();
        this._showMenu(false);
    }

    _onMouseUp(event)
    {
        if (document.getElementById('ephy-user-choices-container'))
            return;

        this._showMenu(!this._wasEdited);
    }

    _onKeyDown(event)
    {
        if (event.key === 'Escape') {
            this._removeMenu();
            return;
        }

        if (event.key !== 'ArrowDown' && event.key !== 'ArrowUp')
            return;

        const container = document.getElementById('ephy-user-choices-container');
        if (!container) {
            this._showMenu(!this._wasEdited);
            return;
        }

        let newSelect = null;
        if (this._selected)
            newSelect = event.key !== 'ArrowUp' ? this._selected.previousSibling : this._selected.nextSibling;

        if (!newSelect)
            newSelect = event.key !== 'ArrowUp' ? container.firstElementChild.lastElementChild : container.firstElementChild.firstElementChild;

        if (newSelect) {
            this._selected = newSelect;
            this._userElement.value = this._selected.firstElementChild.textContent;
            this._usernameSelected();
        } else {
            this._passwordElement.value = '';
        }

        event.preventDefault();
    }

    _showMenu(showAll)
    {
        const mainDiv = document.createElement('div');
        mainDiv.id = 'ephy-user-choices-container';

        const elementRect = this._userElement.getBoundingClientRect();

        // 2147483647 is the maximum value browsers will take for z-index.
        // See http://stackoverflow.com/questions/8565821/css-max-z-index-value
        mainDiv.style.cssText = 'position: absolute;' +
            'z-index: 2147483647;' +
            'cursor: default;' +
            'background-color: white;' +
            'box-shadow: 5px 5px 5px rgba(0,0,0,0.2);' +
            'border-top: 0px;' +
            'border-radius: 8px;' +
            'padding: 12px 0px;' +
            '-webkit-user-modify: read-only ! important;';
        mainDiv.style.width = this._userElement.offsetWidth + 'px';
        mainDiv.style.left = elementRect.left + document.body.scrollLeft + 'px';
        mainDiv.style.top = elementRect.top + elementRect.height + document.body.scrollTop + 'px';

        const ul = document.createElement('ul');
        ul.style.cssText = 'margin: 0; padding: 0;';
        ul.tabindex = -1;
        mainDiv.appendChild(ul);

        this._selected = null;
        for (let i = 0; i < this._users.length; i++) {
            const user = this._users[i];
            if (!showAll && !user.startsWith(this._userElement.value))
                continue;

            const li = document.createElement('li');
            li.style.cssText = 'list-style-type: none ! important;' +
                'background-image: none ! important;' +
                'padding: 3px 6px ! important;' +
                'color: black;' +
                'margin: 0px;';
            // FIXME: selection colors.
            li.tabindex = -1;
            ul.appendChild(li);

            if (user === this._userElement.value)
                this._selected = li;

            const anchor = document.createElement('a');
            anchor.style.cssText = 'font-weight: normal ! important;' +
                'font-family: sans ! important;' +
                'text-decoration: none ! important;' +
                'color: black;' +
                '-webkit-user-modify: read-only ! important;';
            // FIXME: selection colors.
            anchor.textContent = user;
            li.appendChild(anchor);

            li.addEventListener('mousedown', event => {
                this._userElement.value = user;
                this._selected = li;
                this._removeMenu();
                this._usernameSelected();
            }, true);
        }

        document.body.appendChild(mainDiv);

        if (!this._selected)
            this._passwordElement.value = '';
    }

    _removeMenu()
    {
        const menu = document.getElementById('ephy-user-choices-container');
        if (menu)
            menu.parentNode.removeChild(menu);
    }

    _usernameSelected()
    {
        this._formAuth.username = this._userElement.value;
        this._passwordElement.value = '';
        this._manager.preFill(this._formAuth);
    }
};

Ephy.formControlsAssociated = function(pageID, frameID, forms, serializer)
{
    Ephy.formManagers = [];

    for (let i = 0; i < forms.length; i++) {
        if (!(forms[i] instanceof HTMLFormElement))
            continue;
        const formManager = new Ephy.FormManager(pageID, frameID, forms[i]);
        formManager.handlePasswordForms(serializer);
        formManager.preFillForms();
        Ephy.formManagers.push(formManager);
    }
};

Ephy.handleFormSubmission = function(pageID, frameID, form)
{
    // FIXME: Find out: is it really possible to have multiple frames with same window object???
    let formManager = null;
    for (let i = 0; i < Ephy.formManagers.length; i++) {
        const manager = Ephy.formManagers[i];
        if (manager.frameID() === frameID && manager.form() === form) {
            formManager = manager;
            break;
        }
    }

    if (!formManager) {
        formManager = new Ephy.FormManager(pageID, frameID, form);
        Ephy.formManagers.push(formManager);
    }

    formManager.handleFormSubmission();
};

Ephy.hasModifiedForms = function()
{
    for (let i = 0; i < document.forms.length; i++) {
        const form = document.forms[i];
        let modifiedInputElement = false;
        for (let j = 0; j < form.elements.length; j++) {
            const element = form.elements[j];
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

Ephy.PasswordManager = class PasswordManager
{
    constructor(pageID, frameID)
    {
        this._pageID = pageID;
        this._frameID = frameID;
        this._pendingPromises = [];
        this._promiseCounter = 0;
    }

    _takePendingPromise(id)
    {
        const element = this._pendingPromises.find(element => element.promiseID === id);
        if (element)
            this._pendingPromises = this._pendingPromises.filter(element => element.promiseID !== id);
        return element;
    }

    _onQueryResponse(username, password, id)
    {
        Ephy.log(`Received password query response for username=${username}`);

        const element = this._takePendingPromise(id);
        if (element) {
            if (password)
                element.resolver({username, password});
            else
                element.resolver(null);
        }
    }

    query(origin, targetOrigin, username, usernameField, passwordField)
    {
        Ephy.log(`Querying passwords for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}`);

        return new Promise((resolver, reject) => {
            const promiseID = this._promiseCounter++;
            Ephy.queryPassword(origin, targetOrigin, username, usernameField, passwordField, promiseID, this._pageID, this._frameID);
            this._pendingPromises.push({promiseID, resolver});
        });
    }

    save(origin, targetOrigin, username, password, usernameField, passwordField, isNew)
    {
        Ephy.log(`Saving password for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}, isNew=${isNew}`);

        window.webkit.messageHandlers.passwordManagerSave.postMessage({
            origin, targetOrigin, username, password, usernameField, passwordField, isNew,
            pageID: this._pageID
        });
    }

    // FIXME: Why is pageID a parameter here?
    requestSave(origin, targetOrigin, username, password, usernameField, passwordField, isNew, pageID)
    {
        Ephy.log(`Requesting to save password for origin=${origin}, targetOrigin=${targetOrigin}, username=${username}, usernameField=${usernameField}, passwordField=${passwordField}, isNew=${isNew}`);

        window.webkit.messageHandlers.passwordManagerRequestSave.postMessage({
            origin, targetOrigin, username, password, usernameField, passwordField, isNew,
            pageID
        });
    }

    _onQueryUsernamesResponse(users, id)
    {
        Ephy.log(`Received query usernames response with users=${users}`);

        const element = this._takePendingPromise(id);
        if (element)
            element.resolver(users);
    }

    queryUsernames(origin)
    {
        Ephy.log(`Requesting usernames for origin=${origin}`);

        return new Promise((resolver, reject) => {
            const promiseID = this._promiseCounter++;
            Ephy.queryUsernames(origin, promiseID, this._pageID, this._frameID);
            this._pendingPromises.push({promiseID, resolver});
        });
    }
};

Ephy.FormManager = class FormManager
{
    constructor(pageID, frameID, form)
    {
        this._pageID = pageID;
        this._frameID = frameID;
        this._form = form;
        this._passwordFormMessageSerializer = null;
        this._preFillUserMenu = null;
        this._elementBeingAutoFilled = null;
    }

    // Public

    frameID()
    {
        return this._frameID;
    }

    form()
    {
        return this._form;
    }

    handlePasswordForms(serializer)
    {
        if (!this._containsPasswordElement())
            return;

        Ephy.log('Password form element detected, hooking password form focused callback');
        this._passwordFormMessageSerializer = serializer;
        this._form.addEventListener('focus', this._passwordFormFocused.bind(this), true);
    }

    isAutoFilling(element)
    {
        return this._elementBeingAutoFilled === element;
    }

    preFillForms()
    {
        if (!Ephy.shouldRememberPasswords())
            return;

        const formAuth = this._generateFormAuth(true);
        if (!formAuth) {
            Ephy.log('No pre-fillable/hookable form found');
            return;
        }

        Ephy.log('Hooking and pre-filling a form');

        if (formAuth.usernameNode) {
            Ephy.passwordManager.queryUsernames(formAuth.origin).then(users => {
                if (users.length > 1) {
                    Ephy.log('More than one saved username, hooking menu for choosing which one to select');
                    this._preFillUserMenu = new Ephy.PreFillUserMenu(this, formAuth, users);
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
                    Ephy.log('No result');
                    return;
                }

                Ephy.log('Found: user ' + authInfo.username + ' pass (hidden)');

                if (formAuth.usernameNode && authInfo.username) {
                    this._elementBeingAutoFilled = formAuth.usernameNode;
                    Ephy.autoFill(formAuth.usernameNode, authInfo.username);
                    this._elementBeingAutoFilled = null;
                }

                if (authInfo.password) {
                    this._elementBeingAutoFilled = formAuth.passwordNode;
                    Ephy.autoFill(formAuth.passwordNode, authInfo.password);
                    this._elementBeingAutoFilled = null;
                }
            }
        );
    }

    handleFormSubmission()
    {
        if (!Ephy.shouldRememberPasswords())
            return;

        const formAuth = this._generateFormAuth(false);
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
                                                 authInfo === null,
                                                 this._pageID);
            }
        );
    }

    // Private

    _containsPasswordElement()
    {
        for (let i = 0; i < this._form.elements.length; i++) {
            const element = this._form.elements[i];
            if (element instanceof HTMLInputElement) {
                if (element.type === 'password' || element.type === 'adminpw')
                    return true;
            }
        }
        return false;
    }

    _passwordFormFocused(event)
    {
        let isFormActionInsecure = false;
        if (this._form.action) {
            const url = new URL(this._form.action);
            if (url.protocol === 'http:') {
                // We trust localhost to be local since glib!616.
                const parts = url.hostname.split('.');
                if (parts.length > 0) {
                    const tld = parts[parts.length - 1];
                    isFormActionInsecure = tld !== '127.0.0.1' && tld !== '::1' && tld !== 'localhost';
                }
            }
        }
        window.webkit.messageHandlers.passwordFormFocused.postMessage(this._passwordFormMessageSerializer(this._pageID, isFormActionInsecure));
    }

    _findPasswordFields()
    {
        const passwordFields = [];
        for (let i = 0; i < this._form.elements.length; i++) {
            const element = this._form.elements[i];
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

    _findFormAuthElements(forAutofill)
    {
        const passwordNodes = this._findPasswordFields();
        if (!passwordNodes || !passwordNodes.length)
            return null;

        // Start at the first found password field and search backwards.
        // Assume the first eligible field to contain username.
        let usernameNode = null;
        const firstPasswordNodeData = passwordNodes[0];
        for (let i = firstPasswordNodeData.index; i >= 0; i--) {
            const element = this._form.elements[i];
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

    _generateFormAuth(forAutofill) {
        const formAuth = this._findFormAuthElements(forAutofill);
        if (formAuth === null)
            return null;

        formAuth.origin = new URL(String(window.location)).origin;
        try {
            formAuth.targetOrigin = new URL(this._form.action).origin;
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
};
