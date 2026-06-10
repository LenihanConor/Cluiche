// ChatPanel.tsx — AI assistant dockable panel for CluicheEditor
// Single-file component; sub-components are inline functions.

import { useState, useRef, useEffect, useCallback, KeyboardEvent } from 'react';
import { useBridgeSubscribe } from '@dia/editor-ui';

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

type ToolCallStatus = 'running' | 'ok' | 'error';

type ToolCallInfo = {
    callId: string;
    fn: string;
    params: object;
    status: ToolCallStatus;
    result?: object;
    error?: string;
    durationMs?: number;
};

type ChatMessage = {
    id: string;
    role: 'user' | 'assistant' | 'system';
    content: string;
    toolCalls: ToolCallInfo[];
    timestamp: number;
};

type ErrorBannerState = {
    type: string;
    message: string;
    retry?: boolean;
};

type PendingConfirm = {
    callId: string;
    fn: string;
    params: object;
    description: string;
};

type BackendStatus = {
    backend: string;
    model: string;
    available: boolean;
};

type ContextWarning = {
    usedTokens: number;
    budgetTokens: number;
    pct: number;
};

type ContextMode = 'full_context' | 'tools_only' | 'custom';

type ChatMetrics = {
    messagesSent: number;
    toolCalls: number;
    tokensStreamed: number;
};

// ---------------------------------------------------------------------------
// Bridge send helper
// ---------------------------------------------------------------------------

function sendEvent(type: string, data?: object): void {
    window.parent.postMessage({ __diaFromFrame: true, payload: { type, data: data ?? {} } }, '*');
}

// ---------------------------------------------------------------------------
// Colour palette (from mockup)
// ---------------------------------------------------------------------------

const C = {
    bodyBg: '#1a1a1a',
    headerBg: '#252526',
    headerBorder: '#333',
    contextBarBg: '#1e1e2e',
    contextBarBorder: '#2a2a3a',
    statusOnline: '#6dbf6d',
    statusOffline: '#666',
    userAvatarBg: '#264f78',
    userAvatarText: '#9cdcfe',
    aiAvatarBg: '#3a2d5c',
    aiAvatarText: '#c792ea',
    toolCardBg: '#1a2030',
    toolCardBorder: '#2a3a55',
    toolCardLeft: '#4a7aaa',
    toolOkBg: '#1a3020',
    toolOkText: '#6dbf6d',
    toolRunningBg: '#2a2a1a',
    toolRunningText: '#d4c060',
    toolErrorBg: '#301a1a',
    toolErrorText: '#f47474',
    inputBg: '#1e1e1e',
    inputBorder: '#3a3a3a',
    inputFocusBorder: '#4a7aaa',
    sendBg: '#264f78',
    sendText: '#9cdcfe',
    backendSelectBg: '#3c3c3c',
    backendSelectBorder: '#555',
    modelBadgeBg: '#2d4a2d',
    modelBadgeText: '#6dbf6d',
    textMuted: '#888',
    text: '#d4d4d4',
    textDim: '#aaa',
    border: '#333',
    chipBg: '#2a2a3a',
    chipText: '#9cdcfe',
    warningBannerBg: '#2a2010',
    warningBannerText: '#d4c060',
    confirmCardBg: '#1e1e2e',
    confirmCardBorder: '#4a7aaa',
    confirmOkBg: '#264f78',
    confirmCancelBg: '#3c3c3c',
    detailBg: '#1c1c2c',
    detailBorder: '#2a2a3a',
};

// ---------------------------------------------------------------------------
// Inline sub-components
// ---------------------------------------------------------------------------

function Avatar({ role }: { role: 'user' | 'assistant' }) {
    const bg = role === 'user' ? C.userAvatarBg : C.aiAvatarBg;
    const color = role === 'user' ? C.userAvatarText : C.aiAvatarText;
    const label = role === 'user' ? 'U' : 'AI';
    return (
        <div style={{
            width: 26,
            height: 26,
            borderRadius: '50%',
            background: bg,
            color,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            fontSize: 11,
            fontWeight: 700,
            flexShrink: 0,
        }}>
            {label}
        </div>
    );
}

function ToolCallCard({ tool, onSelect }: { tool: ToolCallInfo; onSelect: (id: string) => void }) {
    const statusColors: Record<ToolCallStatus, { bg: string; text: string; label: string }> = {
        running: { bg: C.toolRunningBg, text: C.toolRunningText, label: 'running…' },
        ok:      { bg: C.toolOkBg,      text: C.toolOkText,      label: 'ok' },
        error:   { bg: C.toolErrorBg,   text: C.toolErrorText,   label: 'error' },
    };
    const sc = statusColors[tool.status];
    return (
        <div
            onClick={() => onSelect(tool.callId)}
            style={{
                background: C.toolCardBg,
                border: `1px solid ${C.toolCardBorder}`,
                borderLeft: `3px solid ${C.toolCardLeft}`,
                borderRadius: 4,
                padding: '4px 8px',
                marginTop: 4,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: 8,
            }}
        >
            <span style={{ fontFamily: 'monospace', fontSize: 11, color: C.chipText }}>{tool.fn}</span>
            <span style={{
                background: sc.bg,
                color: sc.text,
                borderRadius: 3,
                padding: '1px 6px',
                fontSize: 10,
                fontWeight: 600,
            }}>
                {sc.label}
            </span>
            {tool.durationMs !== undefined && (
                <span style={{ color: C.textMuted, fontSize: 10, marginLeft: 'auto' }}>{tool.durationMs}ms</span>
            )}
        </div>
    );
}

function MessageBubble({
    msg,
    onToolSelect,
}: {
    msg: ChatMessage;
    onToolSelect: (id: string) => void;
}) {
    if (msg.role === 'system') {
        return (
            <div style={{
                display: 'flex',
                justifyContent: 'center',
                marginBottom: 8,
            }}>
                <span style={{
                    color: C.textMuted,
                    fontSize: 11,
                    fontStyle: 'italic',
                    textAlign: 'center',
                }}>
                    {msg.content}
                </span>
            </div>
        );
    }

    const isUser = msg.role === 'user';
    return (
        <div style={{
            display: 'flex',
            gap: 10,
            flexDirection: isUser ? 'row-reverse' : 'row',
            alignItems: 'flex-start',
            marginBottom: 12,
        }}>
            <Avatar role={msg.role} />
            <div style={{ maxWidth: '75%', display: 'flex', flexDirection: 'column', gap: 4 }}>
                <div style={{
                    background: isUser ? C.userAvatarBg : '#222',
                    border: `1px solid ${isUser ? '#3a6090' : C.border}`,
                    borderRadius: 6,
                    padding: '6px 10px',
                    color: C.text,
                    fontSize: 12,
                    whiteSpace: 'pre-wrap',
                    wordBreak: 'break-word',
                    lineHeight: 1.5,
                }}>
                    {msg.content}
                </div>
                {msg.toolCalls.length > 0 && (
                    <div>
                        {msg.toolCalls.map(tc => (
                            <ToolCallCard key={tc.callId} tool={tc} onSelect={onToolSelect} />
                        ))}
                    </div>
                )}
            </div>
        </div>
    );
}

function StreamingBubble({ text }: { text: string }) {
    return (
        <div style={{
            display: 'flex',
            gap: 10,
            alignItems: 'flex-start',
            marginBottom: 12,
        }}>
            <Avatar role="assistant" />
            <div style={{
                background: '#222',
                border: `1px solid ${C.border}`,
                borderRadius: 6,
                padding: '6px 10px',
                color: C.text,
                fontSize: 12,
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word',
                lineHeight: 1.5,
                maxWidth: '75%',
            }}>
                {text}
                <span style={{ color: C.aiAvatarText, marginLeft: 2 }}>▍</span>
            </div>
        </div>
    );
}

function ConfirmCard({
    pending,
    onConfirm,
    onCancel,
}: {
    pending: PendingConfirm;
    onConfirm: () => void;
    onCancel: () => void;
}) {
    return (
        <div style={{
            background: C.confirmCardBg,
            border: `1px solid ${C.confirmCardBorder}`,
            borderRadius: 6,
            padding: '10px 14px',
            marginBottom: 12,
        }}>
            <div style={{ color: C.text, fontSize: 12, fontWeight: 600, marginBottom: 4 }}>
                Confirm: <span style={{ fontFamily: 'monospace', color: C.chipText }}>{pending.fn}</span>
            </div>
            <div style={{ color: C.textDim, fontSize: 11, marginBottom: 10 }}>{pending.description}</div>
            <div style={{ display: 'flex', gap: 8 }}>
                <button
                    onClick={onConfirm}
                    style={{
                        background: C.confirmOkBg,
                        color: C.sendText,
                        border: 'none',
                        borderRadius: 3,
                        padding: '3px 12px',
                        fontSize: 12,
                        cursor: 'pointer',
                    }}
                >
                    Confirm
                </button>
                <button
                    onClick={onCancel}
                    style={{
                        background: C.confirmCancelBg,
                        color: C.text,
                        border: `1px solid ${C.backendSelectBorder}`,
                        borderRadius: 3,
                        padding: '3px 12px',
                        fontSize: 12,
                        cursor: 'pointer',
                    }}
                >
                    Cancel
                </button>
            </div>
        </div>
    );
}

function DetailPanel({
    selectedToolCallId,
    messages,
    backendStatus,
    metrics,
}: {
    selectedToolCallId: string | null;
    messages: ChatMessage[];
    backendStatus: BackendStatus;
    metrics: ChatMetrics;
}) {
    const selectedTool = selectedToolCallId
        ? messages.flatMap(m => m.toolCalls).find(tc => tc.callId === selectedToolCallId) ?? null
        : null;

    return (
        <div style={{
            width: 240,
            flexShrink: 0,
            background: C.detailBg,
            borderLeft: `1px solid ${C.detailBorder}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            fontSize: 11,
            color: C.textDim,
        }}>
            {/* Tool detail */}
            <div style={{ padding: '8px 10px', borderBottom: `1px solid ${C.detailBorder}` }}>
                <div style={{ color: C.text, fontWeight: 600, marginBottom: 6, fontSize: 11 }}>Tool Detail</div>
                {selectedTool ? (
                    <div>
                        <div style={{ fontFamily: 'monospace', color: C.chipText, marginBottom: 4 }}>{selectedTool.fn}</div>
                        <div style={{ marginBottom: 4 }}>
                            <span style={{ color: C.textMuted }}>Status: </span>
                            <span style={{
                                color: selectedTool.status === 'ok' ? C.toolOkText
                                    : selectedTool.status === 'error' ? C.toolErrorText
                                    : C.toolRunningText,
                            }}>
                                {selectedTool.status}
                            </span>
                        </div>
                        <div style={{ marginBottom: 4 }}>
                            <div style={{ color: C.textMuted, marginBottom: 2 }}>Args:</div>
                            <pre style={{
                                background: '#111',
                                border: `1px solid ${C.border}`,
                                borderRadius: 3,
                                padding: '4px 6px',
                                fontSize: 10,
                                color: C.text,
                                overflow: 'auto',
                                maxHeight: 80,
                                margin: 0,
                                whiteSpace: 'pre-wrap',
                                wordBreak: 'break-all',
                            }}>
                                {JSON.stringify(selectedTool.params, null, 2)}
                            </pre>
                        </div>
                        {selectedTool.result !== undefined && (
                            <div>
                                <div style={{ color: C.textMuted, marginBottom: 2 }}>Result:</div>
                                <pre style={{
                                    background: '#111',
                                    border: `1px solid ${C.border}`,
                                    borderRadius: 3,
                                    padding: '4px 6px',
                                    fontSize: 10,
                                    color: C.toolOkText,
                                    overflow: 'auto',
                                    maxHeight: 80,
                                    margin: 0,
                                    whiteSpace: 'pre-wrap',
                                    wordBreak: 'break-all',
                                }}>
                                    {JSON.stringify(selectedTool.result, null, 2)}
                                </pre>
                            </div>
                        )}
                        {selectedTool.error !== undefined && (
                            <div style={{ color: C.toolErrorText, fontSize: 10 }}>Error: {selectedTool.error}</div>
                        )}
                        {selectedTool.durationMs !== undefined && (
                            <div style={{ color: C.textMuted, marginTop: 4 }}>{selectedTool.durationMs}ms</div>
                        )}
                    </div>
                ) : (
                    <div style={{ color: C.textMuted, fontStyle: 'italic' }}>Click a tool call to inspect</div>
                )}
            </div>

            {/* Context files */}
            <div style={{ padding: '8px 10px', borderBottom: `1px solid ${C.detailBorder}` }}>
                <div style={{ color: C.text, fontWeight: 600, marginBottom: 6, fontSize: 11 }}>Context Files</div>
                {[
                    { name: 'engine_overview.md', tokens: 1420 },
                    { name: 'editor_actions.md', tokens: 890 },
                ].map(f => (
                    <div key={f.name} style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        marginBottom: 4,
                    }}>
                        <span style={{
                            background: C.chipBg,
                            color: C.chipText,
                            borderRadius: 3,
                            padding: '1px 6px',
                            fontSize: 10,
                            maxWidth: 140,
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            whiteSpace: 'nowrap',
                        }}>
                            {f.name}
                        </span>
                        <span style={{ color: C.textMuted, fontSize: 10 }}>{f.tokens}t</span>
                    </div>
                ))}
            </div>

            {/* Session info + live metrics */}
            <div style={{ padding: '8px 10px' }}>
                <div style={{ color: C.text, fontWeight: 600, marginBottom: 6, fontSize: 11 }}>Session</div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
                    <div><span style={{ color: C.textMuted }}>Backend: </span>{backendStatus.backend}</div>
                    <div><span style={{ color: C.textMuted }}>Model: </span>{backendStatus.model}</div>
                    <div><span style={{ color: C.textMuted }}>Messages: </span>{messages.length}</div>
                </div>
                <div style={{ marginTop: 10, borderTop: `1px solid ${C.detailBorder}`, paddingTop: 8 }}>
                    <div style={{ color: C.text, fontWeight: 600, marginBottom: 6, fontSize: 11 }}>Metrics</div>
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 3 }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                            <span style={{ color: C.textMuted }}>Messages sent</span>
                            <span style={{ color: C.chipText, fontVariantNumeric: 'tabular-nums' }}>{metrics.messagesSent}</span>
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                            <span style={{ color: C.textMuted }}>Tool calls</span>
                            <span style={{ color: C.chipText, fontVariantNumeric: 'tabular-nums' }}>{metrics.toolCalls}</span>
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                            <span style={{ color: C.textMuted }}>Tokens streamed</span>
                            <span style={{ color: C.chipText, fontVariantNumeric: 'tabular-nums' }}>{metrics.tokensStreamed.toLocaleString()}</span>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
}

// ---------------------------------------------------------------------------
// Error banner
// ---------------------------------------------------------------------------

function ErrorBanner({ banner, onRetry, onDismiss }: {
    banner: ErrorBannerState;
    onRetry: () => void;
    onDismiss: () => void;
}) {
    // destructive_cancel is shown as a system message in the conversation, not a banner
    if (banner.type === 'destructive_cancel') return null;
    return (
        <div style={{
            background: '#4b1515',
            color: '#f48771',
            padding: '8px 12px',
            fontSize: 12,
            display: 'flex',
            alignItems: 'center',
            gap: 8,
            flexShrink: 0,
        }}>
            <span style={{ flex: 1 }}>{banner.message}</span>
            {banner.retry && (
                <button
                    onClick={onRetry}
                    style={{
                        background: '#264f78',
                        color: '#d4d4d4',
                        border: 'none',
                        padding: '2px 8px',
                        cursor: 'pointer',
                        fontSize: 12,
                        borderRadius: 3,
                    }}
                >
                    Retry
                </button>
            )}
            <button
                onClick={onDismiss}
                style={{
                    background: 'none',
                    color: '#888',
                    border: 'none',
                    cursor: 'pointer',
                    fontSize: 14,
                    padding: '0 2px',
                }}
            >
                ×
            </button>
        </div>
    );
}

// ---------------------------------------------------------------------------
// Empty state
// ---------------------------------------------------------------------------

function EmptyState({ onSuggest }: { onSuggest: (text: string) => void }) {
    const suggestions = [
        'What scenes are in this project?',
        'Place a player entity at the origin.',
    ];
    return (
        <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: 16,
            color: C.textMuted,
            padding: '20px 30px',
            textAlign: 'center',
        }}>
            <div style={{ fontSize: 24 }}>💬</div>
            <div style={{ fontSize: 13, color: C.textDim }}>Hello! Ask me anything about Dia.</div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 8, width: '100%', maxWidth: 280 }}>
                <div style={{ fontSize: 11, color: C.textMuted }}>Try:</div>
                {suggestions.map(s => (
                    <button
                        key={s}
                        onClick={() => onSuggest(s)}
                        style={{
                            background: C.chipBg,
                            color: C.chipText,
                            border: `1px solid ${C.contextBarBorder}`,
                            borderRadius: 4,
                            padding: '6px 10px',
                            fontSize: 11,
                            cursor: 'pointer',
                            textAlign: 'left',
                        }}
                    >
                        "{s}"
                    </button>
                ))}
            </div>
        </div>
    );
}

// ---------------------------------------------------------------------------
// ChatPanel — main component
// ---------------------------------------------------------------------------

export function ChatPanel() {
    const [messages, setMessages] = useState<ChatMessage[]>([]);
    const [streamingText, setStreamingText] = useState<string>('');
    const [isStreaming, setIsStreaming] = useState(false);
    const [inputText, setInputText] = useState('');
    const [backendStatus, setBackendStatus] = useState<BackendStatus>({ backend: 'ollama', model: 'llama3.2', available: false });
    const [detailOpen, setDetailOpen] = useState(true);
    const [selectedToolCallId, setSelectedToolCallId] = useState<string | null>(null);
    const [contextWarning, setContextWarning] = useState<ContextWarning | null>(null);
    const [pendingConfirm, setPendingConfirm] = useState<PendingConfirm | null>(null);
    const [contextMode, setContextMode] = useState<ContextMode>('full_context');
    const [inputFocused, setInputFocused] = useState(false);
    const [errorBanner, setErrorBanner] = useState<ErrorBannerState | null>(null);
    const [metrics, setMetrics] = useState<ChatMetrics>({ messagesSent: 0, toolCalls: 0, tokensStreamed: 0 });

    const messagesEndRef = useRef<HTMLDivElement>(null);
    const streamingTextRef = useRef<string>('');
    const lastMessageRef = useRef<string>('');

    // Auto-scroll on new messages / streaming
    useEffect(() => {
        messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
    }, [messages, streamingText]);

    // Keep streaming text ref in sync
    useEffect(() => {
        streamingTextRef.current = streamingText;
    }, [streamingText]);

    // ---------------------------------------------------------------------------
    // Bridge subscriptions
    // ---------------------------------------------------------------------------

    useBridgeSubscribe([
        {
            topic: 'chat.token',
            handler: (rawData: unknown) => {
                const data = rawData as { text: string; done: boolean };
                if (!data.done) {
                    setStreamingText(prev => prev + data.text);
                    setIsStreaming(true);
                } else {
                    // Finalize: move accumulated text into messages
                    const finalText = streamingTextRef.current + (data.text ?? '');
                    const newMsg: ChatMessage = {
                        id: Date.now().toString(),
                        role: 'assistant',
                        content: finalText,
                        toolCalls: [],
                        timestamp: Date.now(),
                    };
                    setMessages(prev => [...prev, newMsg]);
                    setStreamingText('');
                    setIsStreaming(false);
                }
            },
        },
        {
            topic: 'chat.tool_start',
            handler: (rawData: unknown) => {
                const data = rawData as { call_id: string; fn: string; params: object };
                const tool: ToolCallInfo = {
                    callId: data.call_id,
                    fn: data.fn,
                    params: data.params,
                    status: 'running',
                };
                setMessages(prev => {
                    if (prev.length === 0) return prev;
                    const updated = [...prev];
                    const last = { ...updated[updated.length - 1] };
                    last.toolCalls = [...last.toolCalls, tool];
                    updated[updated.length - 1] = last;
                    return updated;
                });
            },
        },
        {
            topic: 'chat.tool_result',
            handler: (rawData: unknown) => {
                const data = rawData as { call_id: string; result: object; duration_ms: number };
                setMessages(prev => prev.map(m => ({
                    ...m,
                    toolCalls: m.toolCalls.map(tc =>
                        tc.callId === data.call_id
                            ? { ...tc, status: 'ok' as ToolCallStatus, result: data.result, durationMs: data.duration_ms }
                            : tc
                    ),
                })));
            },
        },
        {
            topic: 'chat.tool_error',
            handler: (rawData: unknown) => {
                const data = rawData as { call_id: string; error: string };
                setMessages(prev => prev.map(m => ({
                    ...m,
                    toolCalls: m.toolCalls.map(tc =>
                        tc.callId === data.call_id
                            ? { ...tc, status: 'error' as ToolCallStatus, error: data.error }
                            : tc
                    ),
                })));
            },
        },
        {
            topic: 'chat.confirm_required',
            handler: (rawData: unknown) => {
                const data = rawData as { call_id: string; fn: string; params: object; description: string };
                setPendingConfirm({
                    callId: data.call_id,
                    fn: data.fn,
                    params: data.params,
                    description: data.description,
                });
            },
        },
        {
            topic: 'chat.error',
            handler: (rawData: unknown) => {
                const data = rawData as { error_type?: string; message: string; retry?: boolean };
                setIsStreaming(false);
                if (data.error_type === 'destructive_cancel') {
                    // Show inline as a system message in the conversation
                    setMessages(prev => [...prev, {
                        id: Date.now().toString(),
                        role: 'system' as const,
                        content: data.message,
                        toolCalls: [],
                        timestamp: Date.now(),
                    }]);
                } else if (data.error_type) {
                    setErrorBanner({ type: data.error_type, message: data.message, retry: data.retry === true });
                } else {
                    // Legacy path: no error_type — show as assistant error message
                    setMessages(prev => [...prev, {
                        id: Date.now().toString(),
                        role: 'assistant' as const,
                        content: `[Error: ${data.message}]`,
                        toolCalls: [],
                        timestamp: Date.now(),
                    }]);
                }
            },
        },
        {
            topic: 'chat.backend_status',
            handler: (rawData: unknown) => {
                const data = rawData as { backend?: string; model?: string; available?: boolean; status?: string; error_type?: string; message?: string };
                if (data.status === 'error' && data.error_type && data.message) {
                    setErrorBanner({ type: data.error_type, message: data.message });
                } else if (data.backend !== undefined && data.model !== undefined && data.available !== undefined) {
                    setBackendStatus({ backend: data.backend, model: data.model, available: data.available });
                }
            },
        },
        {
            topic: 'chat.context_warning',
            handler: (rawData: unknown) => {
                const data = rawData as { used_tokens: number; budget_tokens: number; pct: number };
                setContextWarning({ usedTokens: data.used_tokens, budgetTokens: data.budget_tokens, pct: data.pct });
            },
        },
        {
            topic: 'chat.metrics',
            handler: (rawData: unknown) => {
                const d = rawData as { messages_sent: number; tool_calls: number; tokens_streamed: number };
                setMetrics({ messagesSent: d.messages_sent, toolCalls: d.tool_calls, tokensStreamed: d.tokens_streamed });
            },
        },
    ]);

    // ---------------------------------------------------------------------------
    // Handlers
    // ---------------------------------------------------------------------------

    const handleSend = useCallback((overrideText?: string) => {
        const text = (overrideText ?? inputText).trim();
        if (!text || isStreaming) return;

        const atFileRegex = /@(\S+\.md)/g;
        const extraFiles: string[] = [];
        let match;
        while ((match = atFileRegex.exec(text)) !== null) {
            extraFiles.push(match[1]);
        }

        lastMessageRef.current = text;

        const userMsg: ChatMessage = {
            id: Date.now().toString(),
            role: 'user',
            content: text,
            toolCalls: [],
            timestamp: Date.now(),
        };
        setMessages(prev => [...prev, userMsg]);
        if (!overrideText) setInputText('');
        setIsStreaming(true);

        sendEvent('chat.send_message', { text, context_mode: contextMode, extra_files: extraFiles });
    }, [inputText, isStreaming, contextMode]);

    const handleKeyDown = useCallback((e: KeyboardEvent<HTMLTextAreaElement>) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            handleSend();
        }
    }, [handleSend]);

    const handleConfirm = useCallback((confirmed: boolean) => {
        if (!pendingConfirm) return;
        sendEvent('chat.confirm_response', { call_id: pendingConfirm.callId, confirmed });
        setPendingConfirm(null);
    }, [pendingConfirm]);

    const handleToolSelect = useCallback((id: string) => {
        setSelectedToolCallId(id);
        setDetailOpen(true);
    }, []);

    const handleSuggest = useCallback((text: string) => {
        setInputText(text);
    }, []);

    // ---------------------------------------------------------------------------
    // Render
    // ---------------------------------------------------------------------------

    const showEmpty = messages.length === 0 && !isStreaming;

    return (
        <div style={{
            height: '100%',
            display: 'flex',
            flexDirection: 'column',
            background: C.bodyBg,
            color: C.text,
            fontFamily: "'Segoe UI', system-ui, sans-serif",
            fontSize: 12,
            overflow: 'hidden',
        }}>
            {/* ── Panel header ─────────────────────────────────────────── */}
            <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: 8,
                padding: '5px 10px',
                background: C.headerBg,
                borderBottom: `1px solid ${C.headerBorder}`,
                flexShrink: 0,
            }}>
                {/* Status dot */}
                <div style={{
                    width: 8,
                    height: 8,
                    borderRadius: '50%',
                    background: backendStatus.available ? C.statusOnline : C.statusOffline,
                    flexShrink: 0,
                }} />
                <span style={{ fontWeight: 600, color: C.text, fontSize: 12 }}>AI Assistant</span>

                <div style={{ flex: 1 }} />

                {/* Backend selector */}
                <select
                    value={backendStatus.backend}
                    onChange={e => {
                        const b = e.target.value;
                        setBackendStatus(prev => ({ ...prev, backend: b }));
                        sendEvent('chat.set_backend', { backend: b });
                    }}
                    style={{
                        background: C.backendSelectBg,
                        color: C.text,
                        border: `1px solid ${C.backendSelectBorder}`,
                        borderRadius: 3,
                        padding: '2px 4px',
                        fontSize: 11,
                        cursor: 'pointer',
                    }}
                >
                    <option value="ollama">Ollama</option>
                    <option value="anthropic">Anthropic</option>
                    <option value="openai">OpenAI</option>
                </select>

                {/* Model badge */}
                <span style={{
                    background: C.modelBadgeBg,
                    color: C.modelBadgeText,
                    borderRadius: 3,
                    padding: '1px 6px',
                    fontSize: 10,
                    fontWeight: 600,
                }}>
                    {backendStatus.model}
                </span>

                {/* Details toggle */}
                <button
                    onClick={() => setDetailOpen(o => !o)}
                    style={{
                        background: detailOpen ? '#2a3a55' : C.backendSelectBg,
                        color: detailOpen ? C.chipText : C.textMuted,
                        border: `1px solid ${detailOpen ? C.toolCardLeft : C.backendSelectBorder}`,
                        borderRadius: 3,
                        padding: '2px 8px',
                        fontSize: 11,
                        cursor: 'pointer',
                    }}
                >
                    Details
                </button>

                {/* Clear history button */}
                <button
                    aria-label="New chat"
                    onClick={() => {
                        setMessages([]);
                        setStreamingText('');
                        setIsStreaming(false);
                        setPendingConfirm(null);
                        setErrorBanner(null);
                        sendEvent('chat.clear_history', {});
                    }}
                    title="New chat"
                    style={{
                        background: C.backendSelectBg,
                        color: C.textMuted,
                        border: `1px solid ${C.backendSelectBorder}`,
                        borderRadius: 3,
                        padding: '2px 8px',
                        fontSize: 11,
                        cursor: 'pointer',
                    }}
                >
                    New chat
                </button>
            </div>

            {/* ── Context bar ──────────────────────────────────────────── */}
            <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: 6,
                padding: '4px 10px',
                background: C.contextBarBg,
                borderBottom: `1px solid ${C.contextBarBorder}`,
                flexShrink: 0,
                flexWrap: 'wrap',
            }}>
                <span style={{ color: C.textMuted, fontSize: 11 }}>Context:</span>
                {['engine_overview', 'editor_actions'].map(f => (
                    <span
                        key={f}
                        style={{
                            background: C.chipBg,
                            color: C.chipText,
                            borderRadius: 3,
                            padding: '1px 6px',
                            fontSize: 10,
                        }}
                    >
                        {f}
                    </span>
                ))}
                <button
                    onClick={() => sendEvent('chat.add_context_file', {})}
                    style={{
                        background: 'transparent',
                        color: C.textMuted,
                        border: `1px dashed ${C.backendSelectBorder}`,
                        borderRadius: 3,
                        padding: '1px 6px',
                        fontSize: 10,
                        cursor: 'pointer',
                    }}
                >
                    +file
                </button>

            </div>

            {/* ── Context mode toggle bar ───────────────────────────────── */}
            <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: 4,
                padding: '3px 10px',
                background: C.contextBarBg,
                borderBottom: `1px solid ${C.contextBarBorder}`,
                flexShrink: 0,
            }}>
                {([
                    { mode: 'full_context' as ContextMode, label: 'Full context' },
                    { mode: 'tools_only' as ContextMode, label: 'Tools only' },
                    { mode: 'custom' as ContextMode, label: 'Custom' },
                ] as { mode: ContextMode; label: string }[]).map(({ mode, label }) => (
                    <button
                        key={mode}
                        onClick={() => {
                            setContextMode(mode);
                            sendEvent('chat.set_context_mode', { mode });
                        }}
                        style={{
                            background: contextMode === mode ? '#264f78' : C.backendSelectBg,
                            color: contextMode === mode ? C.sendText : C.textMuted,
                            border: `1px solid ${contextMode === mode ? C.toolCardLeft : C.backendSelectBorder}`,
                            borderRadius: 3,
                            padding: '2px 8px',
                            fontSize: 11,
                            cursor: 'pointer',
                        }}
                    >
                        {label}
                    </button>
                ))}
            </div>

            {/* ── Error banner ─────────────────────────────────────────── */}
            {errorBanner && (
                <ErrorBanner
                    banner={errorBanner}
                    onRetry={() => {
                        setErrorBanner(null);
                        handleSend(lastMessageRef.current || undefined);
                    }}
                    onDismiss={() => setErrorBanner(null)}
                />
            )}

            {/* ── Body (messages + detail panel) ───────────────────────── */}
            <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
                {/* Messages column */}
                <div style={{ flex: 1, overflowY: 'auto', padding: '10px 12px', display: 'flex', flexDirection: 'column' }}>
                    {/* Context window warning */}
                    {contextWarning && contextWarning.pct >= 75 && (
                        <div style={{
                            background: C.warningBannerBg,
                            color: C.warningBannerText,
                            border: `1px solid #4a3a10`,
                            borderRadius: 4,
                            padding: '5px 10px',
                            marginBottom: 10,
                            fontSize: 11,
                        }}>
                            Context window near limit — early messages may be trimmed. ({contextWarning.pct}% used)
                        </div>
                    )}

                    {/* Pending confirmation */}
                    {pendingConfirm && (
                        <ConfirmCard
                            pending={pendingConfirm}
                            onConfirm={() => handleConfirm(true)}
                            onCancel={() => handleConfirm(false)}
                        />
                    )}

                    {/* Empty state */}
                    {showEmpty ? (
                        <EmptyState onSuggest={handleSuggest} />
                    ) : (
                        <>
                            {messages.map(msg => (
                                <MessageBubble key={msg.id} msg={msg} onToolSelect={handleToolSelect} />
                            ))}
                            {isStreaming && streamingText && (
                                <StreamingBubble text={streamingText} />
                            )}
                        </>
                    )}
                    <div ref={messagesEndRef} />
                </div>

                {/* Detail panel */}
                {detailOpen && (
                    <DetailPanel
                        selectedToolCallId={selectedToolCallId}
                        messages={messages}
                        backendStatus={backendStatus}
                        metrics={metrics}
                    />
                )}
            </div>

            {/* ── Input area ───────────────────────────────────────────── */}
            <div style={{
                flexShrink: 0,
                background: C.headerBg,
                borderTop: `1px solid ${C.headerBorder}`,
                padding: '8px 10px',
                display: 'flex',
                flexDirection: 'column',
                gap: 6,
            }}>
                {/* Textarea + send button row */}
                <div style={{ display: 'flex', gap: 6, alignItems: 'flex-end' }}>
                    <textarea
                        value={inputText}
                        onChange={e => setInputText(e.target.value)}
                        onKeyDown={handleKeyDown}
                        onFocus={() => setInputFocused(true)}
                        onBlur={() => setInputFocused(false)}
                        disabled={isStreaming}
                        placeholder="Ask anything… (Enter to send, Shift+Enter for newline)"
                        rows={2}
                        style={{
                            flex: 1,
                            background: C.inputBg,
                            color: C.text,
                            border: `1px solid ${inputFocused ? C.inputFocusBorder : C.inputBorder}`,
                            borderRadius: 4,
                            padding: '6px 8px',
                            fontSize: 12,
                            fontFamily: "'Segoe UI', system-ui, sans-serif",
                            resize: 'none',
                            outline: 'none',
                            lineHeight: 1.4,
                            opacity: isStreaming ? 0.6 : 1,
                        }}
                    />
                    <button
                        onClick={handleSend}
                        disabled={!inputText.trim() || isStreaming}
                        title="Send (Enter)"
                        style={{
                            background: (!inputText.trim() || isStreaming) ? '#1a2a3a' : C.sendBg,
                            color: (!inputText.trim() || isStreaming) ? '#4a6080' : C.sendText,
                            border: 'none',
                            borderRadius: 4,
                            padding: '0 14px',
                            height: 48,
                            fontSize: 16,
                            cursor: (!inputText.trim() || isStreaming) ? 'not-allowed' : 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            justifyContent: 'center',
                            flexShrink: 0,
                        }}
                    >
                        ↑
                    </button>
                </div>

                {/* Quick action buttons row */}
                <div style={{ display: 'flex', gap: 6, flexWrap: 'wrap' }}>
                    {[
                        { label: 'Open a file…', event: 'chat.open_file' },
                        { label: 'Connect to game', event: 'chat.connect_game' },
                        { label: 'Run pipeline', event: 'chat.run_pipeline' },
                    ].map(btn => (
                        <button
                            key={btn.event}
                            onClick={() => sendEvent(btn.event, {})}
                            style={{
                                background: C.backendSelectBg,
                                color: C.textMuted,
                                border: `1px solid ${C.backendSelectBorder}`,
                                borderRadius: 3,
                                padding: '2px 8px',
                                fontSize: 11,
                                cursor: 'pointer',
                            }}
                        >
                            {btn.label}
                        </button>
                    ))}
                </div>
            </div>
        </div>
    );
}

export default ChatPanel;
