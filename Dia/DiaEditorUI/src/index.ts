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

export { ValueLabel } from './ValueLabel';
export type { ValueLabelProps } from './ValueLabel';
export { ValueTooltip } from './ValueTooltip';
export type { ValueTooltipProps } from './ValueTooltip';
export type { ValueInfoLike, ValueInfoBinding, ValueInfoEnumValue, ValueInfoUi, ResolvableKeys } from './valueInfo';
export { componentFieldToValueInfo, serviceEntryToValueInfo } from './valueInfoAdapter';
export type { ComponentSchemaField } from './valueInfoAdapter';

export {
  RampResolver, parseHexColour, rgba8ToHex, sampleRamp, rampColourAt, rampT, buildTicks, buildLegend,
  kGreyRampId, kDeltaDefaultRampId, kCategoricalRampId,
} from './ramps';
export type {
  Rgba8, RampStop, RampJson, RampData, ResolvedRamp, PaletteEntry, PaletteData, SwatchEntry, LegendData, RampWarn,
} from './ramps';

export { hexDistance, cellsWithin, cellsInBox } from './cellGeometry';
export { computeLayout, cellCentre, cellFromPoint, hexCorners } from './cellGeometry';
export type { Topology, ViewState, Layout, LayoutInput, CellRef } from './cellGeometry';
export { colourForValue, colourCells } from './cellColour';
export type { CellColouring } from './cellColour';
export { CellGridCanvas } from './CellGridCanvas';
export type { CellGridCanvasProps } from './CellGridCanvas';

export {
  selectionHash, decodeLayerValues, concatChunks, chunkRanges, kCellsValuesChunk, kCellsSelectionChunk,
  isCellsViewState, isCellsLayerList, isCellsLayerValues, isCellsSelectionPage, isCellsSelectionStats, isCellsError,
} from './cells';
export type {
  CellsViewState, CellsGridInfo, CellsSelectionInfo, CellsHistoryState,
  CellsLayerList, CellsLayerGrid, CellsLayerEntry, CellsOverlayEntry,
  CellsLayerValues, CellsSelectionPage, CellsSelectionStats, CellsError,
} from './cells';
