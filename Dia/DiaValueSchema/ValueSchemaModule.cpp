#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/IValueSchemaContributor.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Health/HealthRegistry.h>

#include <json/json.h>

namespace Dia::ValueSchema
{
	const Dia::Core::StringCRC ValueSchemaModule::kTypeId("ValueSchemaModule");

	ValueSchemaModule::ValueSchemaModule(const Dia::Core::StringCRC& instanceId)
		: Module(instanceId)
	{
	}

	ValueSchemaModule::~ValueSchemaModule() = default;

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
	}

	// --- Lifecycle ------------------------------------------------------------
	Dia::ApplicationFlow::StartResult ValueSchemaModule::DoStart()
	{
		{
			DIA_TRACE_ZONE("ValueSchema.Load", Dia::Observation::Trace::Category::kDiaAssetRuntime);

			// Starting again without a Stop: take the stream down first so consumers never see (and the stream never
			// re-registers over) a service that is about to be destroyed. No-op when not registered.
			mServiceWriter.Deregister();

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
			if (mCodeSideEnabled)
			{
				DIA_PROFILE_SCOPE("ValueSchema.CollectCodeSide", Dia::Observation::Profile::Category::kDiaAssetRuntime);
				ICodeSideSource& source = mCodeSideSource ? *mCodeSideSource : static_cast<ICodeSideSource&>(mLiveSource);
				CodeSideCollector collector;
				mCodeSideReport = collector.Collect(source, *mRegistry);
				mCodeSideCollected = true;
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
		mMetricClampCount = metrics.RegisterCounter(Dia::Core::StringCRC("valueschema.clamp.count"));
		if (mMetricEntriesLoaded) { mMetricEntriesLoaded->Set(static_cast<double>(mRegistry->Count())); }
		if (mMetricFilesFailed) { mMetricFilesFailed->Set(static_cast<double>(mStats.mFilesFailed)); }
		mClamper.BindClampCounter(mMetricClampCount);

		Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);

		mServiceWriter.Register(*mService);
		return Dia::ApplicationFlow::StartResult::kReady;
	}

	void ValueSchemaModule::DoUpdate(float /*deltaTime*/)
	{
	}

	Dia::ApplicationFlow::StopResult ValueSchemaModule::DoStop()
	{
		// Registry + service stay alive until the next DoStart or destruction (see header).
		mServiceWriter.Deregister();
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
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using ValueSchemaModule_ = Dia::ValueSchema::ValueSchemaModule; }
namespace { using ValueSchemaService = Dia::ValueSchema::ValueSchemaService; }
DIA_MODULE(ValueSchemaModule_);
DIA_DESCRIBE(ValueSchemaModule_::kTypeId, "Loads *.schema.json sidecars from the configured schema_roots aliases and publishes the read-only ValueSchemaService.");
DIA_STREAM_TYPE(ValueSchemaService);
