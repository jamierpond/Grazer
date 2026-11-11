import { useEffect, useRef, useState } from 'react';
import { backend } from './generated/backend';
import type { GoItem } from './generated/schema';
import { useGoResults } from './state';

// Dumb renderer over the native palette model: draw `results`, forward input.
// Matching, ranking, selection and live preview all happen in C++.
export default function App() {
    const results = useGoResults();
    const [query, setQuery] = useState('');
    const inputRef = useRef<HTMLInputElement>(null);
    const selectedRef = useRef<HTMLLIElement>(null);

    // On open: pre-fill the input with the current URL, fully selected — it
    // does NOT filter (the native query starts empty), it's there so
    // ⌘L → ⌘A → ⌘C copies the URL, and any typing replaces it.
    useEffect(() => {
        setQuery(results.currentUrl);
        const focus = () => {
            inputRef.current?.focus();
            inputRef.current?.select();
        };
        focus();
        const late = setTimeout(focus, 60);
        return () => clearTimeout(late);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [results.generation]);

    useEffect(() => {
        selectedRef.current?.scrollIntoView({ block: 'nearest' });
    }, [results.selected, results.items]);

    const onQueryChange = (text: string) => {
        setQuery(text);
        void backend.setQuery({ text });
    };

    const isCaretAtEnd = () =>
        inputRef.current?.selectionStart === query.length &&
        inputRef.current?.selectionEnd === query.length;

    // Take the selected row's URL into the input to keep typing; the
    // native query follows, so Enter opens what's typed.
    const completeSelection = () => {
        void backend.complete().then(({ text }) => {
            if (!text) return;
            setQuery(text);
            requestAnimationFrame(() =>
                inputRef.current?.setSelectionRange(text.length, text.length),
            );
        });
    };

    const onKeyDown = (e: React.KeyboardEvent) => {
        const action = findKeyAction(e, isCaretAtEnd);
        if (!action) return;
        e.preventDefault();
        runKeyAction(action, results, completeSelection);
    };

    return (
        <div className="backdrop" onMouseDown={() => void backend.cancel()}>
            <div className="dragstrip" />
            <div className="palette" onMouseDown={(e) => e.stopPropagation()}>
                <input
                    ref={inputRef}
                    className="query"
                    value={query}
                    placeholder="Go to a place, tab, search, or URL…"
                    spellCheck={false}
                    autoFocus
                    onChange={(e) => onQueryChange(e.target.value)}
                    onKeyDown={onKeyDown}
                />

                <ul className="items">
                    {/* Index keys on purpose: every publish replaces the whole
                        list, and URL-based keys can collide (the pinned search
                        row vs a history row for the same search), which makes
                        React leave stale rows behind and skip highlights. */}
                    {results.items.map((item, index) => (
                        <Row
                            key={index}
                            item={item}
                            selected={index === results.selected}
                            rowRef={index === results.selected ? selectedRef : undefined}
                        />
                    ))}

                    {results.items.length === 0 && (
                        <li className="empty">
                            {query
                                ? `Enter ↳ search for “${query}”`
                                : 'Nothing here yet'}
                        </li>
                    )}
                </ul>
            </div>
        </div>
    );
}

type KeyAction =
    | 'down'
    | 'up'
    | 'toggleBookmark'
    | 'clear'
    | 'complete'
    | 'activate'
    | 'cancel';

const ctrlKeyActions = new Map<string, KeyAction>([
    ['j', 'down'],
    ['n', 'down'],
    ['k', 'up'],
    ['p', 'up'],
    ['s', 'toggleBookmark'],
    ['x', 'clear'],
]);

const plainKeyActions = new Map<string, KeyAction>([
    ['ArrowDown', 'down'],
    ['ArrowUp', 'up'],
    ['Tab', 'complete'],
    ['Enter', 'activate'],
    ['Escape', 'cancel'],
]);

function findKeyAction(
    e: React.KeyboardEvent,
    isCaretAtEnd: () => boolean,
): KeyAction | undefined {
    const plain = plainKeyActions.get(e.key);
    if (plain) return plain;
    if (e.key === 'ArrowRight' && isCaretAtEnd()) return 'complete';
    if (e.ctrlKey) return ctrlKeyActions.get(e.key);
    return undefined;
}

function runKeyAction(
    action: KeyAction,
    results: { items: GoItem[]; selected: number },
    completeSelection: () => void,
) {
    const item = results.items[results.selected];
    const realItem = item && !item.isSearch ? item : undefined;

    const handlers: Record<KeyAction, () => void> = {
        down: () => void backend.moveSelection({ delta: 1 }),
        up: () => void backend.moveSelection({ delta: -1 }),
        toggleBookmark: () => realItem && void backend.toggleBookmark(realItem),
        clear: () => realItem && void backend.closeItem(realItem),
        complete: completeSelection,
        activate: () => void backend.activate(),
        cancel: () => void backend.cancel(),
    };

    handlers[action]();
}

function SearchRow({
    item,
    selected,
    rowRef,
}: {
    item: GoItem;
    selected: boolean;
    rowRef?: React.Ref<HTMLLIElement>;
}) {
    return (
        <li
            ref={rowRef}
            className={`row search ${selected ? 'selected' : ''}`}
            onClick={() => void backend.choose(item)}
        >
            <span className="magnifier">⌕</span>
            <span className="text">
                <span className="title">{item.title}</span>
            </span>
        </li>
    );
}

function getDotClass(item: GoItem) {
    if (item.failed) return 'dead';
    if (item.tabId >= 0) return 'open';
    return '';
}

function StarButton({ item }: { item: GoItem }) {
    return (
        <button
            className={`star ${item.bookmarked ? 'active' : ''}`}
            title={item.bookmarked ? 'Remove from places' : 'Save as place'}
            onClick={(e) => {
                e.stopPropagation();
                void backend.toggleBookmark(item);
            }}
        >
            {item.bookmarked ? '★' : '☆'}
        </button>
    );
}

function CloseButton({ item }: { item: GoItem }) {
    return (
        <button
            className="close"
            title={item.tabId >= 0 ? 'Close tab (^X)' : 'Clear suggestion (^X)'}
            onClick={(e) => {
                e.stopPropagation();
                void backend.closeItem(item);
            }}
        >
            ✕
        </button>
    );
}

function Row({
    item,
    selected,
    rowRef,
}: {
    item: GoItem;
    selected: boolean;
    rowRef?: React.Ref<HTMLLIElement>;
}) {
    if (item.isSearch) {
        return <SearchRow item={item} selected={selected} rowRef={rowRef} />;
    }

    return (
        <li
            ref={rowRef}
            className={`row ${selected ? 'selected' : ''} ${item.failed ? 'failed' : ''}`}
            onClick={() => void backend.choose(item)}
        >
            <span className={`dot ${getDotClass(item)}`} />

            <span className="text">
                <span className="title">{item.title || item.url}</span>
                <span className="url">{item.url}</span>
            </span>

            <StarButton item={item} />
            <CloseButton item={item} />
        </li>
    );
}
