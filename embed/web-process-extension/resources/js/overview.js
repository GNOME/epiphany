'use strict';

Ephy.Overview = class Overview
{
    constructor(model)
    {
        this._model = model;
        this._items = [];

        this._pendingThumbnailChanges = [];
        this._pendingTitleChanges = [];

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

        for (let thumbnailChange of this._pendingThumbnailChanges)
            this._onThumbnailChanged(thumbnailChange.url, thumbnailChange.path);
        this._pendingThumbnailChanges = [];

        for (let titleChange of this._pendingTitleChanges)
            this._onTitleChanged(titleChange.url, titleChange.title);
        this._pendingTitleChanges = [];
        this._addPlaceholders();
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

    _addPlaceholders() {
        let parentNode = document.getElementById('most-visited-grid');
        let anchors = document.getElementsByTagName('a');

        for (let i = anchors.length; i < 9; i++) {
            let anchor = document.createElement('a');
            anchor.className = 'overview-item';
            let span_thumbnail = document.createElement('span');
            span_thumbnail.className = 'overview-thumbnail';
            anchor.appendChild(span_thumbnail);
            let span_title = document.createElement('span');
            span_title.className = 'overview-title';
            anchor.appendChild(span_title);

            parentNode.appendChild(anchor);
        }
      }

    _removePlaceholders() {
        let anchors = document.getElementsByTagName('a')

        for (let anchor of anchors) {
            if (anchor.href == '')
                document.removeChild(anchor);
        }
    }

    _removeItem(item)
    {
        item.classList.add('overview-removed');
        // Animation takes 0.25s, remove the item after 0.5s to ensure the animation finished.
        setTimeout(() => {
            item.parentNode.removeChild(item);
            for (let i = 0; i < this._items.length; i++) {
                if (this._items[i].url() == item.href) {
                    this._items.splice(i, 1);
                    break;
                }
            }
            this._addPlaceholders();
            window.webkit.messageHandlers.overview.postMessage(item.href);
        }, 500);  // This value needs to be synced with the one in about.css
    }

    _onURLsChanged(urls)
    {
        let overview = document.getElementById('overview');
        let grid = document.getElementById('most-visited-grid');
        if (overview.classList.contains('overview-empty')) {
            while (grid.lastChild)
                grid.removeChild(grid.lastChild);
            overview.classList.remove('overview-empty');
        }

        this._removePlaceholders();
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
                closeButton.innerHTML = '';
                closeButton.classList.add('overview-close-button');
                anchor.appendChild(closeButton);
                let thumbnailSpan = document.createElement('span');
                thumbnailSpan.classList.add('overview-thumbnail');
                anchor.appendChild(thumbnailSpan);
                let titleSpan = document.createElement('span');
                titleSpan.classList.add('overview-title');
                anchor.appendChild(titleSpan);
                document.getElementById('most-visited-grid').appendChild(anchor);
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
        if (this._items.length == 0) {
            this._pendingThumbnailChanges.push({ url: url, path: path });
            return;
        }

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
        if (this._items.length == 0) {
            this._pendingTitleChanges.push({ url: url, title: title });
            return;
        }

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
            return background.replace('url("file://', '').replace('"); background-size: 100%', '');

        return null;
    }

    setThumbnailPath(path)
    {
        if (path) {
            this._thumbnail.style.backgroundImage = 'url(file://' + path + ')';
            this._thumbnail.style.backgroundSize = '100%';
            this._thumbnail.style.backgroundPosition = 'top';
        } else {
            this._thumbnail.style.backgroundImage = '';
            this._thumbnail.style.backgroundSize = 'auto';
            this._thumbnail.style.backgroundPosition = 'center';
        }
    }

    detachFromParent()
    {
        this._item.parentNode.removeChild(this._item);
    }
};
