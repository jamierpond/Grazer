#pragma once

#include <string>

// Vimium-style bindings, injected into every page at document start -- which
// includes the native error page, so a tab whose load failed stays fully
// drivable. The page's own shortcuts win: a key the page handles (Gmail's j/k,
// GitHub's '/') never reaches the bindings. Add new bindings to the
// `bindings` map below -- keys are keypress sequences ('gg' works, as long as
// no binding is a strict prefix of another), values run when the sequence
// completes. `post(command)` sends a command string to the native side,
// handled in BrowserView::handleCommand. Sites can opt out of bindings that
// clash with their own shortcuts -- see SitesStore, whose JSON vimiumScript()
// splices in for __grazerExcludedKeybindings.
static constexpr auto vimiumScriptSource = R"JS(
(() => {
    if (window.__grazerInstalled)
        return;
    window.__grazerInstalled = true;

    const post = command =>
        window.webkit?.messageHandlers?.grazer?.postMessage(command);

    const step = 60;
    const half = () => window.innerHeight / 2;

    const bindings = {
        'j': () => scrollBy(0, step),
        'k': () => scrollBy(0, -step),
        'h': () => scrollBy(-step, 0),
        'l': () => scrollBy(step, 0),
        'd': () => scrollBy(0, half()),
        'u': () => scrollBy(0, -half()),
        'gg': () => scrollTo(0, 0),
        'G': () => scrollTo(0, document.documentElement.scrollHeight),
        'H': () => history.back(),
        'L': () => history.forward(),
        'r': () => location.reload(),
        'o': () => post('openPalette'),
        't': () => post('openPalette'),
        'b': () => post('toggleBookmark'),
        'x': () => post('closeTab'),
        'J': () => post('prevTab'),
        'K': () => post('nextTab'),
        'f': () => enterHintMode(),
        'n': () => find.next(),
        'N': () => find.previous(),
    };

    // host -> keys the site keeps (SitesStore); a host covers its subdomains.
    const excludedKeybindings = __grazerExcludedKeybindings;

    function allowedHere(seq) {
        const host = location.hostname.toLowerCase();
        const site = Object.keys(excludedKeybindings).find(
            site => host === site || host.endsWith('.' + site));
        const excluded = site ? excludedKeybindings[site] : [];
        return !excluded.some(key => key === 'all' || seq.startsWith(key));
    }

    // --- Find in page (Cmd+F) ----------------------------------------------
    //
    // A bar pinned top-right, in a closed shadow root so the page's CSS can't
    // reach it. Every match is painted with the CSS Custom Highlight API --
    // the page's DOM is never touched -- and the current one is scrolled to.
    // Typing searches; Enter / Shift+Enter step; Escape closes and clears.
    // The native side drives it through window.__grazerFind
    // (BrowserView::findInPage): Cmd+F opens, Cmd+G / Shift-Cmd+G step.
    // Lowercase queries ignore case; any capital makes them match it, and a
    // space matches any run of whitespace.

    const find = (() => {
        const maxMatches = 2000;

        const pageSheet = new CSSStyleSheet();
        pageSheet.replaceSync(
            '::highlight(grazer-find) { background: rgba(255, 215, 110, .45); }'
            + '::highlight(grazer-find-current) { background: #ff9f2e;'
            + ' color: #000; }');

        const barSheet = new CSSStyleSheet();
        barSheet.replaceSync(`
            .bar { display: flex; align-items: center; gap: 8px;
                   padding: 7px 8px 7px 12px;
                   font: 13px -apple-system, BlinkMacSystemFont, sans-serif;
                   background: rgba(24, 24, 28, .96);
                   border: 1px solid rgba(255, 255, 255, .12);
                   border-radius: 10px;
                   box-shadow: 0 8px 28px rgba(0, 0, 0, .45); }
            input { width: 200px; padding: 0; font: inherit; color: #f2f2f5;
                    background: transparent; border: none; outline: none; }
            input::placeholder { color: rgba(255, 255, 255, .35); }
            .bar.none input { color: #e0645c; }
            .count { color: rgba(255, 255, 255, .45); font-size: 11px;
                     white-space: nowrap; }
            .close { padding: 2px 5px; font: inherit; line-height: 1;
                     color: rgba(255, 255, 255, .35); background: none;
                     border: none; cursor: pointer; }
            .close:hover { color: #fff; }`);

        let bar = null;
        let query = '';
        let matches = [];
        let current = -1;

        function pattern(text) {
            const source = text.trim()
                .replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
                .replace(/\s+/g, '\\s+');
            const ignoreCase = text === text.toLowerCase();
            return source ? new RegExp(source, ignoreCase ? 'gi' : 'g') : null;
        }

        // Matches inside one text node; one split across elements is missed.
        function search(text) {
            const regex = pattern(text);
            const found = [];

            if (!regex)
                return found;

            const seen = new Map();
            const visible = el => {
                if (!seen.has(el))
                    seen.set(el, !el.closest(
                            'script, style, noscript, template, textarea')
                        && (el.checkVisibility?.({visibilityProperty: true})
                            ?? true));
                return seen.get(el);
            };

            const walker = document.createTreeWalker(
                document.body ?? document.documentElement,
                NodeFilter.SHOW_TEXT);

            for (let node; (node = walker.nextNode())
                     && found.length < maxMatches;) {
                if (!node.parentElement || !visible(node.parentElement))
                    continue;

                for (const match of node.data.matchAll(regex)) {
                    const range = new Range();
                    range.setStart(node, match.index);
                    range.setEnd(node, match.index + match[0].length);
                    found.push(range);

                    if (found.length >= maxMatches)
                        break;
                }
            }

            return found;
        }

        // The first match at or after `anchor` (a range), else the first one
        // on screen -- so typing more never jumps back past the current hit.
        function startIndex(anchor) {
            if (anchor?.startContainer.isConnected) {
                const i = matches.findIndex(range =>
                    range.compareBoundaryPoints(Range.START_TO_START, anchor)
                        >= 0);
                if (i >= 0)
                    return i;
            }

            const i = matches.findIndex(
                range => range.getBoundingClientRect().bottom > 0);
            return Math.max(i, 0);
        }

        function run(anchor) {
            matches = search(query);
            current = matches.length ? startIndex(anchor) : -1;
            paint();
            reveal();
        }

        function paint() {
            if (window.CSS?.highlights) {
                if (!document.adoptedStyleSheets.includes(pageSheet))
                    document.adoptedStyleSheets =
                        [...document.adoptedStyleSheets, pageSheet];

                const now = new Highlight(...(current >= 0
                    ? [matches[current]] : []));
                now.priority = 1;
                CSS.highlights.set('grazer-find', new Highlight(...matches));
                CSS.highlights.set('grazer-find-current', now);
            }

            if (!bar)
                return;

            const total = matches.length;
            bar.count.textContent = total
                ? `${current + 1} of ${total}${total >= maxMatches ? '+' : ''}`
                : query.trim() ? 'No matches' : '';
            bar.box.classList.toggle('none', !total && !!query.trim());
        }

        // Scroll the current match into view, clear of the bar.
        function reveal() {
            const range = matches[current];

            if (!range)
                return;

            range.startContainer.parentElement?.scrollIntoView(
                {block: 'nearest', inline: 'nearest'});

            const rect = range.getBoundingClientRect();
            if (rect.top < 60 || rect.bottom > innerHeight - 20)
                scrollBy(0, rect.top - innerHeight / 3);
        }

        function step(backwards) {
            if (!query.trim())
                return;

            // Closed (highlights cleared) or the page changed under us: search
            // again from where the reader is, and land on that.
            if (!matches.length || !matches[0].startContainer.isConnected) {
                run(null);
                return;
            }

            const total = matches.length;
            current = (current + (backwards ? -1 : 1) + total) % total;
            paint();
            reveal();
        }

        function build() {
            const host = document.createElement('grazer-find');
            host.style.cssText = 'all:initial;position:fixed;top:10px;'
                + 'right:14px;z-index:2147483647;';

            const root = host.attachShadow({mode: 'closed'});
            root.adoptedStyleSheets = [barSheet];

            const box = document.createElement('div');
            box.className = 'bar';

            const input = document.createElement('input');
            input.placeholder = 'Find in page';
            input.spellcheck = false;
            input.addEventListener('input', () => {
                query = input.value;
                run(matches[current]);
            });

            const count = document.createElement('span');
            count.className = 'count';

            const close = document.createElement('button');
            close.className = 'close';
            close.title = 'Close (Esc)';
            close.textContent = '✕';
            close.addEventListener('click', () => api.close());

            box.append(input, count, close);
            root.append(box);
            document.documentElement.appendChild(host);
            return {host, box, input, count};
        }

        const api = {
            open() {
                if (!bar) {
                    bar = build();
                    bar.input.value = query;

                    if (query.trim())
                        run(null);
                }

                bar.input.focus();
                bar.input.select();
            },

            next: () => step(false),
            previous: () => step(true),

            close() {
                bar?.host.remove();
                bar = null;
                matches = [];
                current = -1;
                CSS.highlights?.delete('grazer-find');
                CSS.highlights?.delete('grazer-find-current');
            },

            isOpen: () => bar !== null,

            // The bar's input has focus. Its shadow root is closed, so the
            // page only ever sees the host as the focused element.
            hasFocus: () => bar !== null && document.activeElement === bar.host,

            // Keys typed into the bar. The page never sees them: its own
            // shortcuts would fire, since to the page the target isn't an
            // input. Typing itself is the default action and goes through.
            handleKey(event) {
                event.stopImmediatePropagation();

                if (event.isComposing || event.metaKey || event.ctrlKey
                    || event.altKey)
                    return;

                if (event.key === 'Escape') {
                    event.preventDefault();
                    api.close();
                } else if (event.key === 'Enter') {
                    event.preventDefault();
                    step(event.shiftKey);
                }
            },
        };

        return api;
    })();

    window.__grazerFind = find;

    // --- Link hints ('f') ------------------------------------------------

    const hintChars = 'asdfghjkl';
    let hintState = null;

    function hintLabels(count) {
        let length = 1;
        while (Math.pow(hintChars.length, length) < count)
            length++;

        return Array.from({length: count}, (_, i) => {
            let n = i, label = '';
            for (let d = 0; d < length; d++) {
                label = hintChars[n % hintChars.length] + label;
                n = Math.floor(n / hintChars.length);
            }
            return label;
        });
    }

    function clickables() {
        const selector = 'a[href], button, input, select, textarea, summary, '
            + '[onclick], [role="button"], [role="link"], [tabindex]';
        return [...document.querySelectorAll(selector)].filter(el => {
            const r = el.getBoundingClientRect();
            return r.width > 0 && r.height > 0
                && r.bottom > 0 && r.top < innerHeight
                && r.right > 0 && r.left < innerWidth
                && getComputedStyle(el).visibility !== 'hidden';
        });
    }

    function enterHintMode() {
        const els = clickables();
        if (!els.length)
            return;

        const labels = hintLabels(els.length);
        const overlay = document.createElement('div');
        overlay.style.cssText =
            'position:fixed;inset:0;z-index:2147483647;pointer-events:none;';

        els.forEach((el, i) => {
            const r = el.getBoundingClientRect();
            const tag = document.createElement('span');
            tag.textContent = labels[i].toUpperCase();
            tag.dataset.label = labels[i];
            tag.style.cssText =
                `position:fixed;left:${Math.max(0, r.left)}px;`
                + `top:${Math.max(0, r.top)}px;`
                + 'background:#ffd76e;color:#302505;'
                + 'font:bold 11px/1.3 -apple-system,sans-serif;'
                + 'padding:1px 3px;border:1px solid #c38a22;border-radius:3px;'
                + 'box-shadow:0 1px 2px rgba(0,0,0,.4);';
            overlay.appendChild(tag);
        });

        document.documentElement.appendChild(overlay);
        hintState = {els, labels, typed: '', overlay};
    }

    function exitHintMode() {
        hintState?.overlay.remove();
        hintState = null;
    }

    function activate(el) {
        el.focus();
        const editable = el.matches(
            'textarea, select, [contenteditable], input:not([type=button])'
            + ':not([type=submit]):not([type=checkbox]):not([type=radio])');
        if (!editable)
            el.click();
    }

    function handleHintKey(event) {
        if (event.key === 'Escape' || !hintChars.includes(event.key)) {
            exitHintMode();
            return;
        }

        hintState.typed += event.key;
        const matches = hintState.labels.filter(
            label => label.startsWith(hintState.typed));

        if (matches.length === 1 && matches[0] === hintState.typed) {
            const el = hintState.els[hintState.labels.indexOf(matches[0])];
            exitHintMode();
            activate(el);
            return;
        }

        if (!matches.length) {
            exitHintMode();
            return;
        }

        [...hintState.overlay.children].forEach(tag => {
            tag.style.display =
                tag.dataset.label.startsWith(hintState.typed) ? '' : 'none';
        });
    }

    // --- Key dispatch -----------------------------------------------------

    let pending = '';
    let pendingTimer = 0;

    function isEditable(el) {
        return el && (el.isContentEditable
            || ['INPUT', 'TEXTAREA', 'SELECT'].includes(el.tagName));
    }

    addEventListener('keydown', event => {
        if (event.metaKey && !event.ctrlKey && !event.altKey
            && event.key === 'l') {
            event.preventDefault();
            event.stopImmediatePropagation();
            post('openPalette');
            return;
        }

        // Ctrl+N: a new window. macOS takes Cmd+N in the menu bar; Windows
        // has none, so the page takes it.
        if (!navigator.platform.startsWith('Mac') && event.ctrlKey
            && !event.metaKey && !event.altKey && !event.shiftKey
            && event.key.toLowerCase() === 'n') {
            event.preventDefault();
            event.stopImmediatePropagation();
            post('newWindow');
            return;
        }

        if (find.hasFocus()) {
            find.handleKey(event);
            return;
        }

        if (event.metaKey || event.ctrlKey || event.altKey)
            return;

        if (hintState) {
            event.preventDefault();
            event.stopImmediatePropagation();
            handleHintKey(event);
            return;
        }

        if (isEditable(document.activeElement)) {
            if (event.key === 'Escape')
                document.activeElement.blur();
            return;
        }

        if (event.key === 'Escape' && find.isOpen()) {
            event.preventDefault();
            find.close();
            return;
        }

        if (event.key.length !== 1)
            pending = '';
    }, true);

    // The bindings run on keypress, bubbling, so the page sees every key
    // first -- on keydown and keypress, wherever it listens -- and a key it
    // handles (preventDefault, or stopping it before window) is left alone.
    // A page that cancels keydown gets no keypress at all.
    addEventListener('keypress', event => {
        if (event.defaultPrevented) {
            pending = '';
            return;
        }

        if (event.metaKey || event.ctrlKey || event.altKey || hintState
            || isEditable(document.activeElement)
            || event.key.length !== 1)
            return;

        const candidate = pending + event.key;
        clearTimeout(pendingTimer);

        if (bindings[candidate] && allowedHere(candidate)) {
            pending = '';
            event.preventDefault();
            bindings[candidate]();
            return;
        }

        const isPrefix = Object.keys(bindings)
            .some(seq => seq.startsWith(candidate) && allowedHere(seq));

        if (isPrefix) {
            pending = candidate;
            event.preventDefault();
            pendingTimer = setTimeout(() => { pending = ''; }, 800);
            return;
        }

        pending = '';
    });

    for (const type of ['keypress', 'keyup'])
        addEventListener(type, event => {
            if (find.hasFocus())
                event.stopImmediatePropagation();
        }, true);
})();
)JS";

inline std::string vimiumScript(const std::string& excludedKeybindingsJSON)
{
    auto script = std::string(vimiumScriptSource);
    auto token = std::string("__grazerExcludedKeybindings");
    script.replace(script.find(token), token.size(), excludedKeybindingsJSON);
    return script;
}
