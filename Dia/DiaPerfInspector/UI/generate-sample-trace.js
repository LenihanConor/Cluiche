#!/usr/bin/env node

/**
 * Generate a sample Chrome-trace JSON file for manual Perfetto testing.
 * Run: node generate-sample-trace.js
 * Output: src/model/__fixtures__/sample.trace.json
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Minimal Chrome trace with metadata and some sample events
const trace = {
    traceEvents: [
        {
            ph: 'M',
            pid: 1,
            tid: 0,
            ts: 0,
            name: 'process_name',
            args: { name: 'Dia Perf Inspector' },
        },
        {
            ph: 'M',
            pid: 1,
            tid: 0,
            ts: 0,
            name: 'thread_name',
            args: { name: 'Main' },
        },
        {
            ph: 'M',
            pid: 1,
            tid: 1,
            ts: 0,
            name: 'thread_name',
            args: { name: 'Sim' },
        },
        {
            ph: 'M',
            pid: 1,
            tid: 2,
            ts: 0,
            name: 'thread_name',
            args: { name: 'Render' },
        },
        // Main thread: tick and modules (time in microseconds)
        {
            ph: 'X',
            pid: 1,
            tid: 0,
            ts: 300000000, // 300 seconds
            dur: 33333, // ~33ms for 30 Hz
            name: 'Main tick',
            args: { is_tick: true },
        },
        {
            ph: 'X',
            pid: 1,
            tid: 0,
            ts: 300000000,
            dur: 5000, // 5ms for Window
            name: 'Window',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 0,
            ts: 300005000,
            dur: 8000, // 8ms for Input
            name: 'Input',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 0,
            ts: 300013000,
            dur: 6000, // 6ms for Console
            name: 'Console',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 0,
            ts: 300019000,
            dur: 4000, // 4ms for Debug
            name: 'Debug',
        },
        // Sim thread: tick and modules
        {
            ph: 'X',
            pid: 1,
            tid: 1,
            ts: 300000000,
            dur: 33333,
            name: 'Sim tick',
            args: { is_tick: true },
        },
        {
            ph: 'X',
            pid: 1,
            tid: 1,
            ts: 300000000,
            dur: 12000,
            name: 'Physics',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 1,
            ts: 300012000,
            dur: 8000,
            name: 'AI',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 1,
            ts: 300020000,
            dur: 5000,
            name: 'World',
        },
        // Render thread: tick and modules
        {
            ph: 'X',
            pid: 1,
            tid: 2,
            ts: 300000000,
            dur: 16667, // ~16.7ms for 60 Hz
            name: 'Render tick',
            args: { is_tick: true },
        },
        {
            ph: 'X',
            pid: 1,
            tid: 2,
            ts: 300000000,
            dur: 6000,
            name: 'Scene',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 2,
            ts: 300006000,
            dur: 7000,
            name: 'Sprites',
        },
        {
            ph: 'X',
            pid: 1,
            tid: 2,
            ts: 300013000,
            dur: 2000,
            name: 'Present',
        },
    ],
    displayTimeUnit: 'ms',
};

const outDir = path.join(__dirname, 'src', 'model', '__fixtures__');
const outFile = path.join(outDir, 'sample.trace.json');

if (!fs.existsSync(outDir)) {
    fs.mkdirSync(outDir, { recursive: true });
}

fs.writeFileSync(outFile, JSON.stringify(trace, null, 2));
console.log(`Generated: ${outFile}`);
console.log(`To test in Perfetto: open https://ui.perfetto.dev and drag ${outFile} into the window`);
