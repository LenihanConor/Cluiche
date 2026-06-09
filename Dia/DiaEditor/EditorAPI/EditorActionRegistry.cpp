#include "DiaEditor/EditorAPI/EditorActionRegistry.h"
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Dia
{
	namespace Editor
	{
		EditorActionRegistry::EditorActionRegistry() = default;

		void EditorActionRegistry::Initialize()
		{
			mRegisteredCounter   = Dia::Observation::Metric::MetricRegistry::Instance()
			                          .RegisterCounter(Dia::Core::StringCRC("editor.actions_registered"));
			mDeregisteredCounter = Dia::Observation::Metric::MetricRegistry::Instance()
			                          .RegisterCounter(Dia::Core::StringCRC("editor.actions_deregistered"));
			mInitialized = true;
		}

		void EditorActionRegistry::Shutdown()
		{
			mHandlers.RemoveAll();
			mManifest  = EditorActionManifest{};
			mInitialized = false;
		}

		bool EditorActionRegistry::RegisterAction(const EditorActionDescriptor& descriptor)
		{
			if (descriptor.name == Dia::Core::StringCRC())
			{
				DIA_LOG_WARNING("EditorActionRegistry", "RegisterAction called with empty name — ignored");
				return false;
			}

			if (mManifest.FindByName(descriptor.name) != nullptr)
			{
				DIA_LOG_WARNING("EditorActionRegistry", "Duplicate action name — ignored");
				return false;
			}

			// Add handler entry (parallel to manifest)
			HandlerEntry he;
			he.name    = descriptor.name;
			he.handler = descriptor.handler;
			mHandlers.Add(he);

			// Add handler-free entry to manifest
			EditorActionEntry entry;
			entry.name           = descriptor.name;
			entry.description    = descriptor.description;
			entry.category       = descriptor.category;
			entry.owner          = descriptor.owner;
			entry.params         = descriptor.params;
			entry.dispatchThread = descriptor.dispatchThread;
			mManifest.Add(entry);

			// Cross-register with DiaAPI JSON command path
			if (descriptor.handler)
			{
				Dia::API::CommandInfoJson info;
				info.name        = descriptor.name;
				info.description = descriptor.description;
				info.owner       = descriptor.owner;
				info.callback    = descriptor.handler;
				Dia::API::RegisterCommandJson(info);
			}

			if (mRegisteredCounter)
				mRegisteredCounter->Inc();

			DIA_LOG_INFO("EditorActionRegistry", "Registered action");
			return true;
		}

		void EditorActionRegistry::DeregisterActionsForOwner(Dia::Core::StringCRC owner)
		{
			unsigned int removed = 0;

			// Remove from mHandlers
			unsigned int i = 0;
			while (i < mHandlers.Size())
			{
				const EditorActionEntry* entry = mManifest.FindByName(mHandlers[i].name);
				if (entry && entry->owner != nullptr && Dia::Core::StringCRC(entry->owner) == owner)
				{
					mHandlers.RemoveAt(i);
					++removed;
				}
				else
				{
					++i;
				}
			}

			mManifest.RemoveByOwner(owner);

			if (removed > 0 && mDeregisteredCounter)
			{
				for (unsigned int r = 0; r < removed; ++r)
					mDeregisteredCounter->Inc();
			}

			DIA_LOG_INFO("EditorActionRegistry", "Deregistered actions for owner");
		}

		Json::Value EditorActionRegistry::ExecuteAction(Dia::Core::StringCRC name, const Json::Value& params)
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
				{
					if (mHandlers[i].handler)
						return mHandlers[i].handler(params);

					Json::Value err;
					err["success"] = false;
					err["reason"]  = "no_handler";
					return err;
				}
			}

			DIA_LOG_ERROR("EditorActionRegistry", "ExecuteAction called with unknown action name");
			Json::Value err;
			err["success"] = false;
			err["reason"]  = "unknown_action";
			return err;
		}

		const EditorActionManifest& EditorActionRegistry::GetManifest() const
		{
			return mManifest;
		}

	} // namespace Editor
} // namespace Dia
