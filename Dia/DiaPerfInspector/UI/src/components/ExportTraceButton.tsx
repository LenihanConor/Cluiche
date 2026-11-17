import { selectView, useWindowStore } from '../useWindowStore';
import { buildChromeTrace } from '../model/chromeTrace';
import { exportTraceFile } from '../exportTrace';

export function ExportTraceButton() {
    const lanes = useWindowStore((s) => s.lanes);
    const view = useWindowStore((s) => s.view);

    const handleExport = () => {
        const visible = selectView({ lanes, view });
        const trace = buildChromeTrace(visible.lanes);
        // ISO timestamp without colons: YYYY-MM-DDTHH-MM-SS-ms
        const now = new Date();
        const iso = now.toISOString().replace(/[:.]/g, '-').slice(0, -1); // Remove 'Z' and format
        const filename = `perf-window-${iso}.json`;
        exportTraceFile(trace, filename);
    };

    return (
        <button
            type="button"
            onClick={handleExport}
            style={{
                background: '#3a3a3a',
                border: '1px solid #4a4a4a',
                color: '#d4d4d4',
                font: 'inherit',
                padding: '2px 9px',
                cursor: 'pointer',
            }}
        >
            Export Perfetto
        </button>
    );
}
