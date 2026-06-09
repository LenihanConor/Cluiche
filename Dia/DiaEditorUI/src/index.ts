// DiaEditorUI — shared React component library for Cluiche editor plugins
// Exports added as components are implemented.

export { theme, injectThemeVars, inputStyle, buttonStyle } from './theme';
export type { Theme } from './theme';
export { TrafficLightDot } from './TrafficLightDot';
export type { DotState, TrafficLightDotProps } from './TrafficLightDot';

export { EmptyState } from './EmptyState';
export type { EmptyStateProps } from './EmptyState';

export { TabBar } from './TabBar';
export type { Tab, TabBarProps } from './TabBar';

export { ConnectionStatus } from './ConnectionStatus';
export type { ConnectionState, ConnectionStatusProps } from './ConnectionStatus';

export { useBridgeSubscribe, useBridgeRequest } from './useBridge';
export type { BridgeSubscription } from './useBridge';

export { useToast, useToastStore } from './notifications/useToast';
export type { Toast, ToastSeverity } from './notifications/useToast';
export { ToastRenderer } from './notifications/ToastRenderer';

export { useResizableDivider } from './useResizableDivider';
export type { UseResizableDividerResult } from './useResizableDivider';

export { deriveExpectedPath, buildNavigateFailedContext } from './navigateFailedUtils';
export type { NavigateFailedContext } from './navigateFailedUtils';
