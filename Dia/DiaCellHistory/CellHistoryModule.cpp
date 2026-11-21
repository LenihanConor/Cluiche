#include <DiaCellHistory/CellHistoryModule.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaCellLayers/MirrorQueries.h>
#include <DiaObservation/Log/DiaLog.h>
#include <json/json.h>

#ifdef DIA_DEBUG
#include <DiaCellHistory/CellHistoryCommands.h>
#endif

#include <chrono>

namespace Dia::CellHistory
{
	const Dia::Core::StringCRC CellHistoryModule::kTypeId("CellHistoryModule");

	CellHistoryModule::CellHistoryModule(const Dia::Core::StringCRC& instanceId)
		: Module(instanceId)
	{
	}

	CellHistoryModule::~CellHistoryModule() = default;

	bool CellHistoryModule::CommandsRegistered() const
	{
#ifdef DIA_DEBUG
		return mRegisteredWith != nullptr;
#else
		return false;
#endif
	}

	void CellHistoryModule::OnConfigure(const char* configJson)
	{
		mConfig = HistoryConfig();
		if (configJson == nullptr || configJson[0] == '\0') { return; }

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(configJson, root) || !root.isObject())
		{
			DIA_LOG_ERROR("HIS", "CellHistoryModule config is not a JSON object; using defaults");
			return;
		}
		const Json::Value& block = root["history"].isObject() ? root["history"] : root;
		HistoryConfig parsed;
		const HistoryConfigResult r = ParseHistoryConfig(block, parsed);
		if (!r.mOk)
		{
			DIA_LOG_ERROR("HIS", "CellHistoryModule config rejected (%s); using defaults", r.mError.AsCStr());
			return;
		}
		mConfig = parsed;
	}

	void CellHistoryModule::ConnectStreams(Dia::ApplicationFlow::IStreamConnector& connector)
	{
		mServiceWriter.Connect(connector);
		mRegistryReader.Connect(connector);
		mSchemaReader.Connect(connector);
	}

	void CellHistoryModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
	{
		ConnectStreams(app);
	}

	Dia::ApplicationFlow::StartResult CellHistoryModule::DoStart()
	{
		// Starting again without a Stop: withdraw the stream and the commands before replacing the recorder.
		UnregisterCommands();
		mServiceWriter.Deregister();
		mRecorder.Reset();
		mObservation.Reset();
		mStartSeed = mRunIdSeedSet
			? mRunIdSeed
			: static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
		DIA_LOG_INFO("HIS", "CellHistoryModule started; waiting for the cell layer registry");
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void CellHistoryModule::TryCreateRecorder()
	{
		if (mRecorder != nullptr) { return; }
		Dia::CellLayers::CellLayerRegistry* registry = mRegistryOverride;
		if (registry == nullptr && mRegistryReader.IsConnected() && mRegistryReader.IsAvailable()) { registry = &mRegistryReader.Get(); }
		if (registry == nullptr) { return; }

		const Dia::ValueSchema::ValueSchemaService* schema = mSchemaOverride;
		if (schema == nullptr && mSchemaReader.IsConnected() && mSchemaReader.IsAvailable()) { schema = &mSchemaReader.Get(); }
		if (schema == nullptr) { DIA_LOG_WARNING("HIS", "CellHistoryModule: no ValueSchemaService; history runs without schema data"); }

		mRecorder = Dia::Core::MakeUnique<CellHistoryRecorder>(*registry, schema, mConfig, mStartSeed);
		mObservation = Dia::Core::MakeUnique<CellHistoryObservation>(*mRecorder);
		if (mServiceWriter.IsConnected()) { mServiceWriter.Register(*mRecorder); }
		DIA_LOG_INFO("HIS", "CellHistoryRecorder published on stream '%s'", kServiceStreamId);
	}

	void CellHistoryModule::TryRegisterCommands()
	{
#ifdef DIA_DEBUG
		if (mRecorder == nullptr || mRegistrar == nullptr || mRegisteredWith != nullptr) { return; }
		if (mCommands == nullptr) { mCommands = Dia::Core::MakeUnique<CellHistoryCommands>(*mRecorder); }
		if (mCommands->Register(*mRegistrar)) { mRegisteredWith = mRegistrar; }
		else { DIA_LOG_ERROR("HIS", "CellHistoryModule: sim-hosted cells.history.* command registration failed"); }
#endif
	}

	void CellHistoryModule::UnregisterCommands()
	{
#ifdef DIA_DEBUG
		if (mCommands != nullptr && mRegisteredWith != nullptr) { mCommands->Unregister(*mRegisteredWith); }
		mRegisteredWith = nullptr;
		mCommands.Reset();	// handlers capture the recorder
#endif
	}

	void CellHistoryModule::DoUpdate(float /*deltaTime*/)
	{
		TryCreateRecorder();
		TryRegisterCommands();
		if (mObservation != nullptr) { mObservation->OnRecorderUpdate(); }
	}

	Dia::ApplicationFlow::StopResult CellHistoryModule::DoStop()
	{
		// The recorder stays alive until the next DoStart or destruction (see header).
		// The observation is torn down (unregisters health reporter).
		UnregisterCommands();
		mObservation.Reset();
		mServiceWriter.Deregister();
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using CellHistoryModule_ = Dia::CellHistory::CellHistoryModule; }
namespace { using CellHistoryRecorder = Dia::CellHistory::CellHistoryRecorder; }
DIA_MODULE(CellHistoryModule_);
DIA_DESCRIBE(CellHistoryModule_::kTypeId, "Owns the CellHistoryRecorder and publishes it on the CellHistoryRecorderService ServiceStream.");
DIA_STREAM_TYPE(CellHistoryRecorder);
