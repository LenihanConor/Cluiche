#pragma once

#include <DiaAPI/CommandRegistry/ValueParam.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <functional>

namespace Dia
{
	namespace Editor
	{
		enum class DispatchThread
		{
			kMainThread,    // action queued to main thread; caller blocks on future
			kCallerThread   // action executes inline on calling thread (read-only only)
		};

		// Single parameter definition for an action (shared with DiaAPI)
		using EditorActionParam = Dia::API::ValueParam;

		// Param schema: up to 8 params per action (PD-004: DiaCore containers, no STL)
		struct EditorActionParamSchema
		{
			static const unsigned int kMaxParams = 8;
			Dia::Core::Containers::DynamicArrayC<EditorActionParam, kMaxParams> params;
		};

		// Handler signature: receives JSON params, returns JSON result
		using ActionHandler = std::function<Json::Value(const Json::Value& params)>;

		// Full descriptor for one registered action
		struct EditorActionDescriptor
		{
			Dia::Core::StringCRC    name;                           // e.g. StringCRC("project.open_path")
			const char*             description = nullptr;          // human + AI readable (rich, ~2-4 sentences)
			const char*             category    = nullptr;          // e.g. "project", "plugin"
			const char*             owner       = nullptr;          // e.g. "AppEditorController"
			EditorActionParamSchema params;
			DispatchThread          dispatchThread = DispatchThread::kMainThread;
			ActionHandler           handler;
		};

	} // namespace Editor
} // namespace Dia
