'use strict';

Ephy.Overview = class Overview
{
    #model;
    #items = [];
    #pendingThumbnailChanges = [];
    #pendingTitleChanges = [];
    #onURLsChangedFunction;
    #onThumbnailChangedFunction;
    #onTitleChangedFunction;

    constructor(model)
    {
        this.#model = model;

        // Event handlers are weak references in EphyWebOverviewModel, we need to keep
        // a strong reference to them while Ephy.Overview is alive.
        this.#onURLsChangedFunction = this.#onURLsChanged.bind(this);
        this.#model.onurlschanged = this.#onURLsChangedFunction;
        this.#onThumbnailChangedFunction = this.#onThumbnailChanged.bind(this);
        this.#model.onthumbnailchanged = this.#onThumbnailChangedFunction;
        this.#onTitleChangedFunction = this.#onTitleChanged.bind(this);
        this.#model.ontitlechanged = this.#onTitleChangedFunction;
        document.addEventListener('DOMContentLoaded', this.#initialize.bind(this), false);
        document.addEventListener('keypress', this.#onKeyPress.bind(this), false);
    }

    #initialize()
    {
        const anchors = document.getElementsByTagName('a');
        if (anchors.length === 0)
            return;

        for (let i = 0; i < anchors.length; i++) {
            const anchor = anchors[i];
            if (anchor.className !== 'overview-item')
                continue;

            const item = new Ephy.Overview.Item(anchor);

            const closeButton = anchor.getElementsByClassName('overview-close-button')[0];
            closeButton.onclick = (event) => {
                this.#removeItem(anchor);
                event.preventDefault();
            };

            // URLs and titles are always sent from the UI process, but thumbnails
            // aren't, so update the model with the thumbnail if there's one.
            const thumbnailPath = item.thumbnailPath();
            if (thumbnailPath)
                this.#model.setThumbnail(item.url(), thumbnailPath);
            else
                item.setThumbnailPath(this.#model.getThumbnail(item.url()));

            this.#items.push(item);
        }

        const items = this.#model.urls;
        if (items.length > this.#items.length)
            this.#onURLsChanged(items);

        for (const thumbnailChange of this.#pendingThumbnailChanges)
            this.#onThumbnailChanged(thumbnailChange.url, thumbnailChange.path);
        this.#pendingThumbnailChanges = [];

        for (const titleChange of this.#pendingTitleChanges)
            this.#onTitleChanged(titleChange.url, titleChange.title);
        this.#pendingTitleChanges = [];
        this.#addPlaceholders();
    }

    #onKeyPress(event)
    {
        if (event.which !== 127)
            return;

        const item = document.activeElement;
        if (item.classList.contains('overview-item')) {
            this.#removeItem(item);
            event.preventDefault();
        }
    }

    #addPlaceholders() {
        const parentNode = document.getElementById('most-visited-grid');
        const anchors = document.getElementsByTagName('a');

        for (let i = anchors.length; i < 9; i++) {
            const anchor = document.createElement('a');
            anchor.className = 'overview-item';
            const spanThumbnail = document.createElement('span');
            spanThumbnail.className = 'overview-thumbnail';
            anchor.appendChild(spanThumbnail);
            const spanTitle = document.createElement('span');
            spanTitle.className = 'overview-title';
            anchor.appendChild(spanTitle);

            parentNode.appendChild(anchor);
        }
      }

    #removePlaceholders() {
        const anchors = document.getElementsByTagName('a');

        for (const anchor of anchors) {
            if (anchor.href === '')
                document.removeChild(anchor);
        }
    }

    #removeItem(item)
    {
        item.classList.add('overview-removed');
        // Animation takes 0.25s, remove the item after 0.5s to ensure the animation finished.
        setTimeout(() => {
            item.parentNode.removeChild(item);
            for (let i = 0; i < this.#items.length; i++) {
                if (this.#items[i].url() === item.href) {
                    this.#items.splice(i, 1);
                    break;
                }
            }
            this.#addPlaceholders();
            window.webkit.messageHandlers.overview.postMessage(item.href);
        }, 500);  // This value needs to be synced with the one in about.css
    }

    #onURLsChanged(urls)
    {
        const overview = document.getElementById('overview');
        const grid = document.getElementById('most-visited-grid');
        if (overview.classList.contains('overview-empty')) {
            while (grid.lastChild)
                grid.removeChild(grid.lastChild);
            overview.classList.remove('overview-empty');
        }

        this.#removePlaceholders();
        for (let i = 0; i < urls.length; i++) {
            const url = urls[i];

            let item;
            if (this.#items[i]) {
                item = this.#items[i];
            } else {
                Ephy.log('create an item for the url ' + url.url);
                const anchor = document.createElement('a');
                anchor.classList.add('overview-item');
                const closeButton = document.createElement('div');
                closeButton.title = Ephy._('Remove from overview');
                closeButton.onclick = (event) => {
                    this.#removeItem(anchor);
                    event.preventDefault();
                };
                closeButton.innerHTML = '';
                closeButton.classList.add('overview-close-button');
                anchor.appendChild(closeButton);
                const thumbnailSpan = document.createElement('span');
                thumbnailSpan.classList.add('overview-thumbnail');
                anchor.appendChild(thumbnailSpan);
                const titleSpan = document.createElement('span');
                titleSpan.classList.add('overview-title');
                anchor.appendChild(titleSpan);
                document.getElementById('most-visited-grid').appendChild(anchor);
                item = new Ephy.Overview.Item(anchor);
                this.#items.push(item);
            }

            item.setURL(url.url);
            item.setTitle(url.title);
            item.setThumbnailPath(this.#model.getThumbnail(url.url));
        }

        while (this.#items.length > urls.length) {
            const item = this.#items.pop();
            item.detachFromParent();
        }
    }

    #onThumbnailChanged(url, path)
    {
        if (this.#items.length === 0) {
            this.#pendingThumbnailChanges.push({ url: url, path: path });
            return;
        }

        for (let i = 0; i < this.#items.length; i++) {
            const item = this.#items[i];
            if (item.url() === url) {
                item.setThumbnailPath(path);
                return;
            }
        }
    }

    #onTitleChanged(url, title)
    {
        if (this.#items.length === 0) {
            this.#pendingTitleChanges.push({ url: url, title: title });
            return;
        }

        for (let i = 0; i < this.#items.length; i++) {
            const item = this.#items[i];
            if (item.url() === url) {
                item.setTitle(title);
                return;
            }
        }
    }
};

Ephy.Overview.Item = class OverviewItem
{
    #item;
    #title = null;
    #thumbnail = null;

    constructor(item)
    {
        this.#item = item;

        for (const child of this.#item.childNodes) {
            if (!(child instanceof Element))
                continue;

            if (child.classList.contains('overview-title'))
                this.#title = child;
            else if (child.classList.contains('overview-thumbnail'))
                this.#thumbnail = child;
        }
    }

    url()
    {
        return this.#item.href;
    }

    setURL(url)
    {
        this.#item.href = url;
    }

    title()
    {
        return this.#item.title;
    }

    setTitle(title)
    {
        this.#item.title = title;
        this.#title.textContent = title;
    }

    thumbnailPath()
    {
        const background = this.#thumbnail.style.getPropertyValue('background');
        if (!background)
            return null;

        if (background.startsWith('url("file://'))
            return background.replace('url("file://', '').replace('"); background-size: 100%', '');

        return null;
    }

    setThumbnailPath(path)
    {
        if (path) {
            this.#thumbnail.style.backgroundImage = 'url(file://' + path + ')';
            this.#thumbnail.style.backgroundSize = '100%';
            this.#thumbnail.style.backgroundPosition = 'top';
        } else {
            this.#thumbnail.style.backgroundImage = '';
            this.#thumbnail.style.backgroundSize = 'auto';
            this.#thumbnail.style.backgroundPosition = 'center';
        }
    }

    detachFromParent()
    {
        this.#item.parentNode.removeChild(this.#item);
    }
};
