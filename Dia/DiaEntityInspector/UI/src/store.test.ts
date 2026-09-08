import { describe, it, expect, beforeEach } from 'vitest';
import { useInspectorStore } from './store';

beforeEach(() => {
    useInspectorStore.setState({
        connected: false,
        entities: [],
        queries: [],
        mailbox: [],
        watchItems: [],
        selectedIdx: null,
        activeTab: 'fields',
        frame: 0,
        entityCount: 0,
    });
});

describe('InspectorStore', () => {
    it('initializes with default values', () => {
        const state = useInspectorStore.getState();
        expect(state.connected).toBe(false);
        expect(state.entities).toEqual([]);
        expect(state.queries).toEqual([]);
        expect(state.mailbox).toEqual([]);
        expect(state.watchItems).toEqual([]);
        expect(state.selectedIdx).toBeNull();
        expect(state.activeTab).toBe('fields');
        expect(state.frame).toBe(0);
        expect(state.entityCount).toBe(0);
    });

    it('setConnected updates connected state', () => {
        useInspectorStore.getState().setConnected(true);
        expect(useInspectorStore.getState().connected).toBe(true);
        useInspectorStore.getState().setConnected(false);
        expect(useInspectorStore.getState().connected).toBe(false);
    });

    it('setEntities replaces entities array', () => {
        const entities = [{ i: 0, g: 1, n: 'A', t: ['T'], d: 0 }];
        useInspectorStore.getState().setEntities(entities);
        expect(useInspectorStore.getState().entities).toEqual(entities);
    });

    it('setQueries replaces queries array', () => {
        const queries = [{ sig: 'Q1', count: 2, members: ['A'] }];
        useInspectorStore.getState().setQueries(queries);
        expect(useInspectorStore.getState().queries).toEqual(queries);
    });

    it('setMailbox replaces mailbox array', () => {
        const mail = [{ f: 1, s: 'A', a: 'B', type: 'dmg' }];
        useInspectorStore.getState().setMailbox(mail);
        expect(useInspectorStore.getState().mailbox).toEqual(mail);
    });

    it('clearMailbox resets mailbox to empty', () => {
        useInspectorStore.getState().setMailbox([{ f: 1, s: 'A', a: 'B', type: 'x' }]);
        useInspectorStore.getState().clearMailbox();
        expect(useInspectorStore.getState().mailbox).toEqual([]);
    });

    it('clearMailbox is idempotent on empty mailbox', () => {
        useInspectorStore.getState().clearMailbox();
        expect(useInspectorStore.getState().mailbox).toEqual([]);
    });

    it('setSelectedIdx accepts number or null', () => {
        useInspectorStore.getState().setSelectedIdx(5);
        expect(useInspectorStore.getState().selectedIdx).toBe(5);
        useInspectorStore.getState().setSelectedIdx(null);
        expect(useInspectorStore.getState().selectedIdx).toBeNull();
    });

    it('setActiveTab updates active tab', () => {
        useInspectorStore.getState().setActiveTab('mailbox');
        expect(useInspectorStore.getState().activeTab).toBe('mailbox');
        useInspectorStore.getState().setActiveTab('watch');
        expect(useInspectorStore.getState().activeTab).toBe('watch');
    });

    it('setFrame and setEntityCount update frame state', () => {
        useInspectorStore.getState().setFrame(100);
        useInspectorStore.getState().setEntityCount(42);
        expect(useInspectorStore.getState().frame).toBe(100);
        expect(useInspectorStore.getState().entityCount).toBe(42);
    });

    it('setWatchItems replaces watch items', () => {
        const items = [{ e: 'Player', c: 'Transform', f: 'x', v: '10', d: 'up' }];
        useInspectorStore.getState().setWatchItems(items);
        expect(useInspectorStore.getState().watchItems).toEqual(items);
    });
});
