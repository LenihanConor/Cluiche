import { useEffect, useRef, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";

export interface TopicHealth {
  topic: string;
  sent: number;
  dropped: number;
  dropRate: number; // percentage
}

export interface SubscriptionAck {
  topic: string;
  status: "acked" | "pending" | "timeout";
  latencyMs?: number;
  elapsedMs?: number;
}

export type HealthBadge = "none" | "yellow" | "red";

export interface ConnectionHealth {
  badge: HealthBadge;
  summary: string; // e.g. "3 topics healthy" or "Dropping messages"
  topics: TopicHealth[];
  subscriptions: SubscriptionAck[];
  totalDropsInWindow: number;
}

const POLL_INTERVAL_MS = 10000;
const WINDOW_DURATION_MS = 5000;
const YELLOW_THRESHOLD = 10;
const RED_THRESHOLD = 100;

const STATS_REPLY_TOPIC = "game_connection.command_response.get_server_stats";

interface DropSample {
  timestampMs: number;
  drops: number;
}

export function useConnectionHealth(connected: boolean): ConnectionHealth {
  const [health, setHealth] = useState<ConnectionHealth>({
    badge: "none",
    summary: "",
    topics: [],
    subscriptions: [],
    totalDropsInWindow: 0,
  });

  const dropSamplesRef = useRef<DropSample[]>([]);
  const prevTotalDropsRef = useRef<number>(0);
  const firstPollRef = useRef<boolean>(true);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);
  // Latest stats pushed back from the game via send_command reply.
  const latestStatsRef = useRef<{
    topics?: Array<{ topic: string; sent: number; dropped: number; drop_rate: number }>;
    editor_side?: { messages_dropped: number };
  }>({});

  useEffect(() => {
    if (!connected) {
      dropSamplesRef.current = [];
      prevTotalDropsRef.current = 0;
      firstPollRef.current = true;
      latestStatsRef.current = {};
      setHealth({ badge: "none", summary: "", topics: [], subscriptions: [], totalDropsInWindow: 0 });
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
      return;
    }

    // Subscribe to async game-side stats replies pushed by the C++ controller.
    const unsubStats = EditorBridge.subscribe(STATS_REPLY_TOPIC, (data: unknown) => {
      const d = data as { success?: boolean; result?: typeof latestStatsRef.current } | null;
      if (d?.success && d.result) {
        latestStatsRef.current = d.result;
      }
    });

    const poll = async () => {
      try {
        // Fire-and-forget: ask the game for fresh stats. The reply arrives via
        // the STATS_REPLY_TOPIC subscription above and is stored in latestStatsRef.
        EditorBridge.request("game_connection.send_command", { command: "get_server_stats" }).catch(() => {});

        const ackData = await EditorBridge.request<{
          records?: Array<{ topic: string; latency_ms: number }>;
          pending?: Array<{ topic: string; elapsed_ms: number }>;
        }>("game_connection.get_ack_records", {}).catch(() => ({ records: [], pending: [] }));

        const stats = latestStatsRef.current;
        const nowMs = Date.now();
        const topics: TopicHealth[] = (stats.topics ?? []).map((t) => ({
          topic: t.topic,
          sent: t.sent,
          dropped: t.dropped,
          dropRate: t.drop_rate,
        }));

        // Compute new drops since last poll
        const currentTotalDrops = stats.editor_side?.messages_dropped ?? 0;
        let newDrops = 0;
        if (firstPollRef.current) {
          firstPollRef.current = false;
        } else {
          newDrops = Math.max(0, currentTotalDrops - prevTotalDropsRef.current);
        }
        prevTotalDropsRef.current = currentTotalDrops;

        if (newDrops > 0) {
          dropSamplesRef.current.push({ timestampMs: nowMs, drops: newDrops });
        }

        // Purge samples outside 5s window
        const windowStart = nowMs - WINDOW_DURATION_MS;
        dropSamplesRef.current = dropSamplesRef.current.filter(
          (s) => s.timestampMs >= windowStart
        );

        const totalDropsInWindow = dropSamplesRef.current.reduce(
          (sum, s) => sum + s.drops,
          0
        );

        // Compute badge
        let badge: HealthBadge = "none";
        if (totalDropsInWindow >= RED_THRESHOLD) badge = "red";
        else if (totalDropsInWindow >= YELLOW_THRESHOLD) badge = "yellow";

        // Compute summary
        let summary: string;
        if (badge === "none") {
          const healthyCount = topics.filter((t) => t.dropped === 0).length;
          summary =
            topics.length === 0
              ? "No active subscriptions"
              : `${healthyCount} of ${topics.length} topics healthy`;
        } else {
          summary = "Dropping messages";
        }

        // Build subscription ACK status
        const subscriptions: SubscriptionAck[] = [
          ...(ackData.records ?? []).map((r) => ({
            topic: r.topic,
            status: "acked" as const,
            latencyMs: r.latency_ms,
          })),
          ...(ackData.pending ?? []).map((p) => ({
            topic: p.topic,
            status: (p.elapsed_ms >= 3000 ? "timeout" : "pending") as "timeout" | "pending",
            elapsedMs: p.elapsed_ms,
          })),
        ];

        setHealth({ badge, summary, topics, subscriptions, totalDropsInWindow });
      } catch {
        // Silently ignore poll failures — connection may have just dropped
      }
    };

    poll(); // immediate first poll
    intervalRef.current = setInterval(poll, POLL_INTERVAL_MS);

    return () => {
      unsubStats();
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
    };
  }, [connected]);

  return health;
}
