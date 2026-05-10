////////////////////////////////////////////////////////////////////////////////
// Filename: IDebugStateProvider.h
//
// Read-only interface DebugServer uses to describe the host application's
// runtime state to connected clients.  Owned by DiaDebugServer so the
// library depends on nothing above DiaCore.  An adapter in the host
// application (e.g. CluicheGameBaseline::DebugServerHostModule) implements
// this by translating from whatever lifecycle framework that app uses.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace DebugServer
	{
		// Snapshot of a single module's identity and lifecycle state.
		// `state` is a stable C-string ("inactive", "starting", "active",
		// "stopping", "failed") — the provider translates from whatever
		// enum its lifecycle framework uses, so DiaDebugServer never sees it.
		struct DebugModuleInfo
		{
			Dia::Core::StringCRC instanceId;
			Dia::Core::StringCRC typeId;
			const char*          state;
		};

		class IDebugStateProvider
		{
		public:
			virtual ~IDebugStateProvider() = default;

			// Current high-level stage/phase identifier.
			virtual Dia::Core::StringCRC GetCurrentStage() const = 0;

			// True while a stage transition is in progress (outgoing modules
			// stopping or incoming modules starting).
			virtual bool IsTransitioning() const = 0;

			// True once application shutdown has been requested.
			virtual bool IsShuttingDown() const = 0;

			// Fill `out` with the IDs of every processing unit.  Stable order.
			virtual void GetProcessingUnitIds(
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4>& out) const = 0;

			// Fill `out` with a snapshot of every module in the given PU.
			// No-op if `puId` is not recognised.
			virtual void GetModulesInPU(
				const Dia::Core::StringCRC& puId,
				Dia::Core::Containers::DynamicArrayC<DebugModuleInfo, 64>& out) const = 0;
		};
	}
}
