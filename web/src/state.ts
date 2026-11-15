import { backend } from './generated/backend';
import { makeBridgeStore } from './generated/react';
import type { GoResults } from './generated/schema';

const initial: GoResults = {
    items: [],
    selected: -1,
    generation: 0,
    query: '',
};

export const useGoResults = makeBridgeStore({
    backend,
    event: 'results',
    fetch: backend.getResults,
    initial,
});
