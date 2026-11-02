#pragma once

// CoWEditor - CoW's game-scoped editor plugin library.
// Linked into CluicheEditor only; CoW runtime code must never depend on this library.
// Plugins declared in cow_editor.diaapp (referenced from cow.diagame) are available
// only while the CoW game is loaded in the editor.

namespace CoW
{
namespace Editor
{
	// Registration anchor for CoW editor plugins. Currently empty (no plugins yet).
	// Plugin translation units added to this library register via REGISTER_EDITOR_PLUGIN;
	// referencing this symbol from CluicheEditor forces the linker to keep the library's objects.
	void RegisterCoWEditorPlugins();
}
}
