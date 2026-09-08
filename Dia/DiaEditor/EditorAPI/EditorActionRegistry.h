#pragma once

#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>
#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Metric/Counter.h>

namespace Dia
{
	namespace Editor
	{
		// C++ action registry — single registration point for all editor actions.
		// Cross-registers every action with DiaAPI::CommandRegistry (JSON path) so
		// Python and the CLI can reach the same handlers without a separate registration step.
		//
		// Thread safety: Register/Deregister must be called from the main thread only.
		// ExecuteAction dispatches to the main-thread queue for kMainThread actions and
		// runs inline for kCallerThread actions.
		class EditorActionRegistry
		{
		public:
			EditorActionRegistry();

			void Initialize();
			void Shutdown();

			// Register an action. Returns false and logs a warning if the name is
			// already registered. Adds a handler-free entry to the manifest and
			// cross-registers with DiaAPI::RegisterCommandJson.
			bool RegisterAction(const EditorActionDescriptor& descriptor);

			// Remove all actions whose owner string matches owner. Called in Shutdown
			// by each subsystem that previously called RegisterAction.
			void DeregisterActionsForOwner(Dia::Core::StringCRC owner);

			// Execute an action by name. For kMainThread actions the handler is called
			// inline on whatever thread calls this (callers must ensure main-thread
			// context). Returns {"success":false,"reason":"unknown_action"} on miss.
			Json::Value ExecuteAction(Dia::Core::StringCRC name, const Json::Value& params);

			// Read-only manifest — all registered actions sans handlers.
			const EditorActionManifest& GetManifest() const;

		private:
			static const unsigned int kMaxActions = EditorActionManifest::kMaxActions;

			struct HandlerEntry
			{
				Dia::Core::StringCRC name;
				ActionHandler        handler;
			};

			// Parallel to mManifest — same index, stores the handler the manifest omits.
			Dia::Core::Containers::DynamicArrayC<HandlerEntry, kMaxActions> mHandlers;

			EditorActionManifest mManifest;

			Dia::Observation::Metric::Counter* mRegisteredCounter   = nullptr;
			Dia::Observation::Metric::Counter* mDeregisteredCounter = nullptr;

			bool mInitialized = false;
		};

	} // namespace Editor
} // namespace Dia
