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
// contributor (IValueSchemaContributor) -> CodeSideCollector pass (metrics + commands, profile scope
// "ValueSchema.CollectCodeSide") -> Publish -> register the service on the stream.
// Config "code_side": false (default true) skips the code-side pass. The source is injectable
// (SetCodeSideSource) for tests; default is a module-owned LiveCodeSideSource.
//
// Late registrations (AC 6): metrics/commands registered after Publish are NOT in the (immutable) service.
// LateRegistrationCount() compares the source generations with those recorded at collection;
// CheckLateRegistrations() logs ONE Warning per changed count. It is the hook that valueschema.get
// answering and describe-mode dumps (Features 3 and 6) are to call; nothing calls it automatically.
//
// Lifetime: the module owns the registry and the service. DoStop deregisters the stream (consumers'
// IsAvailable() turns false) but keeps both objects alive; they are replaced on the next DoStart and
// freed with the module, so the service never outlives its registry.
//
// Observation: metrics valueschema.entries_loaded / valueschema.files_failed (gauges) and
// valueschema.ramps_loaded / valueschema.ramp_files_failed (gauges),
// valueschema.metrics_described / metrics_undescribed / commands_described / args_described (gauges, code-side),
// valueschema.component_fields_described / component_fields_undescribed / attr_registry_entries / attr_registry_dropped
// (gauges, Feature 7: ECS component fields via ComponentFieldCollector + ComponentAttributeAudit, profile scope
// "ValueSchema.CollectComponentFields"; dropped attribute registrations log an Error and degrade health),
// valueschema.clamp.count (counter, bound to GetClamper()); health reporter "dia.valueschema".
//
// Query (Feature 3): at the end of DoStart the module registers `valueschema.get` (ValueSchemaQueryHandler) with the
// DiaDebugServer QueryRegistry and unregisters it in DoStop / the destructor. The registry is taken from
// SetQueryRegistry() when set (tests), else from the DebugServer on the "DebugServerService" ServiceStream (the same
// way the inspector modules reach it). Without a DebugServer the query is simply not served (Debug log, not an error).
//
// Describe mode (Feature 6 task 4): when the app's Main began a DescribeSession (`--describe-schema <path> [--stage <name>]`),
// DoUpdate polls an ApplicationDescribeBarrier (all modules past Init, optional stage entered), lets DescribeModeRunner write
// the export from this module's service + code-side/component reports, records the exit code in DescribeSession and calls
// RequestShutdown(). Sim-hosted command names reach Main after DoStart, so once the barrier is ready the module waits for the
// code-side command generation to hold still for 1 s and, if it moved since DoStart, exports a freshly built snapshot (the
// published service and stream are untouched). Gives up with exit code kExitTimedOut after 120 s of waiting. See DescribeMode.h for the design note.

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
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ComponentFieldCollector.h>
#include <DiaValueSchema/ComponentAttributeAudit.h>
#include <DiaValueSchema/ValueSchemaQuery.h>
#include <DiaValueSchema/DescribeMode.h>

namespace Dia::Observation::Metric { class Counter; class Gauge; }
namespace Dia::Entity { struct ComponentTypeDesc; }
namespace Dia::DebugServer { class QueryRegistry; class ISimHostedCatalogue; }
namespace Dia::ApplicationFlow { class IApplicationInspectable; }

namespace Dia::ValueSchema
{
	class IValueSchemaContributor;

	class ValueSchemaModule : public Dia::ApplicationFlow::Module, private IDescribeSnapshotProvider
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

		// Code-side collection (Feature 4). Set before start; null restores the live source. Not owned.
		void					SetCodeSideSource(ICodeSideSource* source)	{ mCodeSideSource = source; }
		const CodeSideReport&	GetCodeSideReport() const					{ return mCodeSideReport; }
		// Metrics + commands registered since the last collection (0 when code_side is off or never collected).
		// Sim-hosted command help (Sim-Hosted Command Help, Task 4). Injects a read-only catalogue into the live source;
		// not owned. Default (never called): the module resolves the DebugServer from the "DebugServerService" stream once
		// that stream is committed (first DoUpdate) -- but only when this module runs on the Main PU, because the
		// catalogue is Main-owned. Names registered afterwards count as late registrations (CheckLateRegistrations).
		void					SetSimHostedCatalogue(const Dia::DebugServer::ISimHostedCatalogue* catalogue)
		{ mLiveSource.SetSimHostedCatalogue(catalogue); mSimCatalogueResolved = true; }
		unsigned int			LateRegistrationCount();
		// Logs one Warning when LateRegistrationCount() > 0 and differs from the last warned count.
		void					CheckLateRegistrations();
		unsigned int			LateWarningCount() const					{ return mLateWarningCount; }

		// Component fields (Feature 7), collected in the same pass/flag as the code-side entries. Default source is the
		// global ComponentRegistry + FieldAttributeRegistry::Instance(); both are injectable for tests (set before start,
		// pointers not owned and must outlive the module's start).
		void SetComponentDescriptors(const Dia::Entity::ComponentTypeDesc* const* descs, unsigned int count)
		{ mComponentDescs = descs; mComponentDescCount = count; mComponentOverride = true; }
		void SetAttributeRegistry(const Dia::Reflect::FieldAttributeRegistry* registry)	{ mAttrRegistry = registry; }
		const ComponentFieldReport&			GetComponentFieldReport() const	{ return mComponentReport; }
		const ComponentAttributeAuditReport& GetComponentAuditReport() const	{ return mAuditReport; }

		// Query registry for valueschema.get (Feature 3). Set before start; null (default) resolves the DebugServer from the
		// "DebugServerService" stream at start. Not owned; must outlive the module's start.
		void SetQueryRegistry(Dia::DebugServer::QueryRegistry* registry)	{ mQueryRegistryOverride = registry; }

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
		// Everything the load pipeline produces; DoStart installs one into the members, describe mode keeps a second one.
		struct Snapshot
		{
			Dia::Core::UniquePtr<ValueSchemaRegistry>	mRegistry;
			Dia::Core::UniquePtr<RampRegistry>			mRamps;
			Dia::Core::UniquePtr<ValueSchemaService>	mService;
			SchemaLoadStats								mStats;
			RampLoadStats								mRampStats;
			CodeSideReport								mCodeSideReport;
			bool										mCodeSideCollected = false;
			ComponentFieldReport						mComponentReport;
			ComponentAttributeAuditReport				mAuditReport;
		};
		void BuildSnapshot(Snapshot& snap);
		// IDescribeSnapshotProvider: waits for the sim-hosted command set to stop changing, then rebuilds the snapshot once if
		// names arrived after DoStart (Sim-Hosted Command Help task 4b). The published service is never touched.
		bool Acquire(DescribeView& out) override;
		Snapshot									mDescribeSnapshot;
		bool										mDescribeRebuilt = false;
		bool										mDescribeGenSeen = false;
		unsigned int								mDescribeLastGen = 0;
		double										mDescribeGenChangedAtSec = 0.0;

		void RegisterQuery();
		void UnregisterQuery();
		void ResolveSimCatalogue();
		bool mSimCatalogueResolved = false;
		void TickDescribeMode(float deltaTime);

		// Describe mode (Feature 6 task 4): created lazily on the first DoUpdate when DescribeSession is active.
		Dia::Core::UniquePtr<DescribeModeRunner>	mDescribeRunner;
		Dia::Core::UniquePtr<IDescribeBarrier>		mDescribeBarrier;
		float										mDescribeWaitedSec = 0.0f;
		Dia::ApplicationFlow::IApplicationInspectable*	mAppInspect = nullptr;

		Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, kMaxSchemaRoots>	mRootAliases;
		Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, kMaxSchemaRoots>	mRampRootAliases;
		Dia::Core::Containers::DynamicArrayC<IValueSchemaContributor*, kMaxContributors>			mContributors;

		Dia::Core::UniquePtr<ValueSchemaRegistry>	mRegistry;
		Dia::Core::UniquePtr<RampRegistry>			mRamps;
		Dia::Core::UniquePtr<ValueSchemaService>	mService;	// references *mRegistry and *mRamps
		ValueClamper								mClamper;
		Dia::Core::UniquePtr<ValueSchemaQueryHandler>	mQueryHandler;	// references *mService
		Dia::DebugServer::QueryRegistry*			mQueryRegistryOverride = nullptr;
		Dia::DebugServer::QueryRegistry*			mQueryRegisteredOn = nullptr;
		LiveCodeSideSource							mLiveSource;
		ICodeSideSource*							mCodeSideSource = nullptr;	// null = mLiveSource
		CodeSideReport								mCodeSideReport;
		ComponentFieldReport						mComponentReport;
		ComponentAttributeAuditReport				mAuditReport;
		ComponentAttributeAudit						mAudit;	// persists across starts: dedupes warnings
		const Dia::Entity::ComponentTypeDesc* const*	mComponentDescs = nullptr;
		unsigned int								mComponentDescCount = 0;
		bool										mComponentOverride = false;
		const Dia::Reflect::FieldAttributeRegistry*	mAttrRegistry = nullptr;	// null = FieldAttributeRegistry::Instance()
		bool										mCodeSideEnabled = true;
		bool										mCodeSideCollected = false;
		unsigned int								mLastWarnedLate = 0;
		unsigned int								mLateWarningCount = 0;
		SchemaLoadStats								mStats;
		RampLoadStats								mRampStats;
		ValueSchemaHealth							mHealth{ mStats, &mRampStats, &mAuditReport };

		Dia::ApplicationFlow::ServiceStreamWriter<ValueSchemaService> mServiceWriter{ this, Dia::Core::StringCRC(kServiceStreamId) };

		Dia::Observation::Metric::Gauge*	mMetricEntriesLoaded = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricFilesFailed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricRampsLoaded = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricRampFilesFailed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricMetricsDescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricMetricsUndescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricCommandsDescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricArgsDescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricComponentDescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricComponentUndescribed = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricAttrEntries = nullptr;
		Dia::Observation::Metric::Gauge*	mMetricAttrDropped = nullptr;
		Dia::Observation::Metric::Counter*	mMetricClampCount = nullptr;
	};
}
