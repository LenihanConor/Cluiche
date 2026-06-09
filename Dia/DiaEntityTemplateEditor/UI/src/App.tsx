import { useEffect, useState, CSSProperties } from 'react';
import { theme, EmptyState, ToastRenderer, useBridgeSubscribe, useBridgeRequest, useResizableDivider, useToast } from '@dia/editor-ui';
import type { NavigateFailedContext } from '@dia/editor-ui';
import { useTemplateEditorStore } from './store';
import type { BlueprintGroup, BlueprintProperties, UsageEntry, AvailableComponent } from './types';
import BlueprintList from './components/BlueprintList';
import PropertyPanel from './components/PropertyPanel';
import UsagePanel from './components/UsagePanel';
import ComponentPicker from './components/ComponentPicker';
import { NavigateFailedModal } from './components/NavigateFailedModal';
import { ConfirmDialog } from './components/ConfirmDialog';

interface ProjectStateResponse { isValid: boolean; diagamePath: string; }
interface ListResponse { success: boolean; groups: BlueprintGroup[]; }
interface LoadResponse { properties: BlueprintProperties; }
interface UsageResponse { usage: { usages: UsageEntry[] } | null; }
interface AvailableComponentsResponse { components: (AvailableComponent & { statusMessage?: string })[] | null; }
interface AddComponentResponse { success: boolean; }
interface RemoveComponentResponse { success: boolean; }
interface UpdateFieldResponse { success: boolean; }
interface CreateAssetResponse { absPath?: string; }

export default function App() {
    const projectValid = useTemplateEditorStore((s) => s.projectValid);
    const setProjectValid = useTemplateEditorStore((s) => s.setProjectValid);
    const groups = useTemplateEditorStore((s) => s.groups);
    const setGroups = useTemplateEditorStore((s) => s.setGroups);
    const selectedId = useTemplateEditorStore((s) => s.selectedId);
    const selectedPath = useTemplateEditorStore((s) => s.selectedPath);
    const setSelected = useTemplateEditorStore((s) => s.setSelected);
    const properties = useTemplateEditorStore((s) => s.properties);
    const setProperties = useTemplateEditorStore((s) => s.setProperties);
    const usage = useTemplateEditorStore((s) => s.usage);
    const setUsage = useTemplateEditorStore((s) => s.setUsage);
    const availableComponents = useTemplateEditorStore((s) => s.availableComponents);
    const setAvailableComponents = useTemplateEditorStore((s) => s.setAvailableComponents);
    const navigateFailedData = useTemplateEditorStore((s) => s.navigateFailedData);
    const setNavigateFailedData = useTemplateEditorStore((s) => s.setNavigateFailedData);

    const request = useBridgeRequest<unknown>();
    const { leftWidth, dividerProps } = useResizableDivider(180, 400);
    const toast = useToast();

    const [pickerOpen, setPickerOpen] = useState(false);
    const [removeTarget, setRemoveTarget] = useState<string | null>(null);

    const usageCount = usage.reduce((s, u) => s + u.instanceCount, 0);
    const sceneCount = usage.length;

    const reloadProperties = (path: string) => {
        (request as (topic: string, payload?: unknown) => Promise<LoadResponse>)
            ('entity_template_editor.load', { path })
            .then((r) => { if (r?.properties) setProperties(r.properties); })
            .catch(() => {});
    };

    const reloadAvailableComponents = (path: string) => {
        (request as (topic: string, payload?: unknown) => Promise<AvailableComponentsResponse>)
            ('entity_template_editor.get_available_components', { path })
            .then((r) => { setAvailableComponents(r?.components?.filter(c => !c.statusMessage) ?? []); })
            .catch(() => {});
    };

    const refreshList = () => {
        (request as (topic: string, payload?: unknown) => Promise<ListResponse>)
            ('entity_template_editor.get_list')
            .then((r) => { if (r?.success && r.groups) setGroups(r.groups); })
            .catch(() => {});
    };

    const selectBlueprint = (id: string, path: string) => {
        setSelected(id, path);

        (request as (topic: string, payload?: unknown) => Promise<LoadResponse>)
            ('entity_template_editor.load', { path })
            .then((r) => { if (r?.properties) setProperties(r.properties); })
            .catch(() => {});

        (request as (topic: string, payload?: unknown) => Promise<UsageResponse>)
            ('entity_template_editor.get_usage', { assetId: id })
            .then((r) => { setUsage(r?.usage?.usages ?? []); })
            .catch(() => {});

        (request as (topic: string, payload?: unknown) => Promise<AvailableComponentsResponse>)
            ('entity_template_editor.get_available_components', { path })
            .then((r) => { setAvailableComponents(r?.components?.filter(c => !c.statusMessage) ?? []); })
            .catch(() => {});
    };

    const onFieldChange = (componentType: string, fieldName: string, value: string | number | null) => {
        (request as (topic: string, payload?: unknown) => Promise<UpdateFieldResponse>)
            ('entity_template_editor.update_field', { path: selectedPath, componentType, fieldName, value })
            .then((r) => {
                if (r?.success === false) {
                    toast.push('Save error: update failed', 'error');
                } else {
                    toast.push('Saved', 'success');
                    if (selectedPath) reloadProperties(selectedPath);
                }
            })
            .catch((err: unknown) => {
                const msg = err instanceof Error ? err.message : String(err);
                toast.push(`Save error: ${msg}`, 'error');
            });
    };

    const onAddComponent = (typeId: string) => {
        (request as (topic: string, payload?: unknown) => Promise<AddComponentResponse>)
            ('entity_template_editor.add_component', { path: selectedPath, componentType: typeId })
            .then((r) => {
                if (r?.success === false) {
                    toast.push(`Add failed: could not add ${typeId}`, 'error');
                } else {
                    toast.push(`Added ${typeId}`, 'success');
                    setPickerOpen(false);
                    if (selectedPath) {
                        reloadProperties(selectedPath);
                        reloadAvailableComponents(selectedPath);
                    }
                }
            })
            .catch((err: unknown) => {
                const msg = err instanceof Error ? err.message : String(err);
                toast.push(`Add failed: ${msg}`, 'error');
            });
    };

    const onRemoveConfirm = () => {
        if (!removeTarget) return;
        const type = removeTarget;
        setRemoveTarget(null);
        (request as (topic: string, payload?: unknown) => Promise<RemoveComponentResponse>)
            ('entity_template_editor.remove_component', { path: selectedPath, componentType: type })
            .then((r) => {
                if (r?.success === false) {
                    toast.push(`Remove failed: could not remove ${type}`, 'error');
                } else {
                    toast.push(`Removed ${type}`, 'success');
                    if (selectedPath) {
                        reloadProperties(selectedPath);
                        reloadAvailableComponents(selectedPath);
                    }
                }
            })
            .catch((err: unknown) => {
                const msg = err instanceof Error ? err.message : String(err);
                toast.push(`Remove failed: ${msg}`, 'error');
            });
    };

    const onCreateFile = (context: NavigateFailedContext) => {
        (request as (topic: string, payload?: unknown) => Promise<CreateAssetResponse>)
            ('asset_catalogue.create_asset', { assetType: context.assetType, id: context.instanceId, source_path: context.expectedPath })
            .then((r) => {
                setNavigateFailedData(null);
                selectBlueprint(context.instanceId, r?.absPath || context.expectedPath);
                refreshList();
            })
            .catch(() => {});
    };

    const onRemoveEntry = (context: NavigateFailedContext) => {
        (request as (topic: string, payload?: unknown) => Promise<unknown>)
            ('asset_catalogue.delete_record', { id: context.instanceId })
            .then(() => {
                setNavigateFailedData(null);
                toast.push(`Removed entry ${context.instanceId}`, 'success');
                refreshList();
            })
            .catch(() => {});
    };

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
        {
            topic: 'entity_template_editor.navigated',
            handler: (data) => {
                const d = data as { id?: string; path?: string };
                if (d?.id && d?.path) selectBlueprint(d.id, d.path);
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
                <div data-testid="panel-list" style={{ width: leftWidth, minWidth: 180, overflow: 'hidden' }}>
                    <BlueprintList
                        groups={groups}
                        selectedId={selectedId}
                        onSelect={selectBlueprint}
                        onRefresh={refreshList}
                    />
                </div>
                <div style={dividerStyle} {...dividerProps} />
                <div data-testid="panel-center" style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
                    <PropertyPanel
                        properties={properties}
                        selectedPath={selectedPath}
                        onFieldChange={onFieldChange}
                        onRemoveComponent={(type) => setRemoveTarget(type)}
                        onAddComponentClick={() => setPickerOpen(true)}
                    />
                    <ComponentPicker
                        isOpen={pickerOpen}
                        components={availableComponents}
                        usageCount={usageCount}
                        sceneCount={sceneCount}
                        onAdd={onAddComponent}
                        onClose={() => setPickerOpen(false)}
                    />
                </div>
                <div data-testid="panel-right" style={panelRightStyle}>
                    <UsagePanel
                        usage={usage}
                        selectedId={selectedId}
                    />
                </div>
            </div>
            <NavigateFailedModal
                data={navigateFailedData}
                onCreateFile={onCreateFile}
                onRemoveEntry={onRemoveEntry}
                onDismiss={() => setNavigateFailedData(null)}
            />
            <ConfirmDialog
                isOpen={removeTarget !== null}
                title="Remove Component"
                message={`Remove component "${removeTarget ?? ''}" from this blueprint? This affects all instances.`}
                confirmLabel="Remove"
                variant="danger"
                onConfirm={onRemoveConfirm}
                onCancel={() => setRemoveTarget(null)}
            />
            <ToastRenderer />
        </div>
    );
}
