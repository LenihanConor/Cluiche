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
	}

	Dia::ApplicationFlow::StartResult CellLayersModule::DoStart()
	{
		// Starting again without a Stop: withdraw the stream before replacing the registry it points at.
		mServiceWriter.Deregister();
		// Constructed here so the registry's owner PU is the PU thread running this module.
		mRegistry = Dia::Core::MakeUnique<CellLayerRegistry>(mSchema, mCurrentPuId);
		mServiceWriter.Register(*mRegistry);
		DIA_LOG_INFO("CEL", "CellLayerRegistry published on stream '%s'", kServiceStreamId);
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void CellLayersModule::DoUpdate(float /*deltaTime*/)
	{
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
