import { useEffect, CSSProperties } from 'react';
import { theme, EmptyState, ToastRenderer, useBridgeSubscribe, useBridgeRequest, useResizableDivider } from '@dia/editor-ui';
import { useTemplateEditorStore } from './store';
import type { BlueprintGroup } from './types';

interface ProjectStateResponse { isValid: boolean; diagamePath: string; }
interface ListResponse { success: boolean; groups: BlueprintGroup[]; }

export default function App() {
    const projectValid = useTemplateEditorStore((s) => s.projectValid);
    const setProjectValid = useTemplateEditorStore((s) => s.setProjectValid);
    const setGroups = useTemplateEditorStore((s) => s.setGroups);
    const setNavigateFailedData = useTemplateEditorStore((s) => s.setNavigateFailedData);

    const request = useBridgeRequest<unknown>();
    const { leftWidth, dividerProps } = useResizableDivider(180, 400);

    // On mount: poll project state + load list
    useEffect(() => {
        (request as (topic: string, payload?: unknown) => Promise<ProjectStateResponse>)
            ('entity_template_editor.get_project_state')
            .then((r) => { if (r?.isValid !== undefined) setProjectValid(r.isValid); })
            .catch(() => {});

        (request as (topic: string, payload?: unknown) => Promise<ListResponse>)
            ('entity_template_editor.get_list')
            .then((r) => { if (r?.success && r.groups) setGroups(r.groups); })
            .catch(() => {});
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    // Push subscriptions
    useBridgeSubscribe([
        {
            topic: 'entity_template_editor.project_changed',
            handler: (data) => {
                const d = data as { isValid?: boolean };
                if (d?.isValid !== undefined) setProjectValid(d.isValid);
            },
        },
        {
            topic: 'asset_catalogue.registry_changed',
            handler: () => {
                (request as (topic: string, payload?: unknown) => Promise<ListResponse>)
                    ('entity_template_editor.get_list')
                    .then((r) => { if (r?.success && r.groups) setGroups(r.groups); })
                    .catch(() => {});
            },
        },
        {
            topic: 'entity_template_editor.navigate_failed',
            handler: (data) => {
                setNavigateFailedData(data as Parameters<typeof setNavigateFailedData>[0]);
            },
        },
    ]);

    const rootStyle: CSSProperties = {
        display: 'flex',
        flexDirection: 'column',
        height: '100vh',
        background: theme.bg,
        color: theme.text,
        fontFamily: "'Segoe UI', system-ui, sans-serif",
        fontSize: 12,
        overflow: 'hidden',
        position: 'relative',
    };

    const layoutStyle: CSSProperties = {
        display: 'flex',
        flex: 1,
        overflow: 'hidden',
    };

    const dividerStyle: CSSProperties = {
        width: 4,
        cursor: 'col-resize',
        background: theme.border,
        flexShrink: 0,
    };

    const panelRightStyle: CSSProperties = {
        width: 280,
        minWidth: 200,
        borderLeft: `1px solid ${theme.border}`,
        overflow: 'hidden',
    };

    return (
        <div style={rootStyle} data-testid="app-root">
            {!projectValid && (
                <div
                    data-testid="project-overlay"
                    style={{
                        position: 'absolute', top: 0, left: 0, right: 0, bottom: 0,
                        background: 'rgba(0,0,0,0.7)',
                        zIndex: 100,
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                    }}
                >
                    <EmptyState
                        message="No project loaded"
                        hint="Open a .diagame project to use the blueprint editor"
                        icon="📄"
                    />
                </div>
            )}
            <div style={layoutStyle}>
                <div data-testid="panel-list" style={{ width: leftWidth, minWidth: 180, overflow: 'hidden' }} />
                <div style={dividerStyle} {...dividerProps} />
                <div data-testid="panel-center" style={{ flex: 1, overflow: 'hidden' }} />
                <div data-testid="panel-right" style={panelRightStyle} />
            </div>
            <ToastRenderer />
        </div>
    );
}
