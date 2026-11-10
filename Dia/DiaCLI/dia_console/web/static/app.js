/**
 * DiaConsole shell front end (console-native-shell.md, Task 9).
 *
 * SD-CONSOLE-005: this file only ever talks to /api/* on the local FastAPI
 * app. It never constructs subprocess calls, never talks to Click, and
 * never bypasses ExecutionService — the server is the only thing that does.
 *
 * The nav-group friendly-label table lives server-side (dia_console/web/
 * nav_grouping.py) and arrives as each command's `groupLabel` field, so this
 * file only ever buckets by an already-resolved label instead of keeping a
 * second copy of that table in JS.
 */
(function () {
  "use strict";

  // Fixed display order for nav groups (matches the mockup / Data Contracts
  // table). Any groupLabel not in this list (there should only ever be
  // "Advanced") is appended at the end.
  var GROUP_ORDER = [
    "Run",
    "Debug",
    "Test & Quality",
    "Create & Docs",
    "Environment",
    "Advanced",
  ];

  var TERMINAL_EXECUTION_TYPES = [
    "execution.completed",
    "execution.failed",
    "execution.cancelled",
  ];

  var state = {
    commands: [],
    commandsById: {},
    selectedCommand: null,
    expandedGroup: null,
    expandedFamily: null,
    // console-run-tabs.md: one in-flight (or finished-but-still-open)
    // execution per RunTab, newest appended to the right. `activeRunId` is
    // a client-side RunTab.id (NOT the server execution id -- a tab exists,
    // and can be made active, before /api/execute has even resolved).
    runs: [],
    activeRunId: null,
    // Presets (console-presets.md). Populated from GET /api/presets.
    presets: [],
    presetsById: {},
    // Target/config context (console-project-context.md). Populated from
    // GET /api/context on boot; currentTarget/currentConfig track the top
    // bar's <select>s. No project-level (cross-repo/sibling) field here --
    // this repo has exactly one real project.
    context: {
      targets: [],
      configs: [],
      platform: "",
      currentTarget: null,
      currentConfig: null,
    },
  };

  // Monotonic source for RunTab.id -- independent of the server's execution
  // id, which doesn't exist yet at the moment a tab is created.
  var runTabCounter = 0;

  // ------------------------------------------------------------------ utils

  function qs(id) {
    return document.getElementById(id);
  }

  function clear(el) {
    while (el.firstChild) el.removeChild(el.firstChild);
  }

  function el(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  // ------------------------------------------------------------- nav render

  function groupCommands(commands) {
    var byLabel = {};
    commands.forEach(function (cmd) {
      var label = cmd.groupLabel || "Advanced";
      if (!byLabel[label]) byLabel[label] = [];
      byLabel[label].push(cmd);
    });
    var ordered = [];
    GROUP_ORDER.forEach(function (label) {
      if (byLabel[label]) {
        ordered.push({ label: label, commands: byLabel[label] });
        delete byLabel[label];
      }
    });
    Object.keys(byLabel).forEach(function (label) {
      ordered.push({ label: label, commands: byLabel[label] });
    });
    return ordered;
  }

  function commandDisplayName(cmd) {
    return cmd.path.join(" ");
  }

  /**
   * Buckets a nav group's commands by `category` (the shared parent path,
   * e.g. "pipeline.deploy" for pipeline deploy cluichetest/googletest/...).
   * A category with 2+ commands becomes one collapsible "family" row
   * instead of N flat sibling lines -- purely a nav-rendering grouping, not
   * a bespoke per-command UI: any command tree with several same-parent
   * leaves gets this treatment generically (real example: dia_cli's
   * deploy_runner.py dynamically generates one Click command per configured
   * pipeline target, all sharing the "pipeline.deploy" category).
   */
  function groupByFamily(commands) {
    var byCategory = {};
    var order = [];
    commands.forEach(function (cmd) {
      var category = cmd.category || "";
      if (!byCategory[category]) {
        byCategory[category] = [];
        order.push(category);
      }
      byCategory[category].push(cmd);
    });
    var rows = [];
    order.forEach(function (category) {
      var members = byCategory[category];
      if (category && members.length > 1) {
        rows.push({ type: "family", category: category, commands: members });
      } else {
        members.forEach(function (cmd) {
          rows.push({ type: "item", cmd: cmd });
        });
      }
    });
    return rows;
  }

  function familyLabel(category) {
    return category.split(".").join(" ");
  }

  function familyItemLabel(cmd) {
    return cmd.path[cmd.path.length - 1];
  }

  function renderNavItem(cmd, group, label) {
    var itemEl = el("div", "navitem" + (cmd === state.selectedCommand ? " active" : ""));
    itemEl.dataset.commandId = cmd.id;
    // Always the full path, regardless of the shortened label a family row
    // shows -- so searching "pipeline" still finds "cluichetest" nested
    // under the "pipeline deploy" family.
    itemEl.dataset.searchText = commandDisplayName(cmd).toLowerCase();
    itemEl.appendChild(el("span", "dotstat"));
    itemEl.appendChild(document.createTextNode(" " + label));
    itemEl.addEventListener("click", function () {
      state.expandedGroup = group.label;
      selectCommand(cmd.id);
    });
    return itemEl;
  }

  function renderNav() {
    var container = qs("navgroups");
    clear(container);
    var groups = groupCommands(state.commands);

    groups.forEach(function (group) {
      var groupHasSelection = state.selectedCommand
        ? group.commands.indexOf(state.selectedCommand) !== -1
        : false;
      var isExpanded =
        state.expandedGroup === group.label ||
        (state.expandedGroup === null && groupHasSelection);

      var groupEl = el("div", "navgroup" + (isExpanded ? " expanded" : ""));
      groupEl.dataset.group = group.label;

      var titleEl = el("div", "navgroup-title");
      titleEl.appendChild(el("span", "chev", isExpanded ? "▾" : "▸"));
      titleEl.appendChild(document.createTextNode(" " + group.label + " "));
      titleEl.appendChild(el("span", "count", String(group.commands.length)));
      titleEl.addEventListener("click", function () {
        state.expandedGroup = state.expandedGroup === group.label ? null : group.label;
        renderNav();
      });
      groupEl.appendChild(titleEl);

      var itemsEl = el("div", "navitems");
      groupByFamily(group.commands).forEach(function (row) {
        if (row.type === "item") {
          itemsEl.appendChild(renderNavItem(row.cmd, group, commandDisplayName(row.cmd)));
          return;
        }
        var familyHasSelection = state.selectedCommand
          ? row.commands.indexOf(state.selectedCommand) !== -1
          : false;
        var familyExpanded =
          state.expandedFamily === row.category ||
          (state.expandedFamily === null && familyHasSelection);

        var familyEl = el("div", "navfamily" + (familyExpanded ? " expanded" : ""));
        familyEl.dataset.family = row.category;

        var familyTitleEl = el("div", "navfamily-title");
        familyTitleEl.appendChild(el("span", "chev", familyExpanded ? "▾" : "▸"));
        familyTitleEl.appendChild(document.createTextNode(" " + familyLabel(row.category) + " "));
        familyTitleEl.appendChild(el("span", "count", String(row.commands.length)));
        familyTitleEl.addEventListener("click", function () {
          state.expandedFamily = state.expandedFamily === row.category ? null : row.category;
          renderNav();
        });
        familyEl.appendChild(familyTitleEl);

        var familyItemsEl = el("div", "navfamily-items");
        row.commands.forEach(function (cmd) {
          familyItemsEl.appendChild(renderNavItem(cmd, group, familyItemLabel(cmd)));
        });
        familyEl.appendChild(familyItemsEl);
        itemsEl.appendChild(familyEl);
      });
      groupEl.appendChild(itemsEl);
      container.appendChild(groupEl);
    });

    applyNavFilter();
  }

  function applyNavFilter() {
    var query = (qs("nav-search").value || "").trim().toLowerCase();
    var groups = document.querySelectorAll("#navgroups .navgroup");
    groups.forEach(function (groupEl) {
      var groupAnyMatch = query === "";
      groupEl.querySelectorAll(".navfamily").forEach(function (familyEl) {
        var familyAnyMatch = false;
        familyEl.querySelectorAll(".navitem").forEach(function (itemEl) {
          var matches = query === "" || itemEl.dataset.searchText.indexOf(query) !== -1;
          itemEl.classList.toggle("filtered-out", !matches);
          if (matches) familyAnyMatch = true;
        });
        if (query !== "" && familyAnyMatch) familyEl.classList.add("expanded");
        if (familyAnyMatch) groupAnyMatch = true;
      });
      groupEl.querySelectorAll(".navitems > .navitem").forEach(function (itemEl) {
        var matches = query === "" || itemEl.dataset.searchText.indexOf(query) !== -1;
        itemEl.classList.toggle("filtered-out", !matches);
        if (matches) groupAnyMatch = true;
      });
      groupEl.classList.toggle("no-match", !groupAnyMatch);
      if (query !== "" && groupAnyMatch) groupEl.classList.add("expanded");
    });
  }

  // ---------------------------------------------------------------- presets

  /**
   * Presets (console-presets.md, Goals 7-10). This section only ever talks
   * to /api/presets* (SD-CONSOLE-005) -- it never reads the YAML files or
   * builds argv itself.
   */
  function renderPresets() {
    var container = qs("presetitems");
    clear(container);
    if (!state.presets.length) {
      container.appendChild(el("div", "presets-empty", "No presets saved yet."));
      return;
    }
    state.presets.forEach(function (preset) {
      var item = el("div", "presetitem");
      item.dataset.presetId = preset.id;

      item.appendChild(el("span", "name", preset.name));

      var actions = el("span", "presetactions");
      var editBtn = el("span", null, "✎");
      editBtn.title = "Edit";
      editBtn.addEventListener("click", function (event) {
        event.stopPropagation();
        editPreset(preset.id);
      });
      var deleteBtn = el("span", null, "✕");
      deleteBtn.title = "Delete";
      deleteBtn.addEventListener("click", function (event) {
        event.stopPropagation();
        deletePreset(preset.id);
      });
      actions.appendChild(editBtn);
      actions.appendChild(deleteBtn);
      item.appendChild(actions);

      item.addEventListener("click", function () { applyPreset(preset); });
      container.appendChild(item);
    });
  }

  function loadPresets() {
    return fetch("/api/presets")
      .then(function (response) { return response.json(); })
      .then(function (presets) {
        state.presets = presets;
        state.presetsById = {};
        presets.forEach(function (preset) { state.presetsById[preset.id] = preset; });
        renderPresets();
      });
  }

  /**
   * Clicking a preset (Goal 10): selects its command, then fills the
   * rendered form from `preset.values`, marking each filled field's
   * existing `touched` flag true -- an explicit user choice, not a
   * context default. Never auto-submits; the user still clicks Run.
   */
  function applyPreset(preset) {
    selectCommand(preset.commandId);
    var values = preset.values || {};
    Object.keys(values).forEach(function (name) {
      var input = document.querySelector(
        '#form-fields [data-name="' + name + '"], #form-flags [data-name="' + name + '"]');
      if (!input) return;
      var value = values[name];
      if (input.type === "checkbox") {
        input.checked = !!value;
      } else if (Array.isArray(value)) {
        input.value = value.join(", ");
      } else {
        input.value = value;
      }
      input.dataset.touched = "1";
    });
    updateCliPreview();
  }

  /**
   * "Save current" (Goal 8): the form's full current value snapshot --
   * every rendered field's value regardless of its `touched` flag, not
   * just the touched ones.
   */
  function saveCurrentPreset() {
    if (!state.selectedCommand) return;
    var name = window.prompt("Preset name:");
    if (!name) return;
    var id = name.trim().toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/(^-|-$)/g, "");
    if (!id) id = "preset-" + Date.now();
    var values = collectFormValues();
    var merged = {};
    Object.keys(values.arguments).forEach(function (key) { merged[key] = values.arguments[key]; });
    Object.keys(values.options).forEach(function (key) { merged[key] = values.options[key]; });

    fetch("/api/presets", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: id, name: name, commandId: state.selectedCommand.id, values: merged }),
    })
      .then(function (response) {
        if (!response.ok) throw new Error("save preset failed: " + response.status);
        return response.json();
      })
      .then(function () { loadPresets(); })
      .catch(function (err) { window.alert("Failed to save preset: " + err.message); });
  }

  function editPreset(presetId) {
    var preset = state.presetsById[presetId];
    if (!preset) return;
    var newName = window.prompt("Preset name:", preset.name);
    if (!newName) return;
    fetch("/api/presets/" + presetId, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name: newName }),
    })
      .then(function (response) {
        if (!response.ok) throw new Error("edit preset failed: " + response.status);
        return response.json();
      })
      .then(function () { loadPresets(); })
      .catch(function (err) { window.alert("Failed to edit preset: " + err.message); });
  }

  function deletePreset(presetId) {
    if (!window.confirm("Delete this preset?")) return;
    fetch("/api/presets/" + presetId, { method: "DELETE" })
      .then(function (response) {
        if (!response.ok && response.status !== 204) {
          throw new Error("delete preset failed: " + response.status);
        }
        return loadPresets();
      })
      .catch(function (err) { window.alert("Failed to delete preset: " + err.message); });
  }

  // -------------------------------------------------------- context (top bar)

  /**
   * console-project-context.md: target/config/platform context, backed by
   * GET /api/context (which itself is backed by pipeline.toml). This file
   * never reads pipeline.toml or duplicates the target list itself.
   */
  function populateContextSelect(selectEl, values, selectedValue) {
    clear(selectEl);
    values.forEach(function (value) {
      var opt = el("option", null, value);
      opt.value = value;
      selectEl.appendChild(opt);
    });
    selectEl.value = selectedValue;
  }

  function renderContextBar() {
    var targetNames = state.context.targets.map(function (t) { return t.name; });
    populateContextSelect(qs("target-select"), targetNames, state.context.currentTarget);
    populateContextSelect(qs("config-select"), state.context.configs, state.context.currentConfig);
    qs("platform-label").textContent = state.context.platform;
  }

  /**
   * Re-fills any field in the CURRENTLY rendered form whose descriptor
   * `name` is exactly "target" or "config" -- but only if that field's
   * `touched` flag was never set (Goal 6/7). No-op if no command is
   * selected, or if the selected command has neither field.
   */
  function applyContextFieldValue(name, value) {
    if (value === null || value === undefined) return;
    var input = document.querySelector('#form-fields [data-name="' + name + '"]');
    if (!input) return;
    if (input.dataset.touched === "1") return;
    input.value = value;
    updateCliPreview();
  }

  function applyContextToCurrentForm() {
    if (!state.selectedCommand) return;
    applyContextFieldValue("target", state.context.currentTarget);
    applyContextFieldValue("config", state.context.currentConfig);
  }

  function onTargetSelectChanged(event) {
    state.context.currentTarget = event.target.value;
    applyContextToCurrentForm();
  }

  function onConfigSelectChanged(event) {
    state.context.currentConfig = event.target.value;
    applyContextToCurrentForm();
  }

  function loadContext() {
    return fetch("/api/context")
      .then(function (response) { return response.json(); })
      .then(function (context) {
        state.context.targets = context.targets || [];
        state.context.configs = context.configs || [];
        state.context.platform = context.platform || "";
        state.context.currentTarget = context.defaults && context.defaults.target;
        state.context.currentConfig = context.defaults && context.defaults.config;
        renderContextBar();
      });
  }

  // ------------------------------------------------------------ form render

  function fieldTypeInfo(descriptor) {
    // descriptor is an ArgumentDescriptor or OptionDescriptor (model.py).
    var type = descriptor.type;
    if (descriptor.is_flag) return "checkbox";
    if (type === "int" || type === "float") return "number";
    if (type === "choice") return "select";
    return "text"; // string, path
  }

  function makeFieldInputId(kind, name) {
    return "field-" + kind + "-" + name;
  }

  function markFieldTouched(event) {
    event.target.dataset.touched = "1";
  }

  function renderNonFlagField(kind, descriptor) {
    var wrap = el("div", "field" + (descriptor.required ? " required" : ""));
    var label = el("label", null, descriptor.name);
    wrap.appendChild(label);

    var inputType = fieldTypeInfo(descriptor);
    var inputId = makeFieldInputId(kind, descriptor.name);
    var input;
    if (inputType === "select") {
      input = el("select");
      if (!descriptor.required) input.appendChild(el("option", null, ""));
      (descriptor.choices || []).forEach(function (choice) {
        var opt = el("option", null, choice);
        opt.value = choice;
        if (descriptor.default !== null && descriptor.default !== undefined && String(descriptor.default) === choice) {
          opt.selected = true;
        }
        input.appendChild(opt);
      });
    } else {
      input = el("input");
      input.type = inputType === "number" ? "number" : "text";
      // Chromium's own form-history autofill remembers/reoffers values by
      // input name across page loads sharing the same WebView2 profile --
      // this form is regenerated fresh per command selection, so a stale
      // autofilled value could both show the wrong content AND (by firing
      // its own input/change event) mark the field "touched", permanently
      // blocking the context-driven fill below from ever correcting it.
      input.autocomplete = "off";
      if (descriptor.multiple) {
        input.placeholder = "comma-separated";
      } else if (descriptor.default !== null && descriptor.default !== undefined) {
        input.value = String(descriptor.default);
      }
    }
    input.id = inputId;
    input.dataset.kind = kind;
    input.dataset.name = descriptor.name;
    input.dataset.type = descriptor.type;
    input.dataset.multiple = descriptor.multiple ? "1" : "0";
    input.dataset.required = descriptor.required ? "1" : "0";
    // touched (console-project-context.md, Goal 7): set only by a REAL user
    // input/change event, never by a programmatic default-fill (this
    // render, or a later context-driven refill). A context switch only
    // overwrites fields where this flag was never set.
    input.dataset.touched = "0";
    input.addEventListener("input", markFieldTouched);
    input.addEventListener("change", markFieldTouched);
    input.addEventListener("input", updateCliPreview);
    input.addEventListener("change", updateCliPreview);
    wrap.appendChild(input);

    if (descriptor.help) wrap.appendChild(el("div", "help", descriptor.help));
    return wrap;
  }

  function renderFlagField(descriptor) {
    var label = el("label", "checkitem");
    var input = el("input");
    input.type = "checkbox";
    input.id = makeFieldInputId("opt", descriptor.name);
    input.dataset.kind = "opt";
    input.dataset.name = descriptor.name;
    input.dataset.type = "bool";
    input.dataset.flag = "1";
    input.checked = !!descriptor.default;
    input.addEventListener("change", updateCliPreview);
    label.appendChild(input);
    label.appendChild(el("span", "box"));
    label.appendChild(document.createTextNode(" " + (descriptor.cli_flag || descriptor.name)));
    return label;
  }

  function renderForm(cmd) {
    var fieldsEl = qs("form-fields");
    var flagsEl = qs("form-flags");
    clear(fieldsEl);
    clear(flagsEl);

    cmd.arguments.forEach(function (arg) {
      fieldsEl.appendChild(renderNonFlagField("arg", arg));
    });
    cmd.options.forEach(function (opt) {
      if (opt.is_flag) {
        flagsEl.appendChild(renderFlagField(opt));
      } else {
        fieldsEl.appendChild(renderNonFlagField("opt", opt));
      }
    });
  }

  // ---------------------------------------------------------- form -> data

  function parseFieldValue(input) {
    var type = input.dataset.type;
    var multiple = input.dataset.multiple === "1";

    if (input.dataset.flag === "1") {
      return input.checked;
    }

    var raw = input.tagName === "SELECT" ? input.value : input.value;
    if (multiple) {
      var tokens = raw
        .split(",")
        .map(function (t) { return t.trim(); })
        .filter(function (t) { return t.length > 0; });
      if (tokens.length === 0 && input.dataset.required !== "1") return undefined;
      if (type === "int") return tokens.map(function (t) { return parseInt(t, 10); });
      if (type === "float") return tokens.map(function (t) { return parseFloat(t); });
      return tokens;
    }

    if (raw === "" || raw === null || raw === undefined) {
      return input.dataset.required === "1" ? "" : undefined;
    }
    if (type === "int") return parseInt(raw, 10);
    if (type === "float") return parseFloat(raw);
    return raw;
  }

  function collectFormValues() {
    var args = {};
    var opts = {};
    document.querySelectorAll("#form-fields input, #form-fields select, #form-flags input").forEach(function (input) {
      var value = parseFieldValue(input);
      if (value === undefined) return;
      if (input.dataset.kind === "arg") {
        args[input.dataset.name] = value;
      } else {
        opts[input.dataset.name] = value;
      }
    });
    return { arguments: args, options: opts };
  }

  // ------------------------------------------------------- CLI preview mirror

  /**
   * Cosmetic mirror of ExecutionService.build_argv's serialization rules
   * (dia_console/execution.py), for *display only*. The real argv is always
   * built server-side in build_argv when /api/execute actually runs — if
   * these two ever drift apart, only this preview text goes stale; the real
   * run is unaffected either way (Open Design Question 1).
   */
  function buildCliPreviewMirror(cmd, values) {
    var words = ["dia"].concat(cmd.path);

    cmd.options.forEach(function (opt) {
      if (!(opt.name in values.options)) return;
      var value = values.options[opt.name];
      if (opt.is_flag) {
        if (value) words.push(opt.cli_flag);
        return;
      }
      if (value === null || value === undefined) return;
      if (Array.isArray(value)) {
        value.forEach(function (item) { words.push(opt.cli_flag, String(item)); });
        return;
      }
      words.push(opt.cli_flag, String(value));
    });

    cmd.arguments.forEach(function (arg) {
      if (!(arg.name in values.arguments)) return;
      var value = values.arguments[arg.name];
      if (value === null || value === undefined) return;
      if (Array.isArray(value)) {
        value.forEach(function (item) { words.push(String(item)); });
        return;
      }
      words.push(String(value));
    });

    return words.join(" ");
  }

  function updateCliPreview() {
    if (!state.selectedCommand) return;
    var values = collectFormValues();
    qs("cli-preview-text").textContent = buildCliPreviewMirror(state.selectedCommand, values);
  }

  // ------------------------------------------------------------- selection

  function selectCommand(commandId) {
    var cmd = state.commandsById[commandId];
    if (!cmd) return;
    state.selectedCommand = cmd;

    qs("cmdheader").classList.remove("empty");
    qs("cmdheader-name").textContent = commandDisplayName(cmd);
    qs("cmdheader-desc").textContent = cmd.description || "";
    qs("form-area").style.display = "";

    renderForm(cmd);
    applyContextToCurrentForm();
    updateCliPreview();
    renderNav();
  }

  // -------------------------------------------------------------- run tabs
  //
  // console-run-tabs.md: the web UI tracks arbitrary concurrent executions,
  // mirroring the backend's app.state.executions. Each RunTab owns its own
  // pair of EventSources, its own log buffer, and its own results -- only
  // the currently-active tab's data is reflected into the live DOM.

  /** Maps a RunTab's internal status to the `.dot` accent class. */
  function statusDotClass(status) {
    if (status === "running") return "accent";
    if (status === "completed") return "green";
    if (status === "failed") return "red";
    if (status === "cancelled") return "yellow";
    return "";
  }

  function getActiveTab() {
    if (state.activeRunId === null) return null;
    for (var i = 0; i < state.runs.length; i++) {
      if (state.runs[i].id === state.activeRunId) return state.runs[i];
    }
    return null;
  }

  /**
   * Goal 4's de-dup rule: base label is the command's display name; if N
   * tabs currently open already share that base label, the new tab gets
   * " (N+1)" appended. Counts only tabs currently in state.runs, so a
   * closed tab's number is free to be reused by a later one.
   */
  function nextTabLabel(cmd) {
    var baseLabel = commandDisplayName(cmd);
    var count = state.runs.filter(function (t) { return t.baseLabel === baseLabel; }).length;
    if (count === 0) return baseLabel;
    return baseLabel + " (" + (count + 1) + ")";
  }

  function createRunTab(cmd) {
    runTabCounter += 1;
    var baseLabel = commandDisplayName(cmd);
    return {
      id: "run-" + runTabCounter,
      executionId: null,
      commandId: cmd.id,
      baseLabel: baseLabel,
      label: nextTabLabel(cmd),
      status: "running",
      eventsSource: null,
      logsSource: null,
      statuslineText: "Starting…",
      running: true,
      logLines: [],
      activeSubTab: "log",
      results: [],
    };
  }

  /** Builds a `.logline` div from a buffered {text, cls, ts} entry. */
  function buildLoglineElement(entry) {
    var line = el("div", "logline" + (entry.cls ? " " + entry.cls : ""));
    line.appendChild(el("span", "ts", entry.ts));
    line.appendChild(document.createTextNode(entry.text));
    return line;
  }

  /**
   * Buffers a log line onto `tab.logLines` and, only if `tab` is the
   * currently active tab, also appends it straight into the live #logpane
   * DOM -- a thin wrapper around buildLoglineElement so backgrounded tabs
   * keep accumulating without ever touching the DOM.
   */
  function appendLogLineForTab(tab, text) {
    var lower = text.toLowerCase();
    var cls = "";
    if (lower.indexOf("error") !== -1) cls = "err";
    else if (lower.indexOf("warn") !== -1) cls = "warn";

    var entry = { text: text, cls: cls, ts: new Date().toLocaleTimeString() };
    tab.logLines.push(entry);

    if (tab.id === state.activeRunId) {
      var pane = qs("logpane");
      var hint = pane.querySelector(".empty-hint");
      if (hint) hint.remove();
      pane.appendChild(buildLoglineElement(entry));
      pane.scrollTop = pane.scrollHeight;
    }
  }

  /** Fetches /results for a tab's execution; only repaints if still active. */
  function fetchResultsForTab(tab) {
    if (!tab.executionId) return;
    fetch("/api/executions/" + tab.executionId + "/results")
      .then(function (response) { return response.json(); })
      .then(function (records) {
        tab.results = records;
        if (tab.id === state.activeRunId) renderResults(records);
      })
      .catch(function () { /* leave whatever is currently shown */ });
  }

  /** Header runstate (#runstate-dot/#runstate-text): how many tabs are running. */
  function setRunstateText(dotClass, text) {
    qs("runstate-dot").className = "dot" + (dotClass ? " " + dotClass : "");
    qs("runstate-text").textContent = text;
  }

  function recomputeRunstate() {
    var runningCount = state.runs.filter(function (t) { return t.status === "running"; }).length;
    if (runningCount === 0) {
      setRunstateText("", "Idle");
    } else if (runningCount === 1) {
      setRunstateText("accent", "Running");
    } else {
      setRunstateText("accent", runningCount + " running");
    }
  }

  function applySubTabDom(subtab) {
    qs("tab-log").classList.toggle("active", subtab === "log");
    qs("tab-results").classList.toggle("active", subtab === "results");
    qs("logpane").style.display = subtab === "log" ? "" : "none";
    qs("resultspane").style.display = subtab === "results" ? "" : "none";
    qs("pane-clear").classList.toggle("hidden", subtab !== "log");
  }

  /** Click handler for the Log/Results sub-tabs, scoped to the active RunTab. */
  function selectSubTab(subtab) {
    var tab = getActiveTab();
    if (tab) tab.activeSubTab = subtab;
    applySubTabDom(subtab);
    if (subtab === "results" && tab) fetchResultsForTab(tab);
  }

  /**
   * Repaints the ENTIRE output area (statusline, log pane, sub-tab
   * selection, results) from state.activeRunId's own stored RunTab data --
   * no re-fetching. Used on tab switch, tab creation, and tab close.
   */
  function renderActiveRunTab() {
    var tab = getActiveTab();
    var pane = qs("logpane");

    if (!tab) {
      qs("statusline-text").textContent = "No execution yet.";
      qs("statusline").classList.remove("running");
      qs("statusline-dot").className = "dot";
      qs("run-btn").disabled = false;
      qs("cancel-btn").disabled = true;
      clearLogPane();
      renderResults([]);
      applySubTabDom("log");
      return;
    }

    qs("statusline-text").textContent = tab.statuslineText || "No execution yet.";
    qs("statusline").classList.toggle("running", !!tab.running);
    var dotClass = statusDotClass(tab.status);
    qs("statusline-dot").className = "dot" + (dotClass ? " " + dotClass : "");
    // Run is never disabled by the active tab's status -- Run always opens a
    // NEW tab (Goal 2), including while another tab is still running (the
    // whole point of this feature: start the editor without waiting for the
    // game to finish). Only Cancel is scoped to the active tab.
    qs("run-btn").disabled = false;
    qs("cancel-btn").disabled = !tab.running;

    clear(pane);
    if (!tab.logLines.length) {
      pane.appendChild(el("div", "empty-hint", "Run a command to see its live output here."));
    } else {
      tab.logLines.forEach(function (entry) {
        pane.appendChild(buildLoglineElement(entry));
      });
      pane.scrollTop = pane.scrollHeight;
    }

    applySubTabDom(tab.activeSubTab || "log");
    renderResults(tab.results || []);
  }

  /** Rebuilds the #runtabstrip from state.runs, in creation order. */
  function renderRunTabStrip() {
    var strip = qs("runtabstrip");
    clear(strip);

    state.runs.forEach(function (tab) {
      var tabEl = el("div", "runtab" + (tab.id === state.activeRunId ? " active" : ""));
      tabEl.dataset.state = tab.status;

      var dotClass = statusDotClass(tab.status);
      tabEl.appendChild(el("span", "dot" + (dotClass ? " " + dotClass : "")));
      tabEl.appendChild(el("span", "runtab-label", tab.label));

      var closeEl = el("span", "runtab-close", "✕");
      closeEl.addEventListener("click", function (event) {
        event.stopPropagation();
        closeRunTab(tab);
      });
      tabEl.appendChild(closeEl);

      tabEl.addEventListener("click", function () {
        if (state.activeRunId === tab.id) return;
        state.activeRunId = tab.id;
        renderActiveRunTab();
        renderRunTabStrip();
      });

      strip.appendChild(tabEl);
    });
  }

  function closeTabStreams(tab) {
    if (tab.eventsSource) { tab.eventsSource.close(); tab.eventsSource = null; }
    if (tab.logsSource) { tab.logsSource.close(); tab.logsSource = null; }
  }

  /**
   * Removes a tab from state.runs and closes its streams. If it was the
   * active tab, focuses the tab immediately to its left (clamped to the
   * new last tab if the closed tab was the leftmost), or null if none
   * remain (Goal 7).
   */
  function removeRunTab(tab) {
    var index = state.runs.indexOf(tab);
    if (index === -1) return;
    state.runs.splice(index, 1);
    closeTabStreams(tab);

    if (tab.id === state.activeRunId) {
      if (state.runs.length === 0) {
        state.activeRunId = null;
      } else {
        var focusIndex = index - 1;
        if (focusIndex < 0) focusIndex = 0;
        if (focusIndex > state.runs.length - 1) focusIndex = state.runs.length - 1;
        state.activeRunId = state.runs[focusIndex].id;
      }
    }

    renderRunTabStrip();
    renderActiveRunTab();
    recomputeRunstate();
  }

  /** Shared POST .../cancel call, used by both the per-tab close button and #cancel-btn. */
  function cancelExecution(executionId) {
    if (!executionId) return Promise.resolve();
    return fetch("/api/executions/" + executionId + "/cancel", { method: "POST" }).catch(function () {});
  }

  /** .runtab-close click: cancel first if still running, then remove regardless. */
  function closeRunTab(tab) {
    if (tab.status === "running") {
      cancelExecution(tab.executionId).then(function () {
        removeRunTab(tab);
      });
    } else {
      removeRunTab(tab);
    }
  }

  /**
   * Opens this RunTab's own pair of EventSources at creation time (not just
   * when active) so backgrounded tabs keep accumulating. Only touches the
   * live DOM when `tab` is the currently active tab.
   */
  function startStreamsForTab(tab) {
    var executionId = tab.executionId;

    var events = new EventSource("/api/executions/" + executionId + "/events");
    tab.eventsSource = events;
    events.onmessage = function (message) {
      var payload = JSON.parse(message.data);
      tab.statuslineText = (payload.step_id ? payload.step_id + " · " : "") + payload.type;
      if (tab.id === state.activeRunId) {
        qs("statusline-text").textContent = tab.statuslineText;
      }

      if (TERMINAL_EXECUTION_TYPES.indexOf(payload.type) !== -1 && payload.final !== false) {
        tab.running = false;
        if (payload.type === "execution.completed") tab.status = "completed";
        else if (payload.type === "execution.failed") tab.status = "failed";
        else tab.status = "cancelled";

        if (tab.eventsSource === events) { events.close(); tab.eventsSource = null; }

        fetchResultsForTab(tab);
        recomputeRunstate();
        renderRunTabStrip();
        if (tab.id === state.activeRunId) renderActiveRunTab();

        // Give the /logs stream a moment to drain trailing lines, then close
        // it too — otherwise EventSource would try to reconnect once the
        // server-side stream naturally ends.
        window.setTimeout(function () {
          if (tab.logsSource) { tab.logsSource.close(); tab.logsSource = null; }
        }, 300);
      }
    };
    events.onerror = function () {
      // Connection dropped unexpectedly (server gone, etc). Don't spin
      // forever retrying against a run that's no longer trackable.
      if (tab.eventsSource === events) {
        events.close();
        tab.eventsSource = null;
      }
    };

    var logs = new EventSource("/api/executions/" + executionId + "/logs");
    tab.logsSource = logs;
    logs.onmessage = function (message) {
      appendLogLineForTab(tab, JSON.parse(message.data));
    };
    logs.onerror = function () {
      if (tab.logsSource === logs) {
        logs.close();
        tab.logsSource = null;
      }
    };
  }

  // -------------------------------------------------------------- run flow

  /** #cancel-btn: cancels the currently active tab specifically. */
  function cancelRun() {
    var tab = getActiveTab();
    if (!tab || !tab.executionId) return;
    cancelExecution(tab.executionId);
  }

  /** Empties the log pane and restores its idle placeholder. */
  function clearLogPane() {
    var pane = qs("logpane");
    clear(pane);
    pane.appendChild(el("div", "empty-hint", "Run a command to see its live output here."));
  }

  /** Copies whichever of logpane/resultspane is currently visible. */
  function copyActivePaneContents() {
    var pane = qs("logpane").style.display !== "none" ? qs("logpane") : qs("resultspane");
    var text = pane.textContent.trim();
    if (!text || !navigator.clipboard) return;
    navigator.clipboard.writeText(text).then(function () {
      var btn = qs("pane-copy");
      btn.textContent = "copied";
      btn.classList.add("copied");
      setTimeout(function () {
        btn.textContent = "copy";
        btn.classList.remove("copied");
      }, 1200);
    }).catch(function () {});
  }

  // ---------------------------------------------------------- results tab

  /**
   * Renders one severity-grouped card. `record` is a ResultRecord JSON
   * object (kind/severity/title/summary/payload) from GET
   * /api/executions/{id}/results -- this file never parses XML or
   * duplicates dia_console/results.py's adapter logic (SD-CONSOLE-005); it
   * only ever renders what the server already classified.
   */
  function renderResultCard(record) {
    var card = el("div", "card" + (record.severity === "error" ? " err" : ""));
    var title = el("div", "card-title", record.title || record.kind);
    card.appendChild(title);
    if (record.summary) {
      card.appendChild(el("div", "card-sub", record.summary));
    }
    var detailBits = [];
    if (record.payload) {
      Object.keys(record.payload).forEach(function (key) {
        var value = record.payload[key];
        if (value === null || value === undefined) return;
        if (Array.isArray(value)) return; // e.g. logTail — too long for a one-line detail
        detailBits.push(key + ": " + value);
      });
    }
    if (detailBits.length) {
      card.appendChild(el("div", "card-detail", detailBits.join(" · ")));
    }
    return card;
  }

  function renderArtifactsLine(records) {
    if (!records.length) return null;
    var line = el("div", "artifacts-line");
    line.appendChild(el("span", "lbl", "artifacts"));
    records.forEach(function (record, index) {
      if (index > 0) line.appendChild(el("span", "sep", "·"));
      var path = (record.payload && record.payload.path) || record.title;
      line.appendChild(el("span", null, path));
    });
    return line;
  }

  function renderResults(records) {
    var pane = qs("resultspane");
    clear(pane);

    if (!records || !records.length) {
      // .resultspane's base rule centers its lone child (State A, the static
      // empty state) — real cards (State B) instead stack top-aligned, per
      // the mockup's .resultspane rule (flex column + gap, no centering).
      // Overridden inline here rather than via a new CSS class.
      pane.style.alignItems = "";
      pane.style.justifyContent = "";
      pane.style.gap = "";
      pane.appendChild(el("div", "empty-state", "No structured results yet."));
      return;
    }

    pane.style.alignItems = "stretch";
    pane.style.justifyContent = "flex-start";
    pane.style.gap = "8px";

    var cards = records.filter(function (r) { return r.kind !== "Artifact"; });
    var artifacts = records.filter(function (r) { return r.kind === "Artifact"; });

    cards.forEach(function (record) {
      pane.appendChild(renderResultCard(record));
    });

    var artifactsLine = renderArtifactsLine(artifacts);
    if (artifactsLine) pane.appendChild(artifactsLine);
  }

  // -------------------------------------------------------------- run flow

  /**
   * Always appends a new RunTab and makes it active -- never reuses or
   * restarts an existing tab, even for the identical command (Goal 2).
   */
  function runSelectedCommand() {
    if (!state.selectedCommand) return;
    var cmd = state.selectedCommand;
    var values = collectFormValues();

    var tab = createRunTab(cmd);
    state.runs.push(tab);
    state.activeRunId = tab.id;

    renderActiveRunTab();
    renderRunTabStrip();
    recomputeRunstate();

    fetch("/api/execute", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        command_id: cmd.id,
        project_id: null,
        arguments: values.arguments,
        options: values.options,
      }),
    })
      .then(function (response) {
        if (!response.ok) throw new Error("execute failed: " + response.status);
        return response.json();
      })
      .then(function (body) {
        tab.executionId = body.execution_id;
        startStreamsForTab(tab);
      })
      .catch(function (err) {
        tab.running = false;
        tab.status = "failed";
        tab.statuslineText = "failed to start";
        appendLogLineForTab(tab, "ERROR: " + err.message);
        recomputeRunstate();
        renderRunTabStrip();
        if (tab.id === state.activeRunId) renderActiveRunTab();
      });
  }

  // ---------------------------------------------------------------- boot

  function loadCommands() {
    return fetch("/api/commands")
      .then(function (response) { return response.json(); })
      .then(function (commands) {
        state.commands = commands;
        state.commandsById = {};
        commands.forEach(function (cmd) { state.commandsById[cmd.id] = cmd; });
        renderNav();
      });
  }

  function wireStaticControls() {
    qs("run-btn").addEventListener("click", runSelectedCommand);
    qs("cancel-btn").addEventListener("click", cancelRun);
    qs("save-preset-btn").addEventListener("click", saveCurrentPreset);
    qs("tab-log").addEventListener("click", function () { selectSubTab("log"); });
    qs("tab-results").addEventListener("click", function () { selectSubTab("results"); });
    qs("pane-copy").addEventListener("click", copyActivePaneContents);
    qs("pane-clear").addEventListener("click", clearLogPane);
    qs("nav-search").addEventListener("input", applyNavFilter);
    qs("target-select").addEventListener("change", onTargetSelectChanged);
    qs("config-select").addEventListener("change", onConfigSelectChanged);
    qs("cli-preview-copy").addEventListener("click", function () {
      var text = qs("cli-preview-text").textContent;
      if (navigator.clipboard) navigator.clipboard.writeText(text).catch(function () {});
    });

    // console-run-tabs.md: boot with an empty tab strip / idle output area.
    renderRunTabStrip();
    renderActiveRunTab();
    recomputeRunstate();

    // Frameless window -> no native title bar controls; these three glyphs
    // are the only way to move/close the window. window.pywebview.api is
    // injected by pywebview's own bridge script asynchronously -- there is
    // no guarantee it already exists by the time this runs (confirmed by an
    // actual real click: the button existed and .click() didn't throw, but
    // nothing happened, because window.pywebview.api was still undefined
    // when this code ran and so never got the chance to attach anything).
    // pywebview dispatches window's own 'pywebviewready' CustomEvent once
    // its bridge is genuinely ready (webview/js/finish.js) -- wait for that,
    // with a synchronous check first in case it already fired before this
    // script even ran.
    if (window.pywebview && window.pywebview.api) {
      wireTitlebarControls();
    } else {
      window.addEventListener("pywebviewready", wireTitlebarControls);
    }
  }

  function wireTitlebarControls() {
    document.querySelector(".winctrls .minimize").addEventListener("click", function () {
      window.pywebview.api.minimize();
    });
    document.querySelector(".winctrls .maximize").addEventListener("click", function () {
      window.pywebview.api.maximize();
    });
    document.querySelector(".winctrls .close").addEventListener("click", function () {
      window.pywebview.api.close();
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    wireStaticControls();
    loadCommands();
    loadContext();
    loadPresets();
  });
})();
