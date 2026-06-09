#pragma once

#include <DiaEditor/EditorAPI/EditorActionRegistry.h>
#include <DiaEditor/EditorAPI/EditorActionQueue.h>

namespace Dia
{
	namespace Editor
	{
		// Generates the `dia_editor` Python module tree from the manifest.
		// One sub-module per category (e.g. dia_editor.project, dia_editor.game_connection).
		// Each action becomes a callable that dispatches through the EditorActionQueue.
		//
		// Call after all plugins have registered their actions (after PluginLoaderModule loads).
		// Calling more than once logs a warning and is a no-op.
		void GeneratePythonModule(EditorActionRegistry* registry, EditorActionQueue* queue);

		// Write a dia_editor.pyi stub file to the given output path.
		// The stub documents every registered action with its parameter schema and return type.
		// Python tooling (mypy, IDEs) can import this for type checking and autocomplete.
		//
		// outputPath — full path to the .pyi file, e.g. "out/CluicheEditor/scripts/dia_editor.pyi"
		// Returns true if the file was written successfully.
		bool EmitPythonStubs(const EditorActionRegistry* registry, const char* outputPath);

	} // namespace Editor
} // namespace Dia
