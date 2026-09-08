# System Spec: DiaUIUltralight

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ui

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Platform Initialisation | Bootstrap Ultralight Platform (FileSystem, FontLoader, Logger, Config) | [platform-initialisation.md](platform-initialisation.md) | Done |
| Page Loading | Load HTML pages from file:// URLs; blocking load pump | [page-loading.md](page-loading.md) | Done |
| Pixel Buffer Readback | CPU bitmap composite via UIDataBuffer (BGRA, 16 MB ceiling) | [pixel-buffer-readback.md](pixel-buffer-readback.md) | Done |
| JavaScript Binding | `window.app` global; JS→C++ via `BoundMethod`; `OnDOMReady` registration | [javascript-binding.md](javascript-binding.md) | Done |
| Input Injection | Mouse + scroll forwarding via `FireMouseEvent` / `FireScrollEvent` | [input-injection.md](input-injection.md) | Done |
| Game UI Framework Convention | Alpine.js + Tailwind + DaisyUI tier; vendored assets; file:// loading | [game-ui-framework-convention.md](game-ui-framework-convention.md) | Done |
| Game Input Bridge | `CallJSFunction` C++→JS push; keyboard injection; `EInputRouting` mode stack | [game-input-bridge.md](game-input-bridge.md) | Approved |
