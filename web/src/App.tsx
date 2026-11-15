import { useEffect, useRef, useState } from 'react';
import { backend } from './generated/backend';
import type { GoItem } from './generated/schema';
import { useGoResults } from './state';

// Dumb renderer over the native palette model: draw `results`, forward input.
// Matching, ranking and selection all happen in C++.
export default function App() {
    const results = useGoResults();
    const [query, setQuery] = useState('');
    const inputRef = useRef<HTMLInputElement>(null);
    const selectedRef = useRef<HTMLLIElement>(null);

    // On open: the input shows the native query (the current URL for ⌘L,
    // empty for ⌘T), fully selected, so ⌘C copies it and typing replaces it.
    useEffect(() => {
        setQuery(results.query);
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
                        <li className="empty">Nothing here yet</li>
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
    | 'closeTabRow'
    | 'complete'
    | 'activate'
    | 'activateInNewTab'
    | 'cancel';

const ctrlKeyActions = new Map<string, KeyAction>([
    ['j', 'down'],
    ['n', 'down'],
    ['k', 'up'],
    ['p', 'up'],
    ['s', 'toggleBookmark'],
    ['x', 'clear'],
    ['w', 'closeTabRow'],
]);

const plainKeyActions = new Map<string, KeyAction>([
    ['ArrowDown', 'down'],
    ['ArrowUp', 'up'],
    ['Tab', 'complete'],
    ['Enter', 'activate'],
    ['Escape', 'cancel'],
]);

const isNewTabEnter = (e: React.KeyboardEvent) =>
    e.key === 'Enter' && (e.metaKey || e.ctrlKey);

function findKeyAction(
    e: React.KeyboardEvent,
    isCaretAtEnd: () => boolean,
): KeyAction | undefined {
    if (isNewTabEnter(e)) return 'activateInNewTab';
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

    const handlers: Record<KeyAction, () => void> = {
        down: () => void backend.moveSelection({ delta: 1 }),
        up: () => void backend.moveSelection({ delta: -1 }),
        toggleBookmark: () => item?.canStar && void backend.toggleBookmark(item),
        clear: () => item?.canClose && void backend.closeItem(item),
        closeTabRow: () => item?.kind === 'tab' && void backend.closeTabRow(item),
        complete: completeSelection,
        activate: () => void backend.activate({ inNewTab: false }),
        activateInNewTab: () => void backend.activate({ inNewTab: true }),
        cancel: () => void backend.cancel(),
    };

    handlers[action]();
}

const chooseRow = (item: GoItem, e: React.MouseEvent) =>
    void backend.choose({ item, inNewTab: e.metaKey || e.ctrlKey });

// The row's kind, in an icon and a word, so rows of different kinds never look alike.
function getKindLabel(item: GoItem): { icon: string; label: string; className: string } {
    if (item.failed) return { icon: '!', label: "Didn't load", className: 'failed' };
    if (item.isCurrent) return { icon: '▣', label: 'Current', className: 'current' };
    if (item.kind === 'tab') return { icon: '▣', label: 'Switch to tab', className: 'tab' };
    if (item.kind === 'place') return { icon: '★', label: 'Starred', className: 'place' };
    if (item.kind === 'history') return { icon: '◷', label: 'History', className: 'history' };
    if (item.kind === 'url') return { icon: '⊕', label: 'Open', className: 'url' };
    return { icon: '⌕', label: 'Search', className: 'search' };
}

function StarButton({ item }: { item: GoItem }) {
    return (
        <button
            className={`star ${item.bookmarked ? 'active' : ''}`}
            title={item.bookmarked ? 'Unstar (^S)' : 'Star (^S)'}
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
            title={item.kind === 'tab' ? 'Close tab (^X)' : 'Forget this page (^X)'}
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
    const kind = getKindLabel(item);
    const isAction = item.kind === 'search' || item.kind === 'url';

    return (
        <li
            ref={rowRef}
            className={`row ${kind.className} ${selected ? 'selected' : ''}`}
            onClick={(e) => chooseRow(item, e)}
        >
            <span className="icon">{kind.icon}</span>

            <span className="text">
                <span className="title">{item.title || item.url}</span>
                {!isAction && <span className="url">{item.url}</span>}
            </span>

            {!isAction && <span className="kind">{kind.label}</span>}
            {item.canStar && <StarButton item={item} />}
            {item.canClose && <CloseButton item={item} />}
        </li>
    );
}
