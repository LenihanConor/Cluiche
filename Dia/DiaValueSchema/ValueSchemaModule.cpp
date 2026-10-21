#include <DiaValueSchema/ValueSchemaModule.h>
#include <DiaValueSchema/IValueSchemaContributor.h>

#include <DiaApplicationFlow/Application.h>
#include <DiaCore/FilePath/PathStore.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
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

	// --- Config ---------------------------------------------------------------
	// { "schema_roots": ["alias", ...] } -- PathStore aliases, resolved at start.
	void ValueSchemaModule::OnConfigure(const char* configJson)
	{
		mRootAliases.RemoveAll();
		if (!configJson || configJson[0] == '\0') { return; }

		Json::Value root;
		Json::Reader reader;
		if (!reader.parse(configJson, root) || !root.isObject())
		{
			DIA_LOG_WARNING("VSC", "ValueSchemaModule config is not a JSON object; no schema roots");
			return;
		}
		const Json::Value& roots = root["schema_roots"];
		if (!roots.isArray())
		{
			DIA_LOG_WARNING("VSC", "ValueSchemaModule config has no 'schema_roots' array; no schema roots");
			return;
		}
		for (Json::ArrayIndex i = 0; i < roots.size(); ++i)
		{
			if (!roots[i].isString() || roots[i].asString().empty())
			{
				DIA_LOG_WARNING("VSC", "schema_roots[%u] is not a non-empty string; ignored", static_cast<unsigned int>(i));
				continue;
			}
			const std::string alias = roots[i].asString();
			if (alias.size() > 63 || mRootAliases.IsFull())	// String64 holds 63 chars
			{
				DIA_LOG_WARNING("VSC", "schema_roots entry '%s' ignored (too long or more than %u roots)", alias.c_str(), kMaxSchemaRoots);
				continue;
			}
			Dia::Core::Containers::String64 name;
			name = alias.c_str();
			mRootAliases.Add(name);
		}
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

			// Fresh registry each start: a published registry is immutable.
			mService.Reset();
			mRegistry = Dia::Core::MakeUnique<ValueSchemaRegistry>();

			SchemaDirectoryLoader loader;
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
				loader.AddRoot(Dia::Core::PathStore::ResolvePathToCString(alias));
			}

			mStats = loader.LoadInto(*mRegistry);
			mStats.mRootsMissing += unresolved;

			// Code-side entries (Feature 4 / 7) go in before the snapshot is published.
			for (unsigned int i = 0; i < mContributors.Size(); ++i)
			{
				mContributors[i]->ContributeEntries(*mRegistry);
			}

			mRegistry->Publish();
			mService = Dia::Core::MakeUnique<ValueSchemaService>(*mRegistry);
		}

		DIA_LOG_INFO("VSC", "Value schema loaded: %u files (%u failed), %u entries from files, %u total, %u roots missing",
			mStats.mFilesLoaded, mStats.mFilesFailed, mStats.mEntriesLoaded, mRegistry->Count(), mStats.mRootsMissing);

		auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
		mMetricEntriesLoaded = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.entries_loaded"));
		mMetricFilesFailed = metrics.RegisterGauge(Dia::Core::StringCRC("valueschema.files_failed"));
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
		mMetricClampCount = nullptr;
		return Dia::ApplicationFlow::StopResult::kDone;
	}
}

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using ValueSchemaModule_ = Dia::ValueSchema::ValueSchemaModule; }
namespace { using ValueSchemaService = Dia::ValueSchema::ValueSchemaService; }
DIA_MODULE(ValueSchemaModule_);
DIA_DESCRIBE(ValueSchemaModule_::kTypeId, "Loads *.schema.json sidecars from the configured schema_roots aliases and publishes the read-only ValueSchemaService.");
DIA_STREAM_TYPE(ValueSchemaService);
