#pragma once

#include "DiaAssetRuntimeEditor/Panels/AssetStateRow.h"

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

			class AssetStateTablePanel
			{
			public:
				static const unsigned int kMaxAssets = 4096;
				static const float kDefaultPollIntervalSeconds;

				AssetStateTablePanel();

				void Activate(Dia::Editor::WebUIBridge* bridge,
					Dia::Editor::GameConnectionManager* manager,
					SharedPluginState* state);
				void Deactivate();
				void Update(float deltaTime);

				void OnConnectionStateChanged(bool connected);
				void ForceRefresh();

				void SetPollInterval(float seconds);
				float GetPollInterval() const { return mPollInterval; }

				const Dia::Core::Containers::DynamicArrayC<AssetStateRow, kMaxAssets>& GetSnapshot() const { return mSnapshot; }

				static void ParseGetAllStatesResponse(const Json::Value& data,
					Dia::Core::Containers::DynamicArrayC<AssetStateRow, kMaxAssets>& outRows);

			private:
				void Poll();
				void HandlePollResponse(bool success, const Json::Value& result);
				void PushSnapshotToUI();

				Dia::Editor::WebUIBridge* mBridge;
				Dia::Editor::GameConnectionManager* mManager;
				SharedPluginState* mState;

				Dia::Core::Containers::DynamicArrayC<AssetStateRow, kMaxAssets> mSnapshot;

				float mPollInterval;
				float mPollTimer;
				bool mActive;
				bool mConnected;
			};
		}
	}
}
