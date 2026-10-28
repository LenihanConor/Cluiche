#pragma once

// CellLayersModule: owns the CellLayerRegistry and publishes it on the "CellLayerRegistryService" ServiceStream
// (CL-006: no singletons; consumers reach the registry through the stream).
//
// Base / affinity: derives from Module directly with PUAffinity::kAny. Do no per-frame work (DoUpdate is a no-op).
//
// Owner PU: the registry is constructed in DoStart, which runs on the module's processing-unit thread, so that
// PU is the registry's owner: Register/Unregister must be called from it (CellLayerRegistry rejects other PUs).
// Place this module on the same PU as the systems that register layers (normally SimPU). SetCurrentPuIdFn injects
// the registry's PU-id seam (tests); SetSchema supplies the optional ValueSchemaService. Both must be set before start.
//
// Lifetime: DoStart builds a fresh registry and registers it on the stream; DoStop deregisters the stream
// (consumers' IsAvailable() turns false) and keeps the registry alive until the next DoStart or destruction.
// Registrations do not survive a restart.
//
// Consumers: hold a ServiceStreamReader<CellLayerRegistry> on kServiceStreamId, Connect it in OnConnectStreams, and
// call FindCellLayerRegistry(reader) from DoStart or later (null when the stream is not available).

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaStreams/ServiceStreamWriter.h>

namespace Dia::CellLayers
{
	class CellLayersModule : public Dia::ApplicationFlow::Module
	{
	public:
		static const Dia::Core::StringCRC kTypeId;
		static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kAny;
		static constexpr const char* kDescription = "Owns the CellLayerRegistry and publishes it on a ServiceStream";
		static constexpr const char* kServiceStreamId = "CellLayerRegistryService";

		explicit CellLayersModule(const Dia::Core::StringCRC& instanceId);
		~CellLayersModule() override;

		// Set before start. Not owned; must outlive the module's start.
		void SetSchema(const Dia::ValueSchema::ValueSchemaService* schema)	{ mSchema = schema; }
		void SetCurrentPuIdFn(CellLayerRegistry::CurrentPuIdFn fn)			{ mCurrentPuId = fn; }

		// Null until the first DoStart has published.
		CellLayerRegistry*			GetRegistry()		{ return mRegistry.Get(); }
		const CellLayerRegistry*	GetRegistry() const	{ return mRegistry.Get(); }

	protected:
		void								OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult	DoStart() override;
		void								DoUpdate(float deltaTime) override;
		Dia::ApplicationFlow::StopResult	DoStop() override;

	private:
		Dia::Core::UniquePtr<CellLayerRegistry>			mRegistry;
		const Dia::ValueSchema::ValueSchemaService*		mSchema = nullptr;
		CellLayerRegistry::CurrentPuIdFn				mCurrentPuId = nullptr;

		Dia::ApplicationFlow::ServiceStreamWriter<CellLayerRegistry> mServiceWriter{ this, Dia::Core::StringCRC(kServiceStreamId) };
	};

	// The registry published by a CellLayersModule, or null when the stream is unavailable (not connected, not yet
	// committed, or withdrawn). Pass the result to CellLayerViewerDomain's constructor.
	inline CellLayerRegistry* FindCellLayerRegistry(const Dia::ApplicationFlow::ServiceStreamReader<CellLayerRegistry>& reader)
	{
		return reader.IsAvailable() ? &reader.Get() : nullptr;
	}
}
