#pragma once

// CellHistoryModule: owns the CellHistoryRecorder and publishes it on the "CellHistoryRecorderService" ServiceStream
// (CH-005: no singletons; consumers reach the recorder through the stream). Follows the CellLayersModule pattern.
//
// Affinity: PUAffinity::kSim. The recorder is constructed on the module's PU and must be driven (Sample/Emit/read) on the
// same PU as the CellLayerRegistry's owner and the layer providers, which is SimPU. Declare this module on SimPU, in
// the same stage as CellLayersModule.
//
// Lifecycle: DoStart withdraws any previous stream, drops the previous recorder and resets the lazy state. The registry
// and schema streams only commit after every provider's DoStart, so DoUpdate resolves them lazily; on the first update
// that sees the registry it creates the recorder, publishes it and (DIA_DEBUG, when a registrar was supplied) registers
// the cells.history.* commands. The recorder does not exist, and the stream is not published, until then.
// DoStop unregisters the commands and withdraws the stream; the recorder stays alive until the next DoStart or destruction.
// The module never calls Sample: the game drives sampling.
//
// Config: the module's manifest `config` string (OnConfigure, <= 255 chars), a JSON object that is either the `history`
// block itself or an object holding it under "history" (keys as HistoryConfig: bands, track, grid, cellBudgetMB,
// aggregateBudgetMB, eventBudgetMB, maxAggregateLayers; all optional). Malformed or invalid config logs an Error
// ("HIS") and the defaults are used; it never crashes.
//
// RunId: the recorder's runIdSeed is the wall-clock time at module start (system_clock ticks), captured in DoStart.
// SetRunIdSeed overrides it (tests, deterministic runs); set before start.
//
// Registrar: DiaCellHistory cannot see the application-layer SimQueryHostModule, so the game supplies an
// IMirrorQueryRegistrar adapter over it via SetRegistrar (the same adapter the DiaCellLayers viewer commands use) from a
// module on SimPU. Not owned; must outlive DoStop. Commands register once, on the first DoUpdate where both the
// recorder and the registrar exist, and are unregistered in DoStop.
//
// Consumers: hold a ServiceStreamReader<CellHistoryRecorder> on kServiceStreamId, Connect it in OnConnectStreams, and
// call FindCellHistoryRecorder(reader) once DoStart has run and the store has committed (null when unavailable).
//
// Manifest: the stream is declared with this module as provider and needs a consumer (orphan-provider rule); the module
// also needs `consumes` channels on "CellLayerRegistryService" and "ValueSchemaService".

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/CellHistoryObservation.h>
#include <DiaCellHistory/HistoryConfig.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaStreams/IStreamConnector.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaValueSchema/ValueSchemaService.h>

namespace Dia::CellLayers { class IMirrorQueryRegistrar; }

namespace Dia::CellHistory
{
#ifdef DIA_DEBUG
	class CellHistoryCommands;
#endif

	class CellHistoryModule : public Dia::ApplicationFlow::Module
	{
	public:
		static const Dia::Core::StringCRC kTypeId;
		static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
		static constexpr const char* kDescription = "Owns the CellHistoryRecorder and publishes it on a ServiceStream";
		static constexpr const char* kServiceStreamId = "CellHistoryRecorderService";
		static constexpr const char* kRegistryStreamId = "CellLayerRegistryService";
		static constexpr const char* kSchemaStreamId = "ValueSchemaService";

		explicit CellHistoryModule(const Dia::Core::StringCRC& instanceId);
		~CellHistoryModule() override;

		// Explicit overrides, set before start. Not owned; must outlive the module. They take precedence over the streams.
		void SetRegistry(Dia::CellLayers::CellLayerRegistry* registry)			{ mRegistryOverride = registry; }
		void SetSchema(const Dia::ValueSchema::ValueSchemaService* schema)		{ mSchemaOverride = schema; }
		void SetRunIdSeed(uint64_t seed)										{ mRunIdSeed = seed; mRunIdSeedSet = true; }
		void SetRegistrar(Dia::CellLayers::IMirrorQueryRegistrar* registrar)	{ mRegistrar = registrar; }

		// Connects the module's stream handles. OnConnectStreams forwards the Application here; tests pass a connector.
		void ConnectStreams(Dia::ApplicationFlow::IStreamConnector& connector);

		// Null until the recorder has been created.
		CellHistoryRecorder*		GetRecorder()		{ return mRecorder.Get(); }
		const CellHistoryRecorder*	GetRecorder() const	{ return mRecorder.Get(); }
		const HistoryConfig&		GetConfig() const	{ return mConfig; }
		bool						CommandsRegistered() const;

	protected:
		void								OnConfigure(const char* configJson) override;
		void								OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult	DoStart() override;
		void								DoUpdate(float deltaTime) override;
		Dia::ApplicationFlow::StopResult	DoStop() override;

	private:
		void TryCreateRecorder();
		void TryRegisterCommands();
		void UnregisterCommands();

		HistoryConfig		mConfig;
		uint64_t			mRunIdSeed = 0;
		bool				mRunIdSeedSet = false;
		uint64_t			mStartSeed = 0;

		Dia::CellLayers::CellLayerRegistry*			mRegistryOverride = nullptr;
		const Dia::ValueSchema::ValueSchemaService*	mSchemaOverride = nullptr;
		Dia::CellLayers::IMirrorQueryRegistrar*		mRegistrar = nullptr;

		Dia::ApplicationFlow::ServiceStreamReader<Dia::CellLayers::CellLayerRegistry> mRegistryReader{ this, Dia::Core::StringCRC(kRegistryStreamId) };
		Dia::ApplicationFlow::ServiceStreamReader<Dia::ValueSchema::ValueSchemaService> mSchemaReader{ this, Dia::Core::StringCRC(kSchemaStreamId) };
		Dia::ApplicationFlow::ServiceStreamWriter<CellHistoryRecorder> mServiceWriter{ this, Dia::Core::StringCRC(kServiceStreamId) };

		Dia::Core::UniquePtr<CellHistoryRecorder>	mRecorder;
		Dia::Core::UniquePtr<CellHistoryObservation>	mObservation;
#ifdef DIA_DEBUG
		Dia::Core::UniquePtr<CellHistoryCommands>	mCommands;
		Dia::CellLayers::IMirrorQueryRegistrar*		mRegisteredWith = nullptr;
#endif
	};

	// The recorder published by a CellHistoryModule, or null when the stream is unavailable (not connected, not yet
	// committed, or withdrawn).
	inline CellHistoryRecorder* FindCellHistoryRecorder(const Dia::ApplicationFlow::ServiceStreamReader<CellHistoryRecorder>& reader)
	{
		return reader.IsAvailable() ? &reader.Get() : nullptr;
	}
}
