#include "DiaAssetRuntimeEditor/Panels/StateTransitionLogPanel.h"
#include "DiaAssetRuntimeEditor/SharedPluginState.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>

#include <chrono>

namespace Dia
{
	namespace AssetRuntime
	{
		namespace Editor
		{
			StateTransitionLogPanel::StateTransitionLogPanel()
				: mBridge(nullptr)
				, mManager(nullptr)
				, mState(nullptr)
				, mLogHead(0)
				, mLogCount(0)
				, mMaxEntries(kDefaultMaxEntries)
				, mGeneration(0)
				, mPaused(false)
				, mActive(false)
				, mConnected(false)
				, mSubscribed(false)
			{}

			void StateTransitionLogPanel::Activate(Dia::Editor::WebUIBridge* bridge,
				Dia::Editor::GameConnectionManager* manager,
				SharedPluginState* state)
			{
				mBridge = bridge;
				mManager = manager;
				mState = state;
				mActive = true;

				if (mBridge)
				{
					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.log_pause"),
						[this](const Json::Value& /*data*/) -> Json::Value
						{
							SetPaused(true);
							Json::Value result;
							result["success"] = true;
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.log_resume"),
						[this](const Json::Value& /*data*/) -> Json::Value
						{
							SetPaused(false);
							PushLogToUI();
							Json::Value result;
							result["success"] = true;
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.log_clear"),
						[this](const Json::Value& /*data*/) -> Json::Value
						{
							ClearLog();
							Json::Value result;
							result["success"] = true;
							return result;
						});

					mBridge->RegisterRequestHandler(
						Dia::Core::StringCRC("asset_runtime_editor.log_set_max"),
						[this](const Json::Value& data) -> Json::Value
						{
							Json::Value result;
							if (data.isMember("max") && data["max"].isNumeric())
							{
								SetMaxEntries(data["max"].asUInt());
								result["success"] = true;
							}
							else
							{
								result["success"] = false;
								result["error"] = "missing max";
							}
							return result;
						});
				}
			}

			void StateTransitionLogPanel::Deactivate()
			{
				++mGeneration;

				if (mSubscribed)
					UnsubscribeFromTransitions();

				if (mBridge)
				{
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.log_pause"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.log_resume"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.log_clear"));
					mBridge->UnregisterRequestHandler(Dia::Core::StringCRC("asset_runtime_editor.log_set_max"));
				}

				mActive = false;
				mBridge = nullptr;
				mManager = nullptr;
				mState = nullptr;
			}

			void StateTransitionLogPanel::Update(float /*deltaTime*/)
			{
			}

			void StateTransitionLogPanel::OnConnectionStateChanged(bool connected)
			{
				mConnected = connected;

				if (mBridge)
				{
					Json::Value data;
					data["connected"] = connected;
					mBridge->NotifyUIDataChanged("asset_runtime_editor.log_connection_state", data);
				}

				if (connected)
				{
					AppendMarker(LogEntryType::kMarkerReconnect);
					SubscribeToTransitions();
				}
				else
				{
					AppendMarker(LogEntryType::kMarkerDisconnect);
					if (mSubscribed)
					{
						mSubscribed = false;
					}
				}

				PushLogToUI();
			}

			const TransitionLogEntry& StateTransitionLogPanel::GetLogEntry(unsigned int logicalIndex) const
			{
				unsigned int bufIdx = (mLogHead + logicalIndex) % kMaxLogCapacity;
				return mLogBuffer[bufIdx];
			}

			void StateTransitionLogPanel::SetMaxEntries(unsigned int max)
			{
				if (max < 10) max = 10;
				if (max > kMaxLogCapacity) max = kMaxLogCapacity;
				mMaxEntries = max;
				EnforceFIFO();
				PushLogToUI();
			}

			void StateTransitionLogPanel::SetPaused(bool paused)
			{
				mPaused = paused;
			}

			void StateTransitionLogPanel::ClearLog()
			{
				mLogHead = 0;
				mLogCount = 0;
				PushLogToUI();
			}

			void StateTransitionLogPanel::SubscribeToTransitions()
			{
				if (!mManager || mSubscribed)
					return;

				unsigned int gen = mGeneration;
				mManager->Subscribe(Dia::Core::StringCRC("asset_runtime.transitions"),
					[this, gen](const Json::Value& data)
					{
						if (gen != mGeneration)
							return;
						HandleTransitionEvent(data);
					});
				mSubscribed = true;

				// Also send the subscribe command to the game
				Json::Value args(Json::objectValue);
				mManager->SendCommand("asset_runtime.subscribe_transitions", args);
			}

			void StateTransitionLogPanel::UnsubscribeFromTransitions()
			{
				if (!mManager)
					return;
				mManager->Unsubscribe(Dia::Core::StringCRC("asset_runtime.transitions"));
				mSubscribed = false;
			}

			void StateTransitionLogPanel::HandleTransitionEvent(const Json::Value& data)
			{
				if (!mActive)
					return;

				TransitionLogEntry entry;
				entry.mType = LogEntryType::kTransition;

				if (data.isMember("assetId") && data["assetId"].isString())
					entry.mAssetId = Dia::Core::StringCRC(data["assetId"].asCString());

				if (data.isMember("oldState") && data["oldState"].isString())
					entry.mOldState = StringToAssetStateEnum(data["oldState"].asCString());

				if (data.isMember("newState") && data["newState"].isString())
					entry.mNewState = StringToAssetStateEnum(data["newState"].asCString());

				if (data.isMember("timestamp") && data["timestamp"].isNumeric())
					entry.mTimestamp = data["timestamp"].asUInt64();
				else
				{
					auto now = std::chrono::system_clock::now();
					entry.mTimestamp = static_cast<unsigned long long>(
						std::chrono::duration_cast<std::chrono::milliseconds>(
							now.time_since_epoch()).count());
				}

				AppendEntry(entry);

				if (!mPaused)
					PushIncrementalToUI(entry);
			}

			void StateTransitionLogPanel::AppendEntry(const TransitionLogEntry& entry)
			{
				unsigned int writeIdx = (mLogHead + mLogCount) % kMaxLogCapacity;
				mLogBuffer[writeIdx] = entry;

				if (mLogCount < kMaxLogCapacity)
					mLogCount++;
				else
					mLogHead = (mLogHead + 1) % kMaxLogCapacity;

				EnforceFIFO();
			}

			void StateTransitionLogPanel::AppendMarker(LogEntryType type)
			{
				TransitionLogEntry entry;
				entry.mType = type;

				auto now = std::chrono::system_clock::now();
				entry.mTimestamp = static_cast<unsigned long long>(
					std::chrono::duration_cast<std::chrono::milliseconds>(
						now.time_since_epoch()).count());

				AppendEntry(entry);
			}

			void StateTransitionLogPanel::EnforceFIFO()
			{
				while (mLogCount > mMaxEntries)
				{
					mLogHead = (mLogHead + 1) % kMaxLogCapacity;
					mLogCount--;
				}
			}

			void StateTransitionLogPanel::PushLogToUI()
			{
				if (!mBridge)
					return;

				Json::Value data;
				Json::Value entries(Json::arrayValue);

				// Send in reverse chronological order (newest first)
				for (int i = static_cast<int>(mLogCount) - 1; i >= 0; --i)
				{
					const TransitionLogEntry& entry = GetLogEntry(static_cast<unsigned int>(i));
					Json::Value e;
					e["timestamp"] = static_cast<Json::UInt64>(entry.mTimestamp);

					switch (entry.mType)
					{
						case LogEntryType::kTransition:
							e["type"] = "transition";
							e["assetId"] = entry.mAssetId.AsChar();
							e["oldState"] = AssetStateEnumToString(entry.mOldState);
							e["newState"] = AssetStateEnumToString(entry.mNewState);
							break;
						case LogEntryType::kMarkerDisconnect:
							e["type"] = "disconnect";
							break;
						case LogEntryType::kMarkerReconnect:
							e["type"] = "reconnect";
							break;
					}

					entries.append(e);
				}

				data["entries"] = entries;
				data["total"] = static_cast<int>(mLogCount);
				data["paused"] = mPaused;
				data["maxEntries"] = static_cast<int>(mMaxEntries);
				mBridge->NotifyUIDataChanged("asset_runtime_editor.log_data", data);
			}

			void StateTransitionLogPanel::PushIncrementalToUI(const TransitionLogEntry& entry)
			{
				if (!mBridge)
					return;

				Json::Value data;
				Json::Value e;
				e["timestamp"] = static_cast<Json::UInt64>(entry.mTimestamp);

				switch (entry.mType)
				{
					case LogEntryType::kTransition:
						e["type"] = "transition";
						e["assetId"] = entry.mAssetId.AsChar();
						e["oldState"] = AssetStateEnumToString(entry.mOldState);
						e["newState"] = AssetStateEnumToString(entry.mNewState);
						break;
					case LogEntryType::kMarkerDisconnect:
						e["type"] = "disconnect";
						break;
					case LogEntryType::kMarkerReconnect:
						e["type"] = "reconnect";
						break;
				}

				data["entry"] = e;
				data["total"] = static_cast<int>(mLogCount);
				data["paused"] = mPaused;
				mBridge->NotifyUIDataChanged("asset_runtime_editor.log_entry", data);
			}
		}
	}
}
