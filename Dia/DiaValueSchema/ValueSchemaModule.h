#pragma once

// ValueSchemaModule: loads every *.schema.json under the configured schema roots at start and
// publishes the finished registry on the "ValueSchemaService" ServiceStream (AD-004, VS-005).
//
// Base / affinity: derives from Module directly with PUAffinity::kAny (same as ObservationModule),
// so a manifest may place it on whichever PU needs it. It does no per-frame work (DoUpdate is a
// no-op); consumers on any PU read the immutable published registry through the stream, lock-free.
// Config is delivered through Module::OnConfigure (available on every module base):
//
//     "config": { "schema_roots": ["data", "cow_schema"], "ramp_roots": ["dia_data"] }
//
// Each entry is a PathStore alias resolved to a directory and walked recursively. An unregistered
// alias or missing directory is a Warning, not fatal (counted in GetLoadStats().mRootsMissing).
// Config JSON is capped at 255 chars (String256): keep alias lists short.
//
// Ramps (Feature 5): each `ramp_roots` alias is a directory holding the Dia ramp set as a file named exactly
// "ramps.json" (apps add a `dia_data` alias pointing at Dia/DiaValueSchema/Data). Game ramps are every
// "*.ramps.json" under the schema_roots directories. Dia ramps load first and win id clashes. A whole ramp file
// failing degrades health; a single skipped ramp does not. FindRamp is served by ValueSchemaService.
//
// Start (DoStart): trace zone "ValueSchema.Load" -> discover + load sorted files -> call every
// contributor (IValueSchemaContributor) -> Publish -> register the service on the stream.
//
// Lifetime: the module owns the registry and the service. DoStop deregisters the stream (consumers'
// IsAvailable() turns false) but keeps both objects alive; they are replaced on the next DoStart and
// freed with the module, so the service never outlives its registry.
//
// Observation: metrics valueschema.entries_loaded / valueschema.files_failed (gauges) and
// valueschema.ramps_loaded / valueschema.ramp_files_failed (gauges),
// valueschema.clamp.count (counter, bound to GetClamper()); health reporter "dia.valueschema".

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/ServiceStreamWriter.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Memory/UniquePtr.h>
#include <DiaCore/Strings/String64.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueClamper.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/ValueSchemaHealth.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/RampSetLoader.h>

namespace Dia::Observation::Metric { class Counter; class Gauge; }

namespace Dia::ValueSchema
{
	class IValueSchemaContributor;

	class ValueSchemaModule : public Dia::ApplicationFlow::Module
	{
	public:
		static const Dia::Core::StringCRC kTypeId;
		static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kAny;
		static constexpr const char* kDescription = "Loads *.schema.json sidecars from schema_roots and publishes ValueSchemaService";
		static constexpr const char* kServiceStreamId = "ValueSchemaService";
		static constexpr unsigned int kMaxSchemaRoots = 16;
		static constexpr unsigned int kMaxContributors = 16;

		explicit ValueSchemaModule(const Dia::Core::StringCRC& instanceId);
		~ValueSchemaModule() override;

		// Contributor seam (Feature 4 / 7). Call before the module starts; returns false when full or null.
		bool AddContributor(IValueSchemaContributor* contributor);

		// Null until the first DoStart has published.
		const ValueSchemaService*	GetService() const		{ return mService.Get(); }
		ValueClamper&				GetClamper()			{ return mClamper; }
		const SchemaLoadStats&		GetLoadStats() const	{ return mStats; }
		const ValueSchemaHealth&	GetHealthReporter() const { return mHealth; }
		const RampLoadStats&		GetRampLoadStats() const { return mRampStats; }

		unsigned int	GetSchemaRootCount() const						{ return mRootAliases.Size(); }
		const char*		GetSchemaRoot(unsigned int index) const			{ return mRootAliases[index].AsCStr(); }
		unsigned int	GetRampRootCount() const						{ return mRampRootAliases.Size(); }
		const char*		GetRampRoot(unsigned int index) const			{ return mRampRootAliases[index].AsCStr(); }

	protected:
		void								OnConfigure(const char* configJson) override;
		void								OnConnectStreams(Dia::ApplicationFlow::Application& app) override;
		Dia::ApplicationFlow::StartResult	DoStart() override;
		void								DoUpdate(float deltaTime) override;
		Dia::ApplicationFlow::StopResult	DoStop() override;

	private:
		Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, kMaxSchemaRoots>	mRootAliases;
		Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, kMaxSchemaRoots>	mRampRootAliases;
		Dia::Core::Containers::DynamicArrayC<IValueSchemaContributor*, kMaxContributors>			mContributors;

		Dia::Core::UniquePtr<ValueSchemaRegistry>	mRegistry;
		Dia::Core::UniquePtr<RampRegistry>			mRamps;
		Dia::Core::UniquePtr<ValueSchemaService>	mService;	// references *mRegistry and *mRamps
		ValueClamper								mClamper;
		SchemaLoadStats								mStats;
		RampLoadStats								mRampStats;
		ValueSchemaHealth							mHealth{ mStats, &mRampStats };

		Dia::ApplicationFlow::ServiceStreamWriter<ValueSchemaService> mServiceWriter{ this, Dia::Core::StringCRC(kServiceStreamId) };

		Dia::Observation::Metric::Gauge*	mMetricEntriesLoaded = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricFilesFailed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricRampsLoaded = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricRampFilesFailed = nullptr;
		Dia::Observation::Metric::Counter*	mMetricClampCount = nullptr;
	};
}
