#include <DiaValueSchema/ValueSchemaExporter.h>
#include <DiaValueSchema/ValueSchemaQuery.h>

#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <time.h>
#include <string.h>

namespace Dia::ValueSchema
{
	namespace
	{
		std::string UtcNowIso()
		{
			const time_t now = time(nullptr);
			struct tm utc;
			memset(&utc, 0, sizeof(utc));
			gmtime_s(&utc, &now);
			char buf[32];
			strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
			return buf;
		}

		Json::Value Item(const UndescribedItem& u)
		{
			Json::Value o(Json::objectValue);
			o["name"] = u.mName.AsCStr();
			o["owner"] = u.mOwner.AsCStr();
			return o;
		}

		// "<command>.<arg>" -> index of the '.' that splits it, preferring the LONGEST prefix that is a registered
		// command (command names contain dots). npos when no registered command matches.
		size_t SplitArg(const ValueSchemaService& service, const std::string& name)
		{
			size_t pos = name.rfind('.');
			while (pos != std::string::npos && pos > 0)
			{
				const std::string cmd = name.substr(0, pos);
				const ValueInfo* info = service.Find(Dia::Core::StringCRC(CodeSideCollector::kCommandFor), Dia::Core::StringCRC(cmd.c_str()));
				if (info != nullptr && info->mKind == ValueKind::Command) { return pos; }
				pos = name.rfind('.', pos - 1);
			}
			return std::string::npos;
		}

		Json::Value UndescribedCommands(const ValueSchemaService& service, const CodeSideReport& rep)
		{
			Json::Value list(Json::arrayValue);
			for (unsigned int i = 0; i < rep.mUndescribedCommands.Size(); ++i)
			{
				Json::Value o = Item(rep.mUndescribedCommands[i]);
				o["args"] = Json::Value(Json::arrayValue);
				list.append(o);
			}
			for (unsigned int i = 0; i < rep.mUndescribedParams.Size(); ++i)
			{
				const UndescribedItem& p = rep.mUndescribedParams[i];
				const std::string name = p.mName.AsCStr();
				const size_t pos = SplitArg(service, name);
				const std::string cmd = pos == std::string::npos ? name : name.substr(0, pos);
				const std::string arg = pos == std::string::npos ? std::string() : name.substr(pos + 1);
				Json::Value* target = nullptr;
				for (Json::ArrayIndex k = 0; k < list.size(); ++k)
				{
					if (list[k]["name"].asString() == cmd) { target = &list[k]; break; }
				}
				if (target == nullptr)
				{
					Json::Value o(Json::objectValue);
					o["name"] = cmd;
					o["owner"] = p.mOwner.AsCStr();
					o["args"] = Json::Value(Json::arrayValue);
					list.append(o);
					target = &list[list.size() - 1];
				}
				if (!arg.empty()) { (*target)["args"].append(arg); }
			}
			return list;
		}
	}

	Json::Value ValueSchemaExporter::Build(const ValueSchemaService& service, const CodeSideReport* codeSide,
		const ComponentFieldReport* components, const ExportHeader& header) const
	{
		DIA_PROFILE_SCOPE("ValueSchema.Export", Dia::Observation::Profile::Category::kDiaAssetRuntime);

		Json::Value doc(Json::objectValue);
		doc["format"] = kFormat;
		doc["version"] = kVersion;

		Json::Value h(Json::objectValue);
		h["app"] = header.mApp != nullptr ? header.mApp : "";
		h["config"] = header.mConfig != nullptr ? header.mConfig : "";
		h["generated_at"] = UtcNowIso();
		h["code_side"] = true;
		doc["header"] = h;

		Json::Value entries(Json::arrayValue);
		for (unsigned int i = 0; i < service.Count(); ++i)
		{
			const ValueInfo* info = service.At(i);
			if (info != nullptr) { entries.append(ValueInfoToExportJson(*info)); }
		}
		doc["entries"] = entries;

		Json::Value ramps(Json::arrayValue);
		for (unsigned int i = 0; i < service.RampCount(); ++i)
		{
			const RampData* ramp = service.RampAt(i);
			if (ramp != nullptr) { ramps.append(RampToJson(*ramp)); }
		}
		doc["ramps"] = ramps;

		Json::Value metrics(Json::arrayValue);
		Json::Value commands(Json::arrayValue);
		Json::Value fields(Json::arrayValue);
		if (codeSide != nullptr)
		{
			for (unsigned int i = 0; i < codeSide->mUndescribedMetrics.Size(); ++i) { metrics.append(Item(codeSide->mUndescribedMetrics[i])); }
			commands = UndescribedCommands(service, *codeSide);
		}
		if (components != nullptr)
		{
			for (unsigned int i = 0; i < components->mUndescribedFields.Size(); ++i) { fields.append(Item(components->mUndescribedFields[i])); }
		}
		Json::Value undescribed(Json::objectValue);
		undescribed["metrics"] = metrics;
		undescribed["commands"] = commands;
		undescribed["component_fields"] = fields;
		doc["undescribed"] = undescribed;

		const unsigned int undescribedCount = metrics.size() + commands.size() + fields.size();
		auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
		if (auto* g = reg.RegisterGauge(Dia::Core::StringCRC("valueschema.export_entries"))) { g->Set(static_cast<double>(entries.size())); }
		if (auto* g = reg.RegisterGauge(Dia::Core::StringCRC("valueschema.export_undescribed"))) { g->Set(static_cast<double>(undescribedCount)); }
		return doc;
	}

	bool ValueSchemaExporter::WriteFile(const char* path, const Json::Value& doc)
	{
		DIA_PROFILE_SCOPE("ValueSchema.ExportWrite", Dia::Observation::Profile::Category::kDiaAssetRuntime);
		if (path == nullptr || path[0] == '\0')
		{
			DIA_LOG_ERROR("VSC", "Value schema export not written: empty path");
			return false;
		}
		const std::string tmp = std::string(path) + ".tmp";
		{
			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			builder["emitUTF8"] = true;
			const std::string text = Json::writeString(builder, doc);
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			out.write(text.data(), static_cast<std::streamsize>(text.size()));
			out.close();
			if (!out)
			{
				DIA_LOG_ERROR("VSC", "Value schema export not written: cannot write '%s'", tmp.c_str());
				std::error_code ignore;
				std::filesystem::remove(tmp, ignore);
				return false;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tmp, path, ec);
		if (ec)
		{
			DIA_LOG_ERROR("VSC", "Value schema export not written: rename to '%s' failed (%s)", path, ec.message().c_str());
			std::filesystem::remove(tmp, ec);
			return false;
		}
		DIA_LOG_INFO("VSC", "Value schema export written to '%s' (%u entries, %u undescribed)", path,
			doc["entries"].size(),
			doc["undescribed"]["metrics"].size() + doc["undescribed"]["commands"].size() + doc["undescribed"]["component_fields"].size());
		return true;
	}
}
