import { CSSProperties } from 'react';
import { theme, buttonStyle, EmptyState } from '@dia/editor-ui';
import type { BlueprintProperties, ComponentEntry, FieldEntry } from '../types';
import ComponentAccordion from './ComponentAccordion';
import { FieldRow } from './FieldRow';

interface PropertyPanelProps {
    properties: BlueprintProperties | null;
    onFieldChange: (componentType: string, fieldName: string, value: string | number | null) => void;
    onRemoveComponent: (type: string) => void;
    onAddComponentClick: () => void;
}

const containerStyle: CSSProperties = {
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
    background: theme.bg,
    color: theme.text,
    overflow: 'hidden',
};

const headerStyle: CSSProperties = {
    padding: '8px 12px',
    fontSize: '13px',
    fontWeight: 600,
    color: theme.text,
    borderBottom: `1px solid ${theme.border}`,
    flexShrink: 0,
    userSelect: 'none',
};

const bodyStyle: CSSProperties = {
    flex: 1,
    overflowY: 'auto',
};

const footerStyle: CSSProperties = {
    padding: '8px 12px',
    borderTop: `1px solid ${theme.border}`,
    flexShrink: 0,
};

export function PropertyPanel({
    properties,
    onFieldChange,
    onRemoveComponent,
    onAddComponentClick,
}: Readonly<PropertyPanelProps>) {
    if (properties === null) {
        return (
            <EmptyState
                message="Select a blueprint"
                hint="Choose a blueprint from the list to view and edit it"
            />
        );
    }

    return (
        <div style={containerStyle}>
            <div style={headerStyle} data-testid="blueprint-id">
                {`⬡ ${properties.id}`}
            </div>
            <div style={bodyStyle}>
                {properties.components.map((component: ComponentEntry) => (
                    <ComponentAccordion
                        key={component.type}
                        component={component}
                        onRemove={onRemoveComponent}
                    >
                        {component.fields.map((field: FieldEntry) => (
                            <FieldRow
                                key={field.name}
                                componentType={component.type}
                                field={field}
                                onChange={(compType, fieldName, value) => onFieldChange(compType, fieldName, value)}
                            />
                        ))}
                    </ComponentAccordion>
                ))}
            </div>
            <div style={footerStyle}>
                <button
                    style={buttonStyle('ghost')}
                    data-testid="add-component-btn"
                    onClick={onAddComponentClick}
                >
                    Add Component
                </button>
            </div>
        </div>
    );
}

export default PropertyPanel;
