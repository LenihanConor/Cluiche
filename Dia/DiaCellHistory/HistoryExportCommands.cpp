#ifdef DIA_DEBUG

#include <DiaCellHistory/HistoryExportCommands.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/ExportObservation.h>
#include <DiaCellHistory/HistoryExporter.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaObservation/Log/DiaLog.h>

#include <json/json.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace Dia::CellHistory
{
	using Dia::API::ValueParam;

	namespace
	{
		const char* const kExportName = "cells.history.export";

		const ValueParam kParams[] = {
			{ "path", "string", true, "Output file. Relative: under Cluiche/out/<App>/history/ (no '..'); absolute is allowed" },
			{ "cells", "bool", false, "true: include per-cell data of tracked layers (larger file)" },
			{ "overwrite", "bool", false, "true: replace an existing file (otherwise refused)" } };

		void CopyText(char* dst, uint32_t capacity, const char* src)
		{
			snprintf(dst, capacity, "%s", src != nullptr ? src : "");
		}
	}

	HistoryExportCommands::HistoryExportCommands(const CellHistoryRecorder& recorder) : mRecorder(recorder) {}

	void HistoryExportCommands::SetAppName(const char* app) { CopyText(mApp, sizeof(mApp), app); }
	void HistoryExportCommands::SetBuildInfo(const char* gitCommit) { CopyText(mGit, sizeof(mGit), gitCommit); }
	void HistoryExportCommands::SetOutDir(const char* outDir) { CopyText(mOutDir, sizeof(mOutDir), outDir); }

	void HistoryExportCommands::FormatUtc(int64_t unixSeconds, char* buf, uint32_t capacity)
	{
		if (buf == nullptr || capacity == 0) { return; }
		buf[0] = '\0';
		if (capacity < 21) { return; }
		const time_t t = time_t(unixSeconds);
		tm utc = {};
		if (gmtime_s(&utc, &t) != 0) { return; }
		snprintf(buf, capacity, "%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
	}

	Json::Value HistoryExportCommands::Fail(const char* message) const
	{
		DIA_LOG_ERROR("HIS", "%s rejected: %s", kExportName, message);
		Json::Value out(Json::objectValue);
		out["error"] = message;
		return out;
	}

	Json::Value HistoryExportCommands::Export(const Json::Value& args)
	{
		if (!args.isObject() || !args.isMember("path") || !args["path"].isString()) { return Fail("missing or non-string param 'path'"); }
		bool cells = false;
		bool overwrite = false;
		if (args.isMember("cells"))
		{
			if (!args["cells"].isBool()) { return Fail("param 'cells' must be a bool"); }
			cells = args["cells"].asBool();
		}
		if (args.isMember("overwrite"))
		{
			if (!args["overwrite"].isBool()) { return Fail("param 'overwrite' must be a bool"); }
			overwrite = args["overwrite"].asBool();
		}

		const std::string requested = args["path"].asString();
		char resolved[1000];
		char err[192];
		if (!HistoryExporter::ResolvePath(requested.c_str(), mApp, mOutDir, resolved, sizeof(resolved), err, sizeof(err))) { return Fail(err); }

		// A relative path lands in the history folder: create it on demand.
		const bool relative = !std::filesystem::path(requested).has_root_path();
		if (relative)
		{
			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(resolved).parent_path(), ec);
		}

		char generatedAt[24];
		const int64_t now = mTimeFn != nullptr ? mTimeFn(mTimeUser) : int64_t(std::time(nullptr));
		FormatUtc(now, generatedAt, sizeof(generatedAt));

		ExportOptions options;
		options.cells = cells;
		options.overwrite = overwrite;
		options.app = mApp;
		options.gitCommit = mGit;
		options.generatedAt = generatedAt;
		options.snapshots = mSnapshots;

		const double t0 = mObs != nullptr ? mObs->NowMs() : 0.0;
		ExportResult result;
		if (!HistoryExporter::WriteFile(mRecorder, options, resolved, result)) { return Fail(result.error); }
		if (mObs != nullptr) { mObs->OnExported(mObs->NowMs() - t0, result.bytes); }

		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["path"] = resolved;
		out["bytes"] = Json::UInt64(result.bytes);
		out["samples"] = Json::UInt(result.samples);
		return out;
	}

	bool HistoryExportCommands::Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (mRegistered) { return true; }
		Dia::DebugServer::SimHelpInfo help;
		help.description = "Export the recorded cell history to a dia.cell_history file (temp file + rename). Blocks the simulation while it writes. Relative paths go under out/<App>/history. Returns {ok, path, bytes, samples}.";
		help.category = "cells.history";
		help.owner = "DiaCellHistory";
		help.params = kParams;
		help.paramCount = 3u;
		if (!registrar.RegisterCommand(kExportName, help, [this](const Json::Value& args) -> Json::Value { return Export(args); }))
		{
			DIA_LOG_ERROR("HIS", "export command: registering %s was rejected", kExportName);
			return false;
		}
		mRegistered = true;
		return true;
	}

	void HistoryExportCommands::Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (!mRegistered) { return; }
		registrar.Unregister(kExportName);
		mRegistered = false;
	}
}

#endif // DIA_DEBUG
