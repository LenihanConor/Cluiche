#include <DiaCellLayers/CellLayersModule.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::CellLayers
{
	const Dia::Core::StringCRC CellLayersModule::kTypeId("CellLayersModule");

	CellLayersModule::CellLayersModule(const Dia::Core::StringCRC& instanceId)
		: Module(instanceId)
	{
	}

	CellLayersModule::~CellLayersModule() {}

	void CellLayersModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
	{
		mServiceWriter.Connect(app);
		mSchemaReader.Connect(app);
	}

	void CellLayersModule::TryAttachSchemaStream()
	{
		if (mSchemaAttached || mRegistry == nullptr || mSchema != nullptr || !mSchemaReader.IsAvailable()) { return; }
		mSchema = &mSchemaReader.Get();
		mRegistry->SetSchema(mSchema);
		mSchemaAttached = true;
		DIA_LOG_INFO("CEL", "CellLayerRegistry attached to stream '%s'", kSchemaStreamId);
	}

	Dia::ApplicationFlow::StartResult CellLayersModule::DoStart()
	{
		// Starting again without a Stop: withdraw the stream before replacing the registry it points at.
		mServiceWriter.Deregister();
		// Constructed here so the registry's owner PU is the PU thread running this module.
		if (mSchemaAttached) { mSchema = nullptr; mSchemaAttached = false; }	// a restart re-resolves the stream
		mRegistry = Dia::Core::MakeUnique<CellLayerRegistry>(mSchema, mCurrentPuId);
		mServiceWriter.Register(*mRegistry);
		TryAttachSchemaStream();
		DIA_LOG_INFO("CEL", "CellLayerRegistry published on stream '%s'", kServiceStreamId);
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void CellLayersModule::DoUpdate(float /*deltaTime*/)
	{
		TryAttachSchemaStream();
	}

	Dia::ApplicationFlow::StopResult CellLayersModule::DoStop()
	{
		// The registry stays alive until the next DoStart or destruction (see header).
		mServiceWriter.Deregister();
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using CellLayersModule_ = Dia::CellLayers::CellLayersModule; }
namespace { using CellLayerRegistry = Dia::CellLayers::CellLayerRegistry; }
DIA_MODULE(CellLayersModule_);
DIA_DESCRIBE(CellLayersModule_::kTypeId, "Owns the CellLayerRegistry and publishes it on the CellLayerRegistryService ServiceStream.");
DIA_STREAM_TYPE(CellLayerRegistry);
