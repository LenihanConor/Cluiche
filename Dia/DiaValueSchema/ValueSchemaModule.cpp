#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/IValueSchemaContributor.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaDebugServer/DebugServer.h>
#include <DiaDebugServer/ISimHostedCatalogue.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Health/HealthRegistry.h>

#include <json/json.h>

#include <chrono>

namespace Dia::ValueSchema
{
	const Dia::Core::StringCRC ValueSchemaModule::kTypeId("ValueSchemaModule");

	ValueSchemaModule::ValueSchemaModule(const Dia::Core::StringCRC& instanceId)
		: Module(instanceId)
	{
	}

	ValueSchemaModule::~ValueSchemaModule()
	{
		// Destroyed without Stop: the global registry must not keep a pointer to the member mHealth (no-op if absent).
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
		mClamper.BindClampCounter(nullptr);
		UnregisterQuery();
	}

	bool ValueSchemaModule::AddContributor(IValueSchemaContributor* contributor)
	{
		if (contributor == nullptr || mContributors.IsFull()) { return false; }
		mContributors.Add(contributor);
		return true;
	}

	unsigned int ValueSchemaModule::LateRegistrationCount()
	{
		if (!mCodeSideEnabled || !mCodeSideCollected) { return 0; }
		ICodeSideSource& source = mCodeSideSource ? *mCodeSideSource : static_cast<ICodeSideSource&>(mLiveSource);
		// A generation can go backwards (injected source / registry reset): that is "no late registrations", not a wrap.
		const unsigned int metricGen = source.MetricGeneration();
		const unsigned int commandGen = source.CommandGeneration();
		const unsigned int lateMetrics = metricGen > mCodeSideReport.mMetricGeneration ? metricGen - mCodeSideReport.mMetricGeneration : 0u;
		const unsigned int lateCommands = commandGen > mCodeSideReport.mCommandGeneration ? commandGen - mCodeSideReport.mCommandGeneration : 0u;
		return lateMetrics + lateCommands;
	}

	void ValueSchemaModule::CheckLateRegistrations()
	{
		const unsigned int late = LateRegistrationCount();
		if (late == 0 || late == mLastWarnedLate) { return; }
		mLastWarnedLate = late;
		++mLateWarningCount;
		DIA_LOG_WARNING("VSC", "%u metrics/commands were registered after the schema was published and are not in the live schema", late);
	}

	// --- Config ---------------------------------------------------------------
	// { "schema_roots": ["alias", ...], "ramp_roots": ["alias", ...] } -- PathStore aliases, resolved at start.
	// (Module config JSON is capped at 255 chars.)
	namespace
	{
		using AliasList = Dia::Core::Containers::DynamicArrayC<Dia::Core::Containers::String64, ValueSchemaModule::kMaxSchemaRoots>;

		void ReadAliases(const Json::Value& root, const char* key, bool warnIfMissing, AliasList& out)
		{
			const Json::Value& roots = root[key];
			if (!roots.isArray())
			{
				if (warnIfMissing) { DIA_LOG_WARNING("VSC", "ValueSchemaModule config has no '%s' array; none configured", key); }
				return;
			}
			for (Json::ArrayIndex i = 0; i < roots.size(); ++i)
			{
				if (!roots[i].isString() || roots[i].asString().empty())
				{
					DIA_LOG_WARNING("VSC", "%s[%u] is not a non-empty string; ignored", key, static_cast<unsigned int>(i));
					continue;
				}
				const std::string alias = roots[i].asString();
				if (alias.size() > 63 || out.IsFull())	// String64 holds 63 chars
				{
					DIA_LOG_WARNING("VSC", "%s entry '%s' ignored (too long or more than %u roots)", key, alias.c_str(), ValueSchemaModule::kMaxSchemaRoots);
					continue;
				}
				Dia::Core::Containers::String64 name;
				SetText(name, alias.c_str());	// verbatim: `name = cstr` would printf-format a '%' in the alias
				out.Add(name);
			}
		}
	}

	void ValueSchemaModule::OnConfigure(const char* configJson)
	{
		mRootAliases.RemoveAll();
		mRampRootAliases.RemoveAll();
		mCodeSideEnabled = true;
		if (!configJson || configJson[0] == '\0') { return; }

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(configJson, root) || !root.isObject())
		{
			DIA_LOG_WARNING("VSC", "ValueSchemaModule config is not a JSON object; no schema roots");
			return;
		}
		ReadAliases(root, "schema_roots", true, mRootAliases);
		ReadAliases(root, "ramp_roots", false, mRampRootAliases);
		if (root["code_side"].isBool()) { mCodeSideEnabled = root["code_side"].asBool(); }
	}

	void ValueSchemaModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
	{
		mServiceWriter.Connect(app);
		mAppInspect = app.GetInspectable();	// no RTTI (/GR-): taken here rather than dynamic_cast from IApplicationControl
	}

	// --- valueschema.get registration -------------------------------------------
	namespace
	{
		const char* const kDebugServerStreamId = "DebugServerService";	// DebugServerHostModule::kServiceStreamId (application layer)

		Dia::DebugServer::DebugServer* ResolveDebugServer(Dia::ApplicationFlow::IApplicationInspectable* app)
		{
			if (app == nullptr) { return nullptr; }
			auto* store = app->FindStream(Dia::Core::StringCRC(kDebugServerStreamId));
			// The store's Get() asserts until the framework commits it (after every provider's DoStart), so a lookup made during our
			// own DoStart must not touch it: DoUpdate retries once the DebugServer stream is committed.
			if (store == nullptr || !store->IsCommitted()) { return nullptr; }
			// No RTTI (/GR-): dynamic_cast on these polymorphic types is undefined (crashed at start). The stream id fixes the payload
			// type, exactly as ServiceStreamReader/Writer do with their static_cast.
			auto* typed = static_cast<Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
			return typed ? typed->Get() : nullptr;
		}

		Dia::DebugServer::QueryRegistry* ResolveQueryRegistry(Dia::ApplicationFlow::IApplicationInspectable* app)
		{
			Dia::DebugServer::DebugServer* server = ResolveDebugServer(app);
			return server ? &server->GetQueryRegistry() : nullptr;
		}
	}

	void ValueSchemaModule::ResolveSimCatalogue()
	{
		if (mSimCatalogueResolved) { return; }
		Dia::DebugServer::DebugServer* server = ResolveDebugServer(mAppInspect);
		if (server == nullptr) { return; }	// stream not committed yet (or no DebugServer): retry next update
		mSimCatalogueResolved = true;
		// The catalogue is Main-owned (not thread-safe): never read it from another processing unit.
		const Dia::ApplicationFlow::ProcessingUnit* pu = GetProcessingUnit();
		if (pu != nullptr && pu->GetAffinity() != Dia::ApplicationFlow::PUAffinity::kMain)
		{
			DIA_LOG_WARNING("VSC", "ValueSchemaModule is not on the Main PU; sim-hosted commands are not collected");
			return;
		}
		mLiveSource.SetSimHostedCatalogue(server);
	}

	void ValueSchemaModule::UnregisterQuery()
	{
		if (mQueryRegisteredOn != nullptr)
		{
			mQueryRegisteredOn->Unregister(Dia::Core::StringCRC(ValueSchemaQueryHandler::kQueryName));
			mQueryRegisteredOn = nullptr;
		}
		mQueryHandler.Reset();
	}

	void ValueSchemaModule::RegisterQuery()
	{
		UnregisterQuery();
		Dia::DebugServer::QueryRegistry* registry = mQueryRegistryOverride ? mQueryRegistryOverride : ResolveQueryRegistry(mAppInspect);
		if (registry == nullptr)
		{
			DIA_LOG_DEBUG("VSC", "No DebugServer available; valueschema.get is not served");
			return;
		}
		mQueryHandler = Dia::Core::MakeUnique<ValueSchemaQueryHandler>(*mService);
		ValueSchemaQueryHandler* handler = mQueryHandler.Get();
		registry->Register(Dia::Core::StringCRC(ValueSchemaQueryHandler::kQueryName),
			[handler](const Json::Value& args) { return handler->Handle(args); });
		mQueryRegisteredOn = registry;
	}

	// --- Lifecycle ------------------------------------------------------------
	Dia::ApplicationFlow::StartResult ValueSchemaModule::DoStart()
	{
		{
			DIA_TRACE_ZONE("ValueSchema.Load", Dia::Observation::Trace::Category::kDiaAssetRuntime);

			// Starting again without a Stop: take the stream down first so consumers never see (and the stream never
			// re-registers over) a service that is about to be destroyed. No-op when not registered.
			mServiceWriter.Deregister();
			UnregisterQuery();	// the handler references the service that is about to be replaced

			// Fresh registry each start: a published registry is immutable.
			mService.Reset();
			mRegistry = Dia::Core::MakeUnique<ValueSchemaRegistry>();
			mRamps = Dia::Core::MakeUnique<RampRegistry>();

			SchemaDirectoryLoader loader;
			RampSetLoader rampLoader;
			unsigned int unresolved = 0;
			for (unsigned int i = 0; i < mRootAliases.Size(); ++i)
			{
				const Dia::Core::Path::Alias alias(mRootAliases[i].AsCStr());
				if (!Dia::Core::PathStore::IsPathAliasRegistered(alias))
				{
					DIA_LOG_WARNING("VSC", "Schema root alias '%s' is not registered; skipped", mRootAliases[i].AsCStr());
					++unresolved;
					continue;
				}
				const char* dir = Dia::Core::PathStore::ResolvePathToCString(alias);
				loader.AddRoot(dir);
				rampLoader.AddGameRoot(dir);
			}

			for (unsigned int i = 0; i < mRampRootAliases.Size(); ++i)
			{
				const Dia::Core::Path::Alias alias(mRampRootAliases[i].AsCStr());
				if (!Dia::Core::PathStore::IsPathAliasRegistered(alias))
				{
					DIA_LOG_WARNING("VSC", "Ramp root alias '%s' is not registered; skipped", mRampRootAliases[i].AsCStr());
					continue;
				}
				rampLoader.AddDiaRoot(Dia::Core::PathStore::ResolvePathToCString(alias));
			}

			mStats = loader.LoadInto(*mRegistry);
			mStats.mRootsMissing += unresolved;
			mRampStats = rampLoader.Load(*mRamps);

			// Code-side entries (Feature 4 / 7) go in before the snapshot is published.
			for (unsigned int i = 0; i < mContributors.Size(); ++i)
			{
				mContributors[i]->ContributeEntries(*mRegistry);
			}

			mCodeSideReport = CodeSideReport();
			mCodeSideCollected = false;
			mLastWarnedLate = 0;
			mComponentReport = ComponentFieldReport();
			mAuditReport = ComponentAttributeAuditReport();
			if (mCodeSideEnabled)
			{
				DIA_PROFILE_SCOPE("ValueSchema.CollectCodeSide", Dia::Observation::Profile::Category::kDiaAssetRuntime);
				ICodeSideSource& source = mCodeSideSource ? *mCodeSideSource : static_cast<ICodeSideSource&>(mLiveSource);
				CodeSideCollector collector;
				mCodeSideReport = collector.Collect(source, *mRegistry);
				mCodeSideCollected = true;
			}

			if (mCodeSideEnabled)
			{
				DIA_PROFILE_SCOPE("ValueSchema.CollectComponentFields", Dia::Observation::Profile::Category::kDiaAssetRuntime);
				const Dia::Reflect::FieldAttributeRegistry& attrs = mAttrRegistry ? *mAttrRegistry : Dia::Reflect::FieldAttributeRegistry::Instance();
				ComponentFieldCollector componentCollector;
				if (mComponentOverride)
				{
					mComponentReport = componentCollector.Collect(mComponentDescs, mComponentDescCount, *mRegistry);
					mAuditReport = mAudit.Run(attrs, mComponentDescs, mComponentDescCount);
				}
				else
				{
					mComponentReport = componentCollector.Collect(*mRegistry);
					mAuditReport = mAudit.Run(attrs);
				}
			}

			mRegistry->Publish();
			mRamps->Publish();
			mService = Dia::Core::MakeUnique<ValueSchemaService>(*mRegistry, mRamps.Get());
		}

		DIA_LOG_INFO("VSC", "Value schema loaded: %u files (%u failed), %u entries from files, %u total, %u roots missing",
			mStats.mFilesLoaded, mStats.mFilesFailed, mStats.mEntriesLoaded, mRegistry->Count(), mStats.mRootsMissing);

		if (mCodeSideCollected)
		{
			DIA_LOG_INFO("VSC", "code-side: %u metrics (%u described, %u undescribed), %u commands, %u args (%u described)",
				mCodeSideReport.mMetricsCollected, mCodeSideReport.mMetricsDescribed, mCodeSideReport.mMetricsUndescribed,
				mCodeSideReport.mCommandsCollected, mCodeSideReport.mArgsCollected, mCodeSideReport.mArgsDescribed);
		}

		if (mCodeSideEnabled)
		{
			DIA_LOG_INFO("VSC", "component fields: %u collected (%u described, %u undescribed)",
				mComponentReport.mCollected, mComponentReport.mDescribed, mComponentReport.mUndescribed);
			if (mAuditReport.mDroppedCount > 0)
			{
				DIA_LOG_ERROR("VSC", "%u attribute registrations dropped; raise FieldAttributeRegistry::kMaxEntries", mAuditReport.mDroppedCount);
			}
		}

		DIA_LOG_INFO("VSC", "Ramps loaded: %u files (%u failed), %u ramps, %u rejected",
			mRampStats.mFilesLoaded, mRampStats.mFilesFailed, mRampStats.mRampsLoaded, mRampStats.mRampsRejected);

		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricEntriesLoaded = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.entries_loaded"));
		mMetricFilesFailed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.files_failed"));
		mMetricRampsLoaded = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.ramps_loaded"));
		mMetricRampFilesFailed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.ramp_files_failed"));
		if (mMetricRampsLoaded) { mMetricRampsLoaded->Set(static_cast<double>(mRamps->Count())); }
		if (mMetricRampFilesFailed) { mMetricRampFilesFailed->Set(static_cast<double>(mRampStats.mFilesFailed)); }
		mMetricMetricsDescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.metrics_described"));
		mMetricMetricsUndescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.metrics_undescribed"));
		mMetricCommandsDescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.commands_described"));
		mMetricArgsDescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.args_described"));
		if (mMetricMetricsDescribed) { mMetricMetricsDescribed->Set(static_cast<double>(mCodeSideReport.mMetricsDescribed)); }
		if (mMetricMetricsUndescribed) { mMetricMetricsUndescribed->Set(static_cast<double>(mCodeSideReport.mMetricsUndescribed)); }
		if (mMetricCommandsDescribed) { mMetricCommandsDescribed->Set(static_cast<double>(mCodeSideReport.mCommandsCollected)); }
		if (mMetricArgsDescribed) { mMetricArgsDescribed->Set(static_cast<double>(mCodeSideReport.mArgsDescribed)); }
		mMetricComponentDescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.component_fields_described"));
		mMetricComponentUndescribed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.component_fields_undescribed"));
		mMetricAttrEntries = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.attr_registry_entries"));
		mMetricAttrDropped = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.attr_registry_dropped"));
		if (mMetricComponentDescribed) { mMetricComponentDescribed->Set(static_cast<double>(mComponentReport.mDescribed)); }
		if (mMetricComponentUndescribed) { mMetricComponentUndescribed->Set(static_cast<double>(mComponentReport.mUndescribed)); }
		if (mMetricAttrEntries) { mMetricAttrEntries->Set(static_cast<double>(mAuditReport.mEntriesUsed)); }
		if (mMetricAttrDropped) { mMetricAttrDropped->Set(static_cast<double>(mAuditReport.mDroppedCount)); }
		mMetricClampCount = metrics.RegisterCounter(Dia::Core::StringCRC("valueschema.clamp.count"));
		if (mMetricEntriesLoaded) { mMetricEntriesLoaded->Set(static_cast<double>(mRegistry->Count())); }
		if (mMetricFilesFailed) { mMetricFilesFailed->Set(static_cast<double>(mStats.mFilesFailed)); }
		mClamper.BindClampCounter(mMetricClampCount);

		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);

		mServiceWriter.Register(*mService);
		RegisterQuery();
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	// --- Describe mode ----------------------------------------------------------
	namespace
	{
		constexpr float kDescribeTimeoutSec = 120.0f;
		constexpr unsigned int kStablePolls = 2;

		// Barrier over the Application's own introspection: Init is finished on every PU when no module is Starting or
		// Stopping, none Failed and no stage transition is in flight. Two consecutive ready polls are required because
		// the main thread commits the new stage and BeginStart()s the incoming modules in separate steps.
		class ApplicationDescribeBarrier : public IDescribeBarrier
		{
		public:
			ApplicationDescribeBarrier(Dia::ApplicationFlow::IApplicationControl* control,
				Dia::ApplicationFlow::IApplicationInspectable* inspect, const char* stage)
				: mControl(control)
				, mInspect(inspect)
				, mHasStage(stage != nullptr && stage[0] != '\0')
				, mStage(mHasStage ? stage : "")
			{
			}

			BarrierState Poll() override
			{
				if (mControl == nullptr || mInspect == nullptr) { return BarrierState::kAborted; }

				if (mHasStage)
				{
					if (!mRequested)
					{
						if (!StageExists()) { DIA_LOG_ERROR("VSC", "Describe mode: unknown stage '%s'", mStage.AsChar()); return BarrierState::kAborted; }
						mRequested = true;
						if (mInspect->GetCurrentStage() != mStage) { mControl->TransitionTo(mStage); }
					}
					if (mInspect->GetCurrentStage() != mStage) { mStable = 0; return BarrierState::kWaiting; }
				}

				if (mInspect->IsTransitioning()) { mStable = 0; return BarrierState::kWaiting; }

				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 4> pus;
				mInspect->GetProcessingUnits(pus);
				for (unsigned int p = 0; p < pus.Size(); ++p)
				{
					Dia::Core::Containers::DynamicArrayC<Dia::ApplicationFlow::ModuleStateInfo, 64> modules;
					mInspect->GetActiveModules(pus[p], modules);
					for (unsigned int m = 0; m < modules.Size(); ++m)
					{
						switch (modules[m].state)
						{
						case Dia::ApplicationFlow::ModuleState::kFailed:	return BarrierState::kAborted;
						case Dia::ApplicationFlow::ModuleState::kStarting:
						case Dia::ApplicationFlow::ModuleState::kStopping:	mStable = 0; return BarrierState::kWaiting;
						default: break;
						}
					}
				}
				return ++mStable >= kStablePolls ? BarrierState::kReady : BarrierState::kWaiting;
			}

		private:
			bool StageExists() const
			{
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> stages;
				mInspect->GetAllStages(stages);
				for (unsigned int i = 0; i < stages.Size(); ++i) { if (stages[i] == mStage) { return true; } }
				return false;
			}

			Dia::ApplicationFlow::IApplicationControl*		mControl;
			Dia::ApplicationFlow::IApplicationInspectable*	mInspect;
			bool											mHasStage;
			Dia::Core::StringCRC							mStage;
			bool											mRequested = false;
			unsigned int									mStable = 0;
		};
	}

	static std::chrono::steady_clock::time_point sDescribeStart;

	void ValueSchemaModule::TickDescribeMode(float /*deltaTime*/)
	{
		if (mDescribeRunner == nullptr)
		{
			sDescribeStart = std::chrono::steady_clock::now();
			mDescribeRunner = Dia::Core::MakeUnique<DescribeModeRunner>(DescribeSession::GetRequest());
			mDescribeBarrier.Reset(new ApplicationDescribeBarrier(GetApplication(), mAppInspect, DescribeSession::GetRequest().mStage.AsCStr()));
		}
		if (mDescribeRunner->IsFinished() || mService == nullptr) { return; }

		// Wall-clock, not the frame delta: the MainPU loop is not paced when idle (Main passes a fixed 1/30 s every iteration), so
		// summing deltaTime would burn the 120 s budget in a couple of real seconds while a stage's assets are still loading.
		// Describe mode is process-global (DescribeSession), so one static time point is enough (set when the runner is created).
		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		mDescribeWaitedSec = std::chrono::duration<float>(now - sDescribeStart).count();
		mDescribeRunner->Tick(*mDescribeBarrier, *mService, mCodeSideCollected ? &mCodeSideReport : nullptr,
			mCodeSideEnabled ? &mComponentReport : nullptr);
		if (!mDescribeRunner->IsFinished() && mDescribeWaitedSec >= kDescribeTimeoutSec)
		{
			mDescribeRunner->Fail("timed out waiting for every processing unit to finish Init");
		}
		if (mDescribeRunner->IsFinished())
		{
			DescribeSession::Complete(mDescribeRunner->GetExitCode());
			if (auto* control = GetApplication()) { control->RequestShutdown(); }
		}
	}

	void ValueSchemaModule::DoUpdate(float deltaTime)
	{
		ResolveSimCatalogue();
		// valueschema.get could not be registered in DoStart when the DebugServer stream was not yet committed: retry until it is.
		if (mService && !mQueryHandler && mQueryRegistryOverride == nullptr && ResolveQueryRegistry(mAppInspect) != nullptr) { RegisterQuery(); }
		if (DescribeSession::IsActive() && !DescribeSession::IsComplete()) { TickDescribeMode(deltaTime); }
	}

	Dia::ApplicationFlow::StopResult ValueSchemaModule::DoStop()
	{
		// Registry + service stay alive until the next DoStart or destruction (see header).
		mServiceWriter.Deregister();
		UnregisterQuery();
		Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
		mClamper.BindClampCounter(nullptr);
		mMetricEntriesLoaded = nullptr;
		mMetricFilesFailed = nullptr;
		mMetricRampsLoaded = nullptr;
		mMetricRampFilesFailed = nullptr;
		mMetricClampCount = nullptr;
		mMetricMetricsDescribed = nullptr;
		mMetricMetricsUndescribed = nullptr;
		mMetricCommandsDescribed = nullptr;
		mMetricArgsDescribed = nullptr;
		mMetricComponentDescribed = nullptr;
		mMetricComponentUndescribed = nullptr;
		mMetricAttrEntries = nullptr;
		mMetricAttrDropped = nullptr;
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using ValueSchemaModule_ = Dia::ValueSchema::ValueSchemaModule; }
namespace { using ValueSchemaService = Dia::ValueSchema::ValueSchemaService; }
DIA_MODULE(ValueSchemaModule_);
DIA_DESCRIBE(ValueSchemaModule_::kTypeId, "Loads *.schema.json sidecars from the configured schema_roots aliases and publishes the read-only ValueSchemaService.");
DIA_STREAM_TYPE(ValueSchemaService);
