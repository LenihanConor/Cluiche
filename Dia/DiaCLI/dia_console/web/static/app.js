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
    "Run & Debug",
    "Test & Quality",
    "Assets & Data",
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
    executionId: null,
    eventsSource: null,
    logsSource: null,
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

  // -------------------------------------------------------------- run flow

  function setRunning(isRunning) {
    qs("run-btn").disabled = isRunning;
    qs("cancel-btn").disabled = !isRunning;
    qs("statusline").classList.toggle("running", isRunning);
  }

  function setRunstate(kind, text) {
    var dotClass = { idle: "", running: "accent", success: "green", failure: "red" }[kind] || "";
    qs("runstate-dot").className = "dot" + (dotClass ? " " + dotClass : "");
    qs("runstate-text").textContent = text;
    qs("statusline-dot").className = "dot" + (dotClass ? " " + dotClass : "");
  }

  function appendLogLine(text) {
    var pane = qs("logpane");
    var hint = pane.querySelector(".empty-hint");
    if (hint) hint.remove();

    var line = el("div", "logline");
    var lower = text.toLowerCase();
    if (lower.indexOf("error") !== -1) line.className = "logline err";
    else if (lower.indexOf("warn") !== -1) line.className = "logline warn";

    var ts = el("span", "ts", new Date().toLocaleTimeString());
    line.appendChild(ts);
    line.appendChild(document.createTextNode(text));
    pane.appendChild(line);
    pane.scrollTop = pane.scrollHeight;
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

  function loadResults(executionId) {
    if (!executionId) return;
    fetch("/api/executions/" + executionId + "/results")
      .then(function (response) { return response.json(); })
      .then(function (records) { renderResults(records); })
      .catch(function () { /* leave whatever is currently shown */ });
  }

  function closeStreams() {
    if (state.eventsSource) { state.eventsSource.close(); state.eventsSource = null; }
    if (state.logsSource) { state.logsSource.close(); state.logsSource = null; }
  }

  function startStreams(executionId) {
    state.executionId = executionId;

    var events = new EventSource("/api/executions/" + executionId + "/events");
    state.eventsSource = events;
    events.onmessage = function (message) {
      var payload = JSON.parse(message.data);
      qs("statusline-text").textContent =
        (payload.step_id ? payload.step_id + " · " : "") + payload.type;
      if (TERMINAL_EXECUTION_TYPES.indexOf(payload.type) !== -1) {
        setRunning(false);
        if (payload.type === "execution.completed") setRunstate("success", "Idle");
        else setRunstate("failure", "Idle");
        events.close();
        state.eventsSource = null;
        loadResults(executionId);
        // Give the /logs stream a moment to drain trailing lines, then close
        // it too — otherwise EventSource would try to reconnect once the
        // server-side stream naturally ends.
        window.setTimeout(function () {
          if (state.logsSource) { state.logsSource.close(); state.logsSource = null; }
        }, 300);
      }
    };
    events.onerror = function () {
      // Connection dropped unexpectedly (server gone, etc). Don't spin
      // forever retrying against a run that's no longer trackable.
      if (state.eventsSource === events) {
        events.close();
        state.eventsSource = null;
      }
    };

    var logs = new EventSource("/api/executions/" + executionId + "/logs");
    state.logsSource = logs;
    logs.onmessage = function (message) {
      appendLogLine(JSON.parse(message.data));
    };
    logs.onerror = function () {
      if (state.logsSource === logs) {
        logs.close();
        state.logsSource = null;
      }
    };
  }

  function runSelectedCommand() {
    if (!state.selectedCommand) return;
    var values = collectFormValues();

    closeStreams();
    clear(qs("logpane"));
    renderResults([]);
    setRunning(true);
    setRunstate("running", "Running");
    setActiveTab("log");

    fetch("/api/execute", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        command_id: state.selectedCommand.id,
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
        startStreams(body.execution_id);
      })
      .catch(function (err) {
        setRunning(false);
        setRunstate("failure", "Idle");
        appendLogLine("ERROR: " + err.message);
      });
  }

  function cancelRun() {
    if (!state.executionId) return;
    fetch("/api/executions/" + state.executionId + "/cancel", { method: "POST" }).catch(function () {});
  }

  // -------------------------------------------------------------- tabstrip

  function setActiveTab(tab) {
    qs("tab-log").classList.toggle("active", tab === "log");
    qs("tab-results").classList.toggle("active", tab === "results");
    qs("logpane").style.display = tab === "log" ? "" : "none";
    qs("resultspane").style.display = tab === "results" ? "" : "none";
    qs("pane-clear").classList.toggle("hidden", tab !== "log");
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
    qs("tab-log").addEventListener("click", function () { setActiveTab("log"); });
    qs("tab-results").addEventListener("click", function () {
      setActiveTab("results");
      if (state.executionId) loadResults(state.executionId);
    });
    qs("pane-copy").addEventListener("click", copyActivePaneContents);
    qs("pane-clear").addEventListener("click", clearLogPane);
    qs("nav-search").addEventListener("input", applyNavFilter);
    qs("target-select").addEventListener("change", onTargetSelectChanged);
    qs("config-select").addEventListener("change", onConfigSelectChanged);
    qs("cli-preview-copy").addEventListener("click", function () {
      var text = qs("cli-preview-text").textContent;
      if (navigator.clipboard) navigator.clipboard.writeText(text).catch(function () {});
    });

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
