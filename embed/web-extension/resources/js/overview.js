Ephy.Overview = class Overview
{
    constructor(model)
    {
        this._model = model;
        this._items = [];

        // Event handlers are weak references in EphyWebOverviewModel, we need to keep
        // a strong reference to them while Ephy.Overview is alive.
        this._onURLsChangedFunction = this._onURLsChanged.bind(this);
        this._model.onurlschanged = this._onURLsChangedFunction;
        this._onThumbnailChangedFunction = this._onThumbnailChanged.bind(this);
        this._model.onthumbnailchanged = this._onThumbnailChangedFunction;
        this._onTitleChangedFunction = this._onTitleChanged.bind(this);
        this._model.ontitlechanged = this._onTitleChangedFunction;
        document.addEventListener('DOMContentLoaded', this._initialize.bind(this), false);
        document.addEventListener('keypress', this._onKeyPress.bind(this), false);
    }

    // Private

    _initialize()
    {
        let anchors = document.getElementsByTagName('a');
        for (let i = 0; i < anchors.length; i++) {
            let anchor = anchors[i];
            if (anchor.className != 'overview-item')
                continue;

            let item = new Ephy.Overview.Item(anchor);

            let closeButton = anchor.getElementsByClassName('overview-close-button')[0];
            closeButton.onclick = (event) => {
                this._removeItem(anchor);
                event.preventDefault();
            };

            // URLs and titles are always sent from the UI process, but thumbnails
            // aren't, so update the model with the thumbnail if there's one.
            let thumbnailPath = item.thumbnailPath();
            if (thumbnailPath)
                this._model.setThumbnail(item.url(), thumbnailPath);
            else
                item.setThumbnailPath(this._model.getThumbnail(item.url()));

            this._items.push(item);
        }
        let items = this._model.urls;
        if (items.length > this._items.length)
            this._onURLsChanged(items);
    }

    _onKeyPress(event)
    {
        if (event.which != 127)
            return;

        let item = document.activeElement;
        if (item.classList.contains('overview-item')) {
            this._removeItem(item);
            event.preventDefault();
        }
    }

    _removeItem(item)
    {
        item.classList.add('overview-removed');
        // Animation takes 0.75s, remove the item after 1s to ensure the animation finished.
        setTimeout(() => {
            item.parentNode.removeChild(item);
            for (let i = 0; i < this._items.length; i++) {
                if (this._items[i].url() == item.href) {
                    this._items.splice(i, 1);
                    break;
                }
            }
            window.webkit.messageHandlers.overview.postMessage(item.href);
        }, 1000);
    }

    _onURLsChanged(urls)
    {
        let overview = document.getElementById('overview');
        if (overview.classList.contains('overview-empty')) {
            while (overview.lastChild)
                overview.removeChild(overview.lastChild);
            overview.classList.remove('overview-empty');
        }

        for (let i = 0; i < urls.length; i++) {
            let url = urls[i];

            let item;
            if (this._items[i]) {
                item = this._items[i];
            } else {
                Ephy.log('create an item for the url ' + url.url);
                let anchor = document.createElement('a');
                anchor.classList.add('overview-item');
                let closeButton = document.createElement('div');
                closeButton.title = Ephy._("Remove from overview");
                closeButton.onclick = (event) => {
                    this._removeItem(anchor);
                    event.preventDefault();
                };
                closeButton.innerHTML = '&#10006;';
                closeButton.classList.add('overview-close-button');
                anchor.appendChild(closeButton);
                let thumbnailSpan = document.createElement('span');
                thumbnailSpan.classList.add('overview-thumbnail');
                anchor.appendChild(thumbnailSpan);
                let titleSpan = document.createElement('span');
                titleSpan.classList.add('overview-title');
                anchor.appendChild(titleSpan);
                document.getElementById('overview').appendChild(anchor);
                item = new Ephy.Overview.Item(anchor);
                this._items.push(item);
            }

            item.setURL(url.url);
            item.setTitle(url.title);
            item.setThumbnailPath(this._model.getThumbnail(url.url));
        }

        while (this._items.length > urls.length) {
            let item = this._items.pop();
            item.detachFromParent();
        }
    }

    _onThumbnailChanged(url, path)
    {
        for (let i = 0; i < this._items.length; i++) {
            let item = this._items[i];
            if (item.url() == url) {
                item.setThumbnailPath(path);
                return;
            }
        }
    }

    _onTitleChanged(url, title)
    {
        for (let i = 0; i < this._items.length; i++) {
            let item = this._items[i];
            if (item.url() == url) {
                item.setTitle(title);
                return;
            }
        }
    }
};

Ephy.Overview.Item = class OverviewItem
{
    constructor(item)
    {
        this._item = item;
        this._title = null;
        this._thumbnail = null;

        for (let i = 0; i < this._item.childNodes.length; i++) {
            let child = this._item.childNodes[i];
            if (!(child instanceof Element))
                continue;

            if (child.classList.contains('overview-title'))
                this._title = child;
            else if (child.classList.contains('overview-thumbnail'))
                this._thumbnail = child;
        }
    }

    // Public

    url()
    {
        return this._item.href;
    }

    setURL(url)
    {
        this._item.href = url;
    }

    title()
    {
        return this._item.title;
    }

    setTitle(title)
    {
        this._item.title = title;
        this._title.textContent = title;
    }

    thumbnailPath()
    {
        let style = this._thumbnail.style;
        if (style.isPropertyImplicit('background'))
            return null;

        let background = style.getPropertyValue('background');
        if (!background)
            return null;

        if (background.startsWith('url("file://'))
            return background.replace('url("file://', '').replace('") no-repeat', '');

        return null;
    }

    setThumbnailPath(path)
    {
        if (path)
            this._thumbnail.style.background = 'url(file://' + path + ') no-repeat';
        else
            this._thumbnail.style.background = null;
    }

    detachFromParent()
    {
        this._item.parentNode.removeChild(this._item);
    }
};
