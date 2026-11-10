#ifdef DIA_DEBUG

#include <DiaCellHistory/SnapshotCommands.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/ISnapshotStore.h>
#include <DiaCellHistory/SnapshotCellCache.h>
#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/SnapshotManager.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <DiaCellHistory/SnapshotObservation.h>
#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <json/json.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace Dia::CellHistory
{
	using Dia::API::ValueParam;

	namespace
	{
		namespace fs = std::filesystem;

		const char* const kTakeName = "cells.snapshot.take";
		const char* const kRestoreName = "cells.snapshot.restore";
		const char* const kListName = "cells.snapshot.list";
		const char* const kDeleteName = "cells.snapshot.delete";
		const char* const kLayerName = "cells.history.snapshot";

		const ValueParam kParamsTake[] = {
			{ "name", "string", true, "Snapshot name: 1 to 32 characters from A-Z a-z 0-9 _ -" },
			{ "overwrite", "bool", false, "true: replace an existing snapshot of that name" } };
		const ValueParam kParamsName[] = { { "name", "string", true, "Snapshot name" } };
		const ValueParam kParamsLayer[] = {
			{ "name", "string", true, "Snapshot name" },
			{ "layer", "string", true, "Layer id inside the snapshot" },
			{ "offset", "int", false, "First cell of the chunk (default 0)" },
			{ "count", "int", false, "Cells in the chunk (default and maximum: the chunk limit, 4096)" } };

		Dia::DebugServer::SimHelpInfo Help(const char* description, const char* category, const ValueParam* params, unsigned int count)
		{
			Dia::DebugServer::SimHelpInfo h;
			h.description = description; h.category = category; h.owner = "DiaCellHistory";
			if (params != nullptr) { h.params = params; h.paramCount = count; } else { h.noArgs = true; }
			return h;
		}

		bool ParseUInt(const Json::Value& v, uint64_t& out)
		{
			if (v.isUInt64()) { out = v.asUInt64(); return true; }
			if (v.isInt64()) { if (v.asInt64() < 0) { return false; } out = uint64_t(v.asInt64()); return true; }
			if (!v.isString()) { return false; }
			const std::string s = v.asString();
			if (s.empty()) { return false; }
			for (char c : s) { if (c < '0' || c > '9') { return false; } }
			out = std::strtoull(s.c_str(), nullptr, 10);
			return true;
		}

		bool NameArg(const Json::Value& args, const char*& name, std::string& store, const char*& error)
		{
			if (!args.isObject() || !args.isMember("name") || !args["name"].isString()) { error = "missing or non-string param 'name'"; return false; }
			store = args["name"].asString();
			name = store.c_str();
			return true;
		}

		struct ListEntry
		{
			std::string	name;
			uint32_t	sample = 0;
			std::string	label;
			int64_t		created = 0;
			std::string	world;
			uint64_t	bytes = 0;
			bool		sameRun = false;
			SnapshotStatus status = SnapshotStatus::Ready;
			std::string	reason;
			bool		fromRecord = false;
		};

		void FillFromHeader(ListEntry& e, const SnapshotHeader& h, uint64_t runId, uint64_t bytes)
		{
			e.sample = h.mSample; e.label = h.mLabel; e.created = h.mCreatedUnix; e.world = h.mWorld;
			e.bytes = bytes; e.sameRun = h.mRunId == runId;
		}

		void MarkFailed(ListEntry& e, const char* reason) { e.status = SnapshotStatus::Failed; e.reason = reason; }

		bool ParseCellFileName(const std::string& file, std::string& name)
		{
			static const char kPrefix[] = "snapshot_";
			static const char kSuffix[] = ".cells.json";
			const size_t p = sizeof(kPrefix) - 1, s = sizeof(kSuffix) - 1;
			if (file.size() <= p + s || file.compare(0, p, kPrefix) != 0 || file.compare(file.size() - s, s, kSuffix) != 0) { return false; }
			name = file.substr(p, file.size() - p - s);
			return true;
		}
	}

	SnapshotCommands::SnapshotCommands(const CellHistoryRecorder& recorder, SnapshotManager& manager, ISnapshotStore& store)
		: mRecorder(recorder), mManager(manager), mStore(store) {}

	void SnapshotCommands::SetMaxChunk(uint32_t cells)
	{
		mMaxChunk = std::min(std::max(cells, 1u), kMaxChunkCells);
	}

	Json::Value SnapshotCommands::Fail(const char* command, const char* message) const
	{
		DIA_LOG_ERROR("HIS", "%s rejected: %s", command, message);
		Json::Value out(Json::objectValue);
		out["error"] = message;
		return out;
	}

	Json::Value SnapshotCommands::Take(const Json::Value& args)
	{
		const char* name = nullptr; std::string store; const char* error = nullptr;
		if (!NameArg(args, name, store, error)) { return Fail(kTakeName, error); }
		bool overwrite = false;
		if (args.isMember("overwrite"))
		{
			if (!args["overwrite"].isBool()) { return Fail(kTakeName, "param 'overwrite' must be a bool"); }
			overwrite = args["overwrite"].asBool();
		}
		TakeResult res;
		if (!mManager.Take(name, overwrite, res)) { return Fail(kTakeName, res.error); }
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["pending"] = res.pending;
		return out;
	}

	Json::Value SnapshotCommands::Restore(const Json::Value& args)
	{
		const char* name = nullptr; std::string store; const char* error = nullptr;
		if (!NameArg(args, name, store, error)) { return Fail(kRestoreName, error); }
		RestoreResult res;
		if (!mManager.Restore(name, res)) { return Fail(kRestoreName, res.error); }
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		out["pending"] = res.pending;
		return out;
	}

	void SnapshotCommands::WarnOrphanOnce(const char* name, const char* why)
	{
		for (uint32_t i = 0; i < mOrphanCount; ++i) { if (std::strcmp(mOrphansWarned[i], name) == 0) { return; } }
		if (mOrphanCount < kOrphanMemory)
		{
			std::snprintf(mOrphansWarned[mOrphanCount], sizeof(mOrphansWarned[0]), "%s", name);
			++mOrphanCount;
		}
		DIA_LOG_WARNING("HIS", "cell history snapshot '%s': orphan found: %s", name, why);
	}

	Json::Value SnapshotCommands::List()
	{
		DIA_PROFILE_SCOPE("cellhistory.snapshot_list", ::Dia::Observation::Profile::Category::kDiaGraphics);
		const uint64_t runId = mRecorder.RunId();
		const char* folder = mStore.Folder();
		std::vector<ListEntry> entries;
		auto find = [&](const std::string& n) -> ListEntry* { for (ListEntry& e : entries) { if (e.name == n) { return &e; } } return nullptr; };

		// (a) this session's records
		const SnapshotRecord* records = mManager.Records();
		for (uint32_t i = 0; i < mManager.Count(); ++i)
		{
			const SnapshotRecord& r = records[i];
			if (!r.mUsed) { continue; }
			ListEntry e;
			e.fromRecord = true; e.name = r.mName; e.sample = r.mSample; e.label = r.mLabel; e.created = r.mCreatedUnix;
			e.world = r.mWorld; e.bytes = r.mBytes; e.sameRun = r.mRunId == runId; e.status = r.mStatus; e.reason = r.mReason;
			entries.push_back(e);
		}

		// (b) the cell files of the store folder (earlier sessions, and the files of this session's records)
		std::vector<std::pair<std::string, std::string>> files;		// name, path
		if (folder != nullptr && folder[0] != '\0')
		{
			std::error_code ec;
			for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
			{
				if (!it->is_regular_file(ec)) { continue; }
				std::string name;
				if (ParseCellFileName(it->path().filename().string(), name)) { files.emplace_back(name, it->path().string()); }
			}
		}
		for (const auto& f : files)
		{
			ListEntry* rec = find(f.first);
			if (rec != nullptr && rec->status != SnapshotStatus::Ready) { continue; }		// the record wins for pending/failed
			SnapshotHeader header; SnapshotGridInfo grid; char err[192] = {};
			std::error_code ec;
			const uint64_t size = fs::file_size(f.second, ec);
			ListEntry fresh;
			ListEntry& e = rec != nullptr ? *rec : fresh;
			e.name = f.first;
			if (!SnapshotCellFile::ReadHeaderOnly(f.second.c_str(), header, grid, err, sizeof(err))) { MarkFailed(e, err); WarnOrphanOnce(f.first.c_str(), err); }
			else
			{
				FillFromHeader(e, header, runId, ec ? 0 : size);
				if (!mStore.Exists(f.first.c_str())) { MarkFailed(e, "state file missing"); WarnOrphanOnce(f.first.c_str(), "state file missing"); }
			}
			if (rec == nullptr) { entries.push_back(e); }
		}
		// a ready record whose cell file was not seen
		for (ListEntry& e : entries)
		{
			if (!e.fromRecord || e.status != SnapshotStatus::Ready) { continue; }
			bool seen = false;
			for (const auto& f : files) { if (f.first == e.name) { seen = true; break; } }
			if (!seen) { MarkFailed(e, "cell file missing"); WarnOrphanOnce(e.name.c_str(), "cell file missing"); }
		}

		std::sort(entries.begin(), entries.end(), [](const ListEntry& a, const ListEntry& b)
		{
			return a.created != b.created ? a.created < b.created : a.name < b.name;
		});

		if (mObs != nullptr)
		{
			uint32_t ready = 0;
			for (const ListEntry& e : entries) { if (e.status == SnapshotStatus::Ready) { ++ready; } }
			mObs->SetSnapshotCount(ready);
		}

		Json::Value out(Json::objectValue);
		Json::Value& list = out["snapshots"] = Json::Value(Json::arrayValue);
		for (size_t i = 0; i < entries.size() && i < kMaxListEntries; ++i)
		{
			const ListEntry& e = entries[i];
			Json::Value j(Json::objectValue);
			j["name"] = e.name;
			j["sample"] = e.sample;
			j["label"] = e.label;
			j["createdUnix"] = Json::Int64(e.created);
			j["world"] = e.world;
			j["bytes"] = Json::UInt64(e.bytes);
			j["sameRun"] = e.sameRun;
			j["status"] = e.status == SnapshotStatus::Pending ? "pending" : (e.status == SnapshotStatus::Ready ? "ready" : "failed");
			if (e.status == SnapshotStatus::Failed) { j["reason"] = e.reason; }
			list.append(j);
		}
		if (entries.size() > kMaxListEntries) { out["truncated"] = true; }
		return out;
	}

	Json::Value SnapshotCommands::Delete(const Json::Value& args)
	{
		Json::Value out = DeleteImpl(args);
		if (mObs != nullptr && out.isMember("error")) { mObs->OnFailure(); }
		return out;
	}

	Json::Value SnapshotCommands::DeleteImpl(const Json::Value& args)
	{
		const char* name = nullptr; std::string store; const char* error = nullptr;
		if (!NameArg(args, name, store, error)) { return Fail(kDeleteName, error); }
		if (!IsValidSnapshotName(name)) { return Fail(kDeleteName, "invalid snapshot name (1 to 32 characters from A-Z a-z 0-9 _ -)"); }
		if (mManager.OperationInFlight()) { return Fail(kDeleteName, "snapshot operation in progress"); }

		char path[512] = {};
		if (!SnapshotCellFile::Path(mStore.Folder(), name, path, sizeof(path))) { return Fail(kDeleteName, "cannot build the cell file path"); }
		std::error_code ec;
		const bool hasCells = fs::exists(path, ec);
		const bool hasState = mStore.Exists(name);
		bool hasRecord = false;
		for (uint32_t i = 0; i < mManager.Count(); ++i) { if (mManager.Records()[i].mUsed && std::strcmp(mManager.Records()[i].mName, name) == 0) { hasRecord = true; } }
		if (!hasCells && !hasState && !hasRecord)
		{
			char msg[96];
			std::snprintf(msg, sizeof(msg), "snapshot '%s' not found", name);
			return Fail(kDeleteName, msg);
		}

		if (hasState) { mStore.Delete(name); }
		if (hasCells)
		{
			if (!fs::remove(path, ec) && ec)
			{
				char msg[160];
				std::snprintf(msg, sizeof(msg), "snapshot '%s': cannot remove the cell file: %.80s", name, ec.message().c_str());
				return Fail(kDeleteName, msg);
			}
		}
		mManager.RemoveRecord(name);
		if (mCache != nullptr) { mCache->Invalidate(name); }
		DIA_LOG_INFO("HIS", "cell history snapshot '%s': deleted (state %s, cell file %s)", name, hasState ? "removed" : "absent", hasCells ? "removed" : "absent");
		Json::Value out(Json::objectValue);
		out["ok"] = true;
		return out;
	}

	Json::Value SnapshotCommands::Layer(const Json::Value& args)
	{
		DIA_PROFILE_SCOPE("cellhistory.snapshot_query", ::Dia::Observation::Profile::Category::kDiaGraphics);
		if (mObs != nullptr) { mObs->OnQueryServed(); }
		return LayerImpl(args);
	}

	Json::Value SnapshotCommands::LayerImpl(const Json::Value& args)
	{
		const char* name = nullptr; std::string nameStore; const char* error = nullptr;
		if (!NameArg(args, name, nameStore, error)) { return Fail(kLayerName, error); }
		if (!args.isMember("layer") || !args["layer"].isString()) { return Fail(kLayerName, "missing or non-string param 'layer'"); }
		const std::string layerId = args["layer"].asString();
		uint64_t offset = 0, count = 0;
		const bool hasCount = args.isMember("count");
		if (args.isMember("offset") && !ParseUInt(args["offset"], offset)) { return Fail(kLayerName, "param 'offset' is not a non-negative integer"); }
		if (hasCount && !ParseUInt(args["count"], count)) { return Fail(kLayerName, "param 'count' is not a non-negative integer"); }
		if (!IsValidSnapshotName(name)) { return Fail(kLayerName, "invalid snapshot name (1 to 32 characters from A-Z a-z 0-9 _ -)"); }

		char path[512] = {};
		if (!SnapshotCellFile::Path(mStore.Folder(), name, path, sizeof(path))) { return Fail(kLayerName, "cannot build the cell file path"); }
		char msg[256] = {};
		std::error_code ec;
		if (!fs::exists(path, ec))
		{
			std::snprintf(msg, sizeof(msg), "snapshot '%s' not found", name);
			return Fail(kLayerName, msg);
		}
		SnapshotCellData data;
		char err[192] = {};
		if (!SnapshotCellFile::Read(path, data, err, sizeof(err)))
		{
			std::snprintf(msg, sizeof(msg), "snapshot '%s': cell file unreadable: %.120s", name, err);
			return Fail(kLayerName, msg);
		}
		const SnapshotLayer* layer = nullptr;
		for (uint32_t i = 0; i < data.LayerCount(); ++i)
		{
			if (layerId == data.LayerAt(i)->mId) { layer = data.LayerAt(i); break; }
		}
		if (layer == nullptr)
		{
			std::snprintf(msg, sizeof(msg), "layer '%.64s' not in snapshot '%s'", layerId.c_str(), name);
			return Fail(kLayerName, msg);
		}

		const uint32_t total = data.CellCount();
		if (offset > total) { return Fail(kLayerName, "param 'offset' is beyond the layer"); }
		uint64_t n = hasCount ? count : uint64_t(mMaxChunk);
		n = std::min<uint64_t>(n, mMaxChunk);
		n = std::min<uint64_t>(n, total - offset);

		const bool isFloat = layer->mKind == SnapshotLayerKind::Float;
		const uint32_t cells = uint32_t(n);
		const uint32_t bytesPerCell = isFloat ? 2u : 1u;
		std::vector<char> text(EncodedLength(cells * bytesPerCell) + 1u, '\0');
		uint32_t len = 0;
		const uint8_t* payload = layer->Payload() + size_t(offset) * bytesPerCell;
		bool encoded = true;
		if (cells > 0)
		{
			if (isFloat)
			{
				std::vector<uint16_t> codes(cells);
				for (uint32_t i = 0; i < cells; ++i) { codes[i] = uint16_t(payload[2u * i] | (uint16_t(payload[2u * i + 1u]) << 8)); }
				encoded = EncodeUInt16(codes.data(), cells, text.data(), uint32_t(text.size()), len);
			}
			else { encoded = EncodeUInt8(payload, cells, text.data(), uint32_t(text.size()), len); }
		}
		if (!encoded) { return Fail(kLayerName, "encoding failed"); }

		Json::Value out(Json::objectValue);
		out["name"] = name;
		out["layer"] = layer->mId;
		out["kind"] = isFloat ? "float" : "category";
		if (isFloat) { out["min"] = double(layer->mMin); out["max"] = double(layer->mMax); }
		else { out["min"] = Json::Value(Json::nullValue); out["max"] = Json::Value(Json::nullValue); }
		out["width"] = data.mGrid.mWidth;
		out["height"] = data.mGrid.mHeight;
		out["encoding"] = isFloat ? "u16le-base64" : "u8-base64";
		out["offset"] = Json::UInt64(offset);
		out["count"] = cells;
		out["total"] = total;
		out["data"] = std::string(text.data(), len);
		return out;
	}

	bool SnapshotCommands::Register(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (mRegistered) { return true; }

		struct Def { const char* name; Dia::DebugServer::SimHelpInfo help; Dia::CellLayers::MirrorQueryHandler handler; };
		const Def defs[kNameCount] = {
			{ kTakeName, Help("Take snapshot <name>: game state plus a copy of every recorded layer. Returns {ok, pending}; finishes at the next frame boundary (see cells.snapshot.list). overwrite=true replaces an existing one.", "cells.snapshot", kParamsTake, 2u),
				[this](const Json::Value& args) -> Json::Value { return Take(args); } },
			{ kRestoreName, Help("Restore snapshot <name> at the next frame boundary. Same run: history after the snapshot sample is dropped; another run: history is reset. Returns {ok, pending}.", "cells.snapshot", kParamsName, 1u),
				[this](const Json::Value& args) -> Json::Value { return Restore(args); } },
			{ kListName, Help("List snapshots: {snapshots:[{name, sample, label, createdUnix, world, bytes, sameRun, status: pending|ready|failed, reason?}], truncated?}. Includes snapshots from earlier sessions.", "cells.snapshot", nullptr, 0u),
				[this](const Json::Value&) -> Json::Value { return List(); } },
			{ kDeleteName, Help("Delete snapshot <name>: removes the game's state and the cell file (whichever exist). Refused while a take or restore is in flight. Returns {ok}.", "cells.snapshot", kParamsName, 1u),
				[this](const Json::Value& args) -> Json::Value { return Delete(args); } },
			{ kLayerName, Help("Read one layer of snapshot <name> as stored (float: uint16 codes over min..max; category: uint8 ids), base64, at most 4096 cells per call (offset, count, total). Returns {name, layer, kind, min, max, width, height, encoding, offset, count, total, data}.", "cells.history", kParamsLayer, 4u),
				[this](const Json::Value& args) -> Json::Value { return Layer(args); } } };

		for (size_t i = 0; i < kNameCount; ++i)
		{
			if (registrar.RegisterCommand(defs[i].name, defs[i].help, defs[i].handler)) { continue; }
			DIA_LOG_ERROR("HIS", "snapshot commands: registering %s was rejected, rolling back", defs[i].name);
			for (size_t k = 0; k < i; ++k) { registrar.Unregister(defs[k].name); }
			return false;
		}
		mRegistered = true;
		return true;
	}

	void SnapshotCommands::Unregister(Dia::CellLayers::IMirrorQueryRegistrar& registrar)
	{
		if (!mRegistered) { return; }
		registrar.Unregister(kTakeName);
		registrar.Unregister(kRestoreName);
		registrar.Unregister(kListName);
		registrar.Unregister(kDeleteName);
		registrar.Unregister(kLayerName);
		mRegistered = false;
	}
}

#endif // DIA_DEBUG
