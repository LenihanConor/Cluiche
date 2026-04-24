import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";

// EditorBridge is a module with top-level side effects (sets window.DiaEditor_onResponse etc.)
// We reset module-level state between tests by re-importing via vi.isolateModules.
// For most tests we import directly and reset window.dia between cases.

beforeEach(() => {
  // Provide a fresh mock for window.dia before each test.
  (window as any).dia = { callCpp: vi.fn() };
});

afterEach(() => {
  delete (window as any).dia;
  vi.restoreAllMocks();
});

describe("EditorBridge – sendEvent helpers", () => {
  it("shellReady calls dia.callCpp with shell_ready type", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.shellReady();
    expect((window as any).dia.callCpp).toHaveBeenCalledWith(
      "DiaEditor_call",
      expect.stringContaining('"type":"shell_ready"')
    );
  });

  it("undo calls dia.callCpp with undo type", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.undo();
    expect((window as any).dia.callCpp).toHaveBeenCalledWith(
      "DiaEditor_call",
      expect.stringContaining('"type":"undo"')
    );
  });

  it("redo calls dia.callCpp with redo type", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.redo();
    expect((window as any).dia.callCpp).toHaveBeenCalledWith(
      "DiaEditor_call",
      expect.stringContaining('"type":"redo"')
    );
  });

  it("togglePanelVisibility sends correct panel name", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.togglePanelVisibility("OutputConsole");
    const payload = JSON.parse(
      ((window as any).dia.callCpp as ReturnType<typeof vi.fn>).mock.calls[0][1]
    );
    expect(payload.type).toBe("toggle_panel_visibility");
    expect(payload.data.name).toBe("OutputConsole");
  });

  it("drops event with warning when dia.callCpp is unavailable", async () => {
    delete (window as any).dia;
    const warnSpy = vi.spyOn(console, "warn").mockImplementation(() => {});
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.shellReady();
    expect(warnSpy).toHaveBeenCalledWith(
      expect.stringContaining("dia.callCpp not available"),
      expect.anything()
    );
  });
});

describe("EditorBridge – sendRequest / response dispatch", () => {
  it("getPanels sends a request with incrementing reqId", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    EditorBridge.getPanels(); // don't await — we won't respond
    const payload = JSON.parse(
      ((window as any).dia.callCpp as ReturnType<typeof vi.fn>).mock.calls[0][1]
    );
    expect(payload.type).toBe("get_panels");
    expect(payload.reqId).toMatch(/^r\d+$/);
  });

  it("resolves the correct promise when DiaEditor_onResponse fires", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const promise = EditorBridge.getPanels();

    const reqId = JSON.parse(
      ((window as any).dia.callCpp as ReturnType<typeof vi.fn>).mock.calls[0][1]
    ).reqId;

    const panels = [{ name: "Console", uiPath: "/console", visible: true }];
    window.DiaEditor_onResponse!({ reqId, result: { panels } });

    const result = await promise;
    expect(result.panels).toEqual(panels);
  });

  it("ignores responses with unknown reqId", async () => {
    await import("./EditorBridge");
    expect(() => {
      window.DiaEditor_onResponse!({ reqId: "r9999", result: {} });
    }).not.toThrow();
  });

  it("rejects immediately when dia.callCpp is unavailable", async () => {
    delete (window as any).dia;
    const { EditorBridge } = await import("./EditorBridge");
    await expect(EditorBridge.getPanels()).rejects.toThrow("dia.callCpp not available");
  });

  it("handles response delivered as a JSON string", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const promise = EditorBridge.getCommands();

    const reqId = JSON.parse(
      ((window as any).dia.callCpp as ReturnType<typeof vi.fn>).mock.calls[0][1]
    ).reqId;

    const commands = [{ id: "cmd.save", label: "Save" }];
    window.DiaEditor_onResponse!(JSON.stringify({ reqId, result: { commands } }));

    const result = await promise;
    expect(result.commands).toEqual(commands);
  });
});

describe("EditorBridge – subscribe / topic listeners", () => {
  it("subscribe adds a listener that receives published data", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const listener = vi.fn();
    EditorBridge.subscribe("my_topic", listener);

    window.DiaEditor_onDataChanged!({ topic: "my_topic", data: { value: 42 } });

    expect(listener).toHaveBeenCalledWith({ value: 42 });
  });

  it("unsubscribe removes the listener", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const listener = vi.fn();
    const unsub = EditorBridge.subscribe("my_topic", listener);
    unsub();

    window.DiaEditor_onDataChanged!({ topic: "my_topic", data: {} });

    expect(listener).not.toHaveBeenCalled();
  });

  it("multiple listeners on the same topic all fire", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const a = vi.fn();
    const b = vi.fn();
    EditorBridge.subscribe("shared", a);
    EditorBridge.subscribe("shared", b);

    window.DiaEditor_onDataChanged!({ topic: "shared", data: "ping" });

    expect(a).toHaveBeenCalledWith("ping");
    expect(b).toHaveBeenCalledWith("ping");
  });

  it("ignores payloads with missing topic", async () => {
    await import("./EditorBridge");
    expect(() => {
      window.DiaEditor_onDataChanged!({ data: "no-topic" });
    }).not.toThrow();
  });

  it("listener error is swallowed and other listeners still fire", async () => {
    const { EditorBridge } = await import("./EditorBridge");
    const bad = vi.fn().mockImplementation(() => { throw new Error("boom"); });
    const good = vi.fn();
    const warnSpy = vi.spyOn(console, "warn").mockImplementation(() => {});
    EditorBridge.subscribe("err_topic", bad);
    EditorBridge.subscribe("err_topic", good);

    window.DiaEditor_onDataChanged!({ topic: "err_topic", data: {} });

    expect(good).toHaveBeenCalledTimes(1);
    expect(warnSpy).toHaveBeenCalledWith(
      expect.stringContaining("topic listener failed"),
      "err_topic",
      expect.any(Error)
    );
  });

  it("broadcasts data-changed events to all iframes", async () => {
    await import("./EditorBridge");

    const iframe = document.createElement("iframe");
    document.body.appendChild(iframe);

    const postSpy = vi.spyOn(iframe.contentWindow!, "postMessage");

    window.DiaEditor_onDataChanged!({ topic: "broadcast_topic", data: { x: 1 } });

    expect(postSpy).toHaveBeenCalledWith(
      { __dia: true, topic: "broadcast_topic", data: { x: 1 } },
      "*"
    );

    document.body.removeChild(iframe);
  });
});

describe("EditorBridge – iframe relay", () => {
  it("routes a frame request through dia.callCpp", async () => {
    await import("./EditorBridge");

    window.dispatchEvent(new MessageEvent("message", {
      data: { __diaFromFrame: true, payload: { type: "get_panels", reqId: "frame-r1" } },
      source: window,
    }));

    expect((window as any).dia.callCpp).toHaveBeenCalledWith(
      "DiaEditor_call",
      expect.stringContaining('"type":"get_panels"')
    );
  });

  it("drops iframe message when dia.callCpp is unavailable", async () => {
    delete (window as any).dia;
    const warnSpy = vi.spyOn(console, "warn").mockImplementation(() => {});
    await import("./EditorBridge");

    window.dispatchEvent(new MessageEvent("message", {
      data: { __diaFromFrame: true, payload: { type: "get_panels" } },
      source: window,
    }));

    expect(warnSpy).toHaveBeenCalledWith(
      expect.stringContaining("dia.callCpp not available"),
      expect.anything()
    );
  });

  it("routes response back to the iframe window that sent the request", async () => {
    await import("./EditorBridge");

    const postSpy = vi.spyOn(window, "postMessage");

    // Simulate an iframe posting a request with a reqId
    window.dispatchEvent(new MessageEvent("message", {
      data: { __diaFromFrame: true, payload: { type: "get_panels", reqId: "frame-r42" } },
      source: window,
    }));

    // Simulate C++ responding to that reqId
    window.DiaEditor_onResponse!({ reqId: "frame-r42", result: { panels: [] } });

    expect(postSpy).toHaveBeenCalledWith(
      expect.objectContaining({ __diaResponse: true, reqId: "frame-r42" }),
      "*"
    );
  });

  it("ignores messages without __diaFromFrame flag", async () => {
    await import("./EditorBridge");
    const callSpy = (window as any).dia.callCpp as ReturnType<typeof vi.fn>;
    callSpy.mockClear();

    window.dispatchEvent(new MessageEvent("message", {
      data: { payload: { type: "get_panels" } },
      source: window,
    }));

    expect(callSpy).not.toHaveBeenCalled();
  });
});
