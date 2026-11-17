#pragma once

#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Editor
	{
		// Inspector-tier plugin: live performance view of a running game over DiaDebugServer.
		// Shell only: forwards the perf.timing push to the UI and relays perf.window / perf.frame queries.
		class DiaPerfInspectorPlugin : public LiveConnectionPluginBase
		{
		public:
			DiaPerfInspectorPlugin();

		protected:
			void OnLivePluginLoad() override;
			void OnLivePluginUnload() override;
			void OnGameConnected() override;
			void OnGameDisconnected() override;

		private:
			Json::Value HandleQueryWindow(const Json::Value& data);
			Json::Value HandleQueryFrame(const Json::Value& data);
			Json::Value HandleCommand(const Json::Value& data);
			Json::Value RelayQuery(const char* command, const Json::Value& args);

			int mNextQueryId = 1;
		};
	}
}
