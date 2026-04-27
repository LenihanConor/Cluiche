export interface LogLine {
    level: 'info' | 'warn' | 'error' | 'debug';
    message: string;
    timestamp: number;
}

export interface StageState {
    name: string;
    status: 'not-started' | 'running' | 'passed' | 'failed' | 'interrupted';
    durationMs: number;
    startTimestamp: number;
    logLines: LogLine[];
    expanded: boolean;
}

export interface PipelineState {
    runInProgress: boolean;
    target: string;
    config: string;
    passCount: number;
    failCount: number;
    totalDurationMs: number;
    interrupted: boolean;
    stages: StageState[];
    historyRuns: HistoryRun[];
    viewingHistoryIndex: number | null;
}

export interface PipelineEventData {
    event: string;
    system: string;
    stage: string;
    step: string;
    ts: number;
    durationMs: number;
    error?: string;
    detail?: string;
    level?: string;
}

export interface PipelineSummaryData {
    target: string;
    config: string;
    passCount: number;
    failCount: number;
    totalDurationMs: number;
    startTimestamp: number;
    interrupted: boolean;
    runInProgress: boolean;
}

export interface PipelinePayload {
    events: PipelineEventData[];
    summary: PipelineSummaryData;
}

export interface HistoryRun {
    target: string;
    config: string;
    passCount: number;
    failCount: number;
    totalDurationMs: number;
    startTimestamp: number;
    interrupted: boolean;
}

export const initialPipelineState: PipelineState = {
    runInProgress: false,
    target: '',
    config: '',
    passCount: 0,
    failCount: 0,
    totalDurationMs: 0,
    interrupted: false,
    stages: [],
    historyRuns: [],
    viewingHistoryIndex: null,
};
