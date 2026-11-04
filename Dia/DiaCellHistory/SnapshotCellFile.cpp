#include <DiaCellHistory/SnapshotCellFile.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/CellRing.h>
#include <DiaCellHistory/HistoryEncoding.h>
#include <DiaCellHistory/SnapshotNaming.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <json/json.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Dia::CellHistory
{
	using Dia::Core::StringCRC;

	namespace
	{
		const char* const kFormat = "dia.cell_snapshot";
		const int kVersion = 1;
		const uint32_t kMaxGridSide = 65535;

		void SetError(char* err, uint32_t cap, const char* fmt, const char* a = "", const char* b = "")
		{
			if (err != nullptr && cap > 0) { snprintf(err, cap, fmt, a, b); }
		}

		void CopyText(char* dst, size_t cap, const std::string& src)
		{
			snprintf(dst, cap, "%s", src.c_str());
		}

		uint32_t BytesPerCell(SnapshotLayerKind k) { return k == SnapshotLayerKind::Float ? 2u : 1u; }

		bool ParseHeaderAndGrid(const Json::Value& root, SnapshotHeader& header, SnapshotGridInfo& grid, char* err, uint32_t cap)
		{
			if (!root.isObject()) { SetError(err, cap, "not a dia.cell_snapshot file: root is not an object (format)"); return false; }
			if (!root["format"].isString() || root["format"].asString() != kFormat)
			{
				SetError(err, cap, "not a dia.cell_snapshot file (format)");
				return false;
			}
			if (!root["version"].isInt() || root["version"].asInt() != kVersion)
			{
				SetError(err, cap, "unsupported dia.cell_snapshot version (expected 1)");
				return false;
			}
			const Json::Value& h = root["header"];
			if (!h.isObject()) { SetError(err, cap, "missing header object"); return false; }
			if (!h["name"].isString() || !IsValidSnapshotName(h["name"].asCString()))
			{
				SetError(err, cap, "header.name is missing or not a valid snapshot name");
				return false;
			}
			const std::string run = h["runId"].isString() ? h["runId"].asString() : std::string();
			if (run.size() != 16 || run.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
			{
				SetError(err, cap, "header.runId must be 16 hex digits (runId)");
				return false;
			}
			if (!h["sample"].isUInt()) { SetError(err, cap, "header.sample is missing or not an unsigned integer"); return false; }

			const Json::Value& g = root["grid"];
			if (!g.isObject() || !g["id"].isString()) { SetError(err, cap, "missing grid object or grid.id"); return false; }
			if (!g["width"].isUInt() || !g["height"].isUInt() || g["width"].asUInt() == 0 || g["height"].asUInt() == 0
				|| g["width"].asUInt() > kMaxGridSide || g["height"].asUInt() > kMaxGridSide)
			{
				SetError(err, cap, "grid size invalid: width and height must be 1 to 65535");
				return false;
			}

			header = SnapshotHeader();
			CopyText(header.mName, sizeof(header.mName), h["name"].asString());
			header.mRunId = std::strtoull(run.c_str(), nullptr, 16);
			header.mSample = h["sample"].asUInt();
			if (h["label"].isString()) { CopyText(header.mLabel, sizeof(header.mLabel), h["label"].asString()); }
			header.mCreatedUnix = h["createdUnix"].isIntegral() ? int64_t(h["createdUnix"].asInt64()) : 0;
			if (h["world"].isString()) { CopyText(header.mWorld, sizeof(header.mWorld), h["world"].asString()); }
			grid = SnapshotGridInfo();
			CopyText(grid.mId, sizeof(grid.mId), g["id"].asString());
			grid.mWidth = g["width"].asUInt();
			grid.mHeight = g["height"].asUInt();
			return true;
		}

		bool LoadJson(const char* path, Json::Value& root, char* err, uint32_t cap)
		{
			std::ifstream f(path, std::ios::binary);
			if (!f) { SetError(err, cap, "cannot open snapshot cell file '%s'", path != nullptr ? path : ""); return false; }
			std::stringstream ss;
			ss << f.rdbuf();
			Json::Reader reader;
			if (!reader.parse(ss.str(), root, false))
			{
				SetError(err, cap, "snapshot cell file is not valid JSON (corrupt or truncated)");
				return false;
			}
			return true;
		}
	}

	bool SnapshotGridInfo::SameAs(const SnapshotGridInfo& o) const
	{
		return mWidth == o.mWidth && mHeight == o.mHeight && std::strcmp(mId, o.mId) == 0;
	}

	void SnapshotLayer::SetEnumName(uint32_t i, const char* name)
	{
		if (i >= mEnumCount || name == nullptr) { return; }
		snprintf(mEnumNames + size_t(i) * kEnumNameChars, kEnumNameChars, "%s", name);
	}

	void SnapshotLayer::Release()
	{
		delete[] mPayload;
		delete[] mEnumNames;
		mPayload = nullptr;
		mEnumNames = nullptr;
		mPayloadBytes = 0;
		mEnumCount = 0;
	}

	const SnapshotLayer* SnapshotCellData::FindLayer(StringCRC id) const
	{
		for (uint32_t i = 0; i < mLayerCount; ++i)
		{
			if (StringCRC(mLayers[i].mId) == id) { return &mLayers[i]; }
		}
		return nullptr;
	}

	SnapshotLayer* SnapshotCellData::AddLayer(const char* id, SnapshotLayerKind kind, float mn, float mx, uint32_t enumCount)
	{
		const uint32_t cells = CellCount();
		if (mLayerCount >= kMaxLayers || cells == 0 || id == nullptr || enumCount > SnapshotLayer::kMaxEnumValues) { return nullptr; }
		SnapshotLayer& l = mLayers[mLayerCount];
		l.Release();
		snprintf(l.mId, sizeof(l.mId), "%s", id);
		l.mKind = kind;
		l.mMin = mn;
		l.mMax = mx;
		l.mPayloadBytes = cells * BytesPerCell(kind);
		l.mPayload = new uint8_t[l.mPayloadBytes]();
		if (kind == SnapshotLayerKind::Category && enumCount > 0)
		{
			l.mEnumCount = enumCount;
			l.mEnumNames = new char[size_t(enumCount) * SnapshotLayer::kEnumNameChars]();
		}
		++mLayerCount;
		return &l;
	}

	void SnapshotCellData::Clear()
	{
		for (uint32_t i = 0; i < mLayerCount; ++i) { mLayers[i].Release(); }
		mLayerCount = 0;
		mHeader = SnapshotHeader();
		mGrid = SnapshotGridInfo();
	}

	bool SnapshotCellFile::Path(const char* folder, const char* name, char* buf, uint32_t capacity)
	{
		if (folder == nullptr || folder[0] == '\0' || !IsValidSnapshotName(name) || buf == nullptr || capacity == 0) { return false; }
		const int n = snprintf(buf, capacity, "%s/snapshot_%s.cells.json", folder, name);
		return n > 0 && uint32_t(n) < capacity;
	}

	bool SnapshotCellFile::BuildFromLive(const CellHistoryRecorder& rec, const Dia::CellLayers::CellLayerRegistry& registry,
		const Dia::ValueSchema::ValueSchemaService* schema, const char* name, int64_t createdUnix, const char* world,
		SnapshotCellData& out, char* err, uint32_t errCapacity)
	{
		out.Clear();
		if (!IsValidSnapshotName(name)) { SetError(err, errCapacity, "invalid snapshot name"); return false; }
		const uint32_t recorded = rec.RecordedLayerCount();
		if (recorded == 0) { SetError(err, errCapacity, "no recorded layers to snapshot"); return false; }

		const Dia::CellLayers::CellLayerDesc* first = registry.Find(rec.RecordedLayerId(0));
		if (first == nullptr || first->grid == nullptr) { SetError(err, errCapacity, "recorded layer is not registered"); return false; }
		const Dia::Graphics::CellGridDesc* grid = first->grid;

		snprintf(out.mHeader.mName, sizeof(out.mHeader.mName), "%s", name);
		out.mHeader.mRunId = rec.RunId();
		const uint32_t samples = rec.SampleCount();
		out.mHeader.mSample = samples > 0 ? samples - 1 : 0;
		if (samples > 0 && rec.HasHeader(out.mHeader.mSample))
		{
			snprintf(out.mHeader.mLabel, sizeof(out.mHeader.mLabel), "%s", rec.Header(out.mHeader.mSample).mLabel.AsCStr());
		}
		out.mHeader.mCreatedUnix = createdUnix;
		snprintf(out.mHeader.mWorld, sizeof(out.mHeader.mWorld), "%s", world != nullptr ? world : "");
		snprintf(out.mGrid.mId, sizeof(out.mGrid.mId), "%s", grid->id.AsChar());
		out.mGrid.mWidth = grid->width;
		out.mGrid.mHeight = grid->height;

		const uint32_t cells = out.CellCount();
		std::vector<float> scratch(cells);
		for (uint32_t i = 0; i < recorded; ++i)
		{
			const StringCRC id = rec.RecordedLayerId(i);
			const Dia::CellLayers::CellLayerDesc* desc = registry.Find(id);
			if (desc == nullptr || desc->grid != grid || desc->read.empty()) { continue; }
			const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(desc->schemaKey) : nullptr;
			const bool category = desc->kind == Dia::CellLayers::CellValueKind::Category;

			if (!category && !(info != nullptr && info->mHasMin && info->mHasMax && info->mMax > info->mMin))
			{
				DIA_LOG_WARNING("HIS", "snapshot: float layer '%s' has no schema min/max range; it is not included", id.AsChar());
				continue;
			}
			const uint32_t enumCount = (category && info != nullptr)
				? std::min<uint32_t>(info->mEnumValues.Size(), SnapshotLayer::kMaxEnumValues) : 0u;
			SnapshotLayer* layer = out.AddLayer(id.AsChar(), category ? SnapshotLayerKind::Category : SnapshotLayerKind::Float,
				category ? 0.f : float(info->mMin), category ? 0.f : float(info->mMax), enumCount);
			if (layer == nullptr) { SetError(err, errCapacity, "snapshot layer table is full"); out.Clear(); return false; }
			for (uint32_t v = 0; v < enumCount; ++v) { layer->SetEnumName(v, info->mEnumValues[v].mId.AsCStr()); }

			desc->read(scratch.data(), cells);	// live values
			uint8_t* dst = layer->MutablePayload();
			if (category)
			{
				for (uint32_t c = 0; c < cells; ++c) { dst[c] = QuantiseCellCategory(scratch[c]); }
			}
			else
			{
				for (uint32_t c = 0; c < cells; ++c)
				{
					const uint16_t q = QuantiseCellFloat(scratch[c], layer->mMin, layer->mMax);
					dst[c * 2] = uint8_t(q & 0xFF);
					dst[c * 2 + 1] = uint8_t(q >> 8);
				}
			}
		}
		if (out.LayerCount() == 0)
		{
			SetError(err, errCapacity, "no recorded layer could be captured");
			out.Clear();
			return false;
		}
		return true;
	}

	bool SnapshotCellFile::WriteAtomic(const SnapshotCellData& data, const char* path, char* err, uint32_t errCapacity)
	{
		if (path == nullptr || path[0] == '\0') { SetError(err, errCapacity, "no path given"); return false; }

		Json::Value root(Json::objectValue);
		root["format"] = kFormat;
		root["version"] = kVersion;
		Json::Value header(Json::objectValue);
		header["name"] = data.mHeader.mName;
		char run[24];
		snprintf(run, sizeof(run), "%016llx", (unsigned long long)data.mHeader.mRunId);
		header["runId"] = run;
		header["sample"] = Json::UInt(data.mHeader.mSample);
		header["label"] = data.mHeader.mLabel;
		header["createdUnix"] = Json::Int64(data.mHeader.mCreatedUnix);
		header["world"] = data.mHeader.mWorld;
		root["header"] = header;
		Json::Value grid(Json::objectValue);
		grid["id"] = data.mGrid.mId;
		grid["width"] = Json::UInt(data.mGrid.mWidth);
		grid["height"] = Json::UInt(data.mGrid.mHeight);
		root["grid"] = grid;

		Json::Value layers(Json::objectValue);
		std::vector<char> text;
		for (uint32_t i = 0; i < data.LayerCount(); ++i)
		{
			const SnapshotLayer& l = *data.LayerAt(i);
			Json::Value j(Json::objectValue);
			if (l.mKind == SnapshotLayerKind::Float)
			{
				j["kind"] = "float";
				j["min"] = double(l.mMin);
				j["max"] = double(l.mMax);
			}
			else
			{
				j["kind"] = "category";
				Json::Value values(Json::arrayValue);
				for (uint32_t v = 0; v < l.EnumCount(); ++v) { values.append(l.EnumName(v)); }
				j["values"] = values;
			}
			text.assign(EncodedLength(l.PayloadBytes()) + 1, '\0');
			uint32_t len = 0;
			if (!EncodeUInt8(l.Payload(), l.PayloadBytes(), text.data(), uint32_t(text.size()), len))
			{
				SetError(err, errCapacity, "cannot encode layer '%s'", l.mId);
				return false;
			}
			j["data"] = std::string(text.data(), len);
			layers[l.mId] = j;
		}
		root["layers"] = layers;

		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		const std::string body = Json::writeString(builder, root);

		namespace fs = std::filesystem;
		const std::string tmp = std::string(path) + ".tmp";
		{
			std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
			if (!f) { SetError(err, errCapacity, "cannot create temporary file '%s'", tmp.c_str()); return false; }
			f.write(body.data(), std::streamsize(body.size()));
			f.flush();
			if (!f)
			{
				f.close();
				std::error_code rm;
				fs::remove(tmp, rm);
				SetError(err, errCapacity, "write failed for '%s' (disk full or not writable)", tmp.c_str());
				return false;
			}
		}
		std::error_code ec;
		fs::rename(tmp, path, ec);
		if (ec)
		{
			std::error_code rm;
			fs::remove(tmp, rm);
			SetError(err, errCapacity, "cannot move temporary file into place: %s", ec.message().c_str());
			return false;
		}
		return true;
	}

	bool SnapshotCellFile::ReadHeaderOnly(const char* path, SnapshotHeader& header, SnapshotGridInfo& grid, char* err, uint32_t errCapacity)
	{
		Json::Value root;
		if (!LoadJson(path, root, err, errCapacity)) { return false; }
		return ParseHeaderAndGrid(root, header, grid, err, errCapacity);
	}

	bool SnapshotCellFile::Read(const char* path, SnapshotCellData& out, char* err, uint32_t errCapacity)
	{
		out.Clear();
		Json::Value root;
		if (!LoadJson(path, root, err, errCapacity)) { return false; }
		if (!ParseHeaderAndGrid(root, out.mHeader, out.mGrid, err, errCapacity)) { out.Clear(); return false; }

		const Json::Value& layers = root["layers"];
		if (!layers.isObject()) { SetError(err, errCapacity, "missing layers object"); out.Clear(); return false; }
		if (layers.size() > SnapshotCellData::kMaxLayers) { SetError(err, errCapacity, "too many layers (limit is 64)"); out.Clear(); return false; }

		const std::vector<std::string> names = layers.getMemberNames();
		for (const std::string& id : names)
		{
			const Json::Value& j = layers[id];
			const char* nm = id.c_str();
			if (!j.isObject()) { SetError(err, errCapacity, "layer '%s': not an object", nm); out.Clear(); return false; }
			const std::string kindText = j["kind"].isString() ? j["kind"].asString() : std::string();
			SnapshotLayerKind kind;
			if (kindText == "float") { kind = SnapshotLayerKind::Float; }
			else if (kindText == "category") { kind = SnapshotLayerKind::Category; }
			else { SetError(err, errCapacity, "layer '%s': unknown kind", nm); out.Clear(); return false; }

			float mn = 0.f, mx = 0.f;
			if (kind == SnapshotLayerKind::Float)
			{
				if (!j["min"].isNumeric() || !j["max"].isNumeric() || !(j["max"].asDouble() > j["min"].asDouble()))
				{
					SetError(err, errCapacity, "layer '%s': float layer needs a min < max range", nm);
					out.Clear();
					return false;
				}
				mn = float(j["min"].asDouble());
				mx = float(j["max"].asDouble());
			}
			uint32_t enumCount = 0;
			if (kind == SnapshotLayerKind::Category && j.isMember("values"))
			{
				if (!j["values"].isArray() || j["values"].size() > SnapshotLayer::kMaxEnumValues)
				{
					SetError(err, errCapacity, "layer '%s': values must be an array of at most 256 names", nm);
					out.Clear();
					return false;
				}
				enumCount = j["values"].size();
			}
			if (!j["data"].isString()) { SetError(err, errCapacity, "layer '%s': missing data", nm); out.Clear(); return false; }

			SnapshotLayer* layer = out.AddLayer(nm, kind, mn, mx, enumCount);
			if (layer == nullptr) { SetError(err, errCapacity, "layer '%s': cannot be stored", nm); out.Clear(); return false; }
			for (uint32_t v = 0; v < enumCount; ++v)
			{
				if (j["values"][v].isString()) { layer->SetEnumName(v, j["values"][v].asCString()); }
			}
			const std::string data = j["data"].asString();
			if (data.size() != EncodedLength(layer->PayloadBytes()))
			{
				SetError(err, errCapacity, "layer '%s': payload size does not match the grid", nm);
				out.Clear();
				return false;
			}
			if (!DecodeUInt8(data.c_str(), uint32_t(data.size()), layer->MutablePayload(), layer->PayloadBytes()))
			{
				SetError(err, errCapacity, "layer '%s': invalid base64 data", nm);
				out.Clear();
				return false;
			}
		}
		return true;
	}

	bool SnapshotCellFile::Dequantise(const SnapshotCellData& data, StringCRC layer, float* out, uint32_t n)
	{
		const SnapshotLayer* l = data.FindLayer(layer);
		if (l == nullptr || out == nullptr || n != data.CellCount()) { return false; }
		const uint8_t* p = l->Payload();
		if (l->mKind == SnapshotLayerKind::Float)
		{
			for (uint32_t i = 0; i < n; ++i)
			{
				const uint16_t q = uint16_t(p[i * 2] | (uint16_t(p[i * 2 + 1]) << 8));
				out[i] = DequantiseCellFloat(q, l->mMin, l->mMax);
			}
		}
		else
		{
			for (uint32_t i = 0; i < n; ++i) { out[i] = float(p[i]); }
		}
		return true;
	}
}
