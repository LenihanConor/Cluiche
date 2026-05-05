#pragma once

#include "DiaAssetRuntimeEditor/Panels/TransitionLogEntry.h"

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class GameConnectionManager;
	}

	namespace AssetRuntime
	{
		namespace Editor
		{
			struct SharedPluginState;

			class StateTransitionLogPanel
			{
			public:
				static const unsigned int kDefaultMaxEntries = 1000;
				static const unsigned int kMaxLogCapacity = 4096;

				StateTransitionLogPanel();

				void Activate(Dia::Editor::WebUIBridge* bridge,
					Dia::Editor::GameConnectionManager* manager,
					SharedPluginState* state);
				void Deactivate();
				void Update(float deltaTime);

				void OnConnectionStateChanged(bool connected);

				void SetMaxEntries(unsigned int max);
				unsigned int GetMaxEntries() const { return mMaxEntries; }
				void SetPaused(bool paused);
				bool IsPaused() const { return mPaused; }
				void ClearLog();

			private:
				void SubscribeToTransitions();
				void UnsubscribeFromTransitions();
				void HandleTransitionEvent(const Json::Value& data);
				void AppendEntry(const TransitionLogEntry& entry);
				void EnforceFIFO();
				void PushLogToUI();
				void AppendMarker(LogEntryType type);

				Dia::Editor::WebUIBridge* mBridge;
				Dia::Editor::GameConnectionManager* mManager;
				SharedPluginState* mState;

				Dia::Core::Containers::DynamicArrayC<TransitionLogEntry, kMaxLogCapacity> mLog;

				unsigned int mMaxEntries;
				bool mPaused;
				bool mActive;
				bool mConnected;
				bool mSubscribed;
			};
		}
	}
}
