#pragma once

// Test helper (Testing/ convention): reads a `dia.cell_history` file written by HistoryExporter with jsoncpp and decodes its
// base64 arrays with HistoryEncoding. Reused by the export tests and the conformance fixtures.

#include <DiaCellHistory/HistoryEncoding.h>
#include <json/json.h>

#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Dia::CellHistory::Testing
{
	struct DecodedExport
	{
		Json::Value root;
		std::string error;

		bool Load(const char* path)
		{
			std::ifstream f(path, std::ios::binary);
			if (!f) { error = "cannot open file"; return false; }
			std::stringstream ss;
			ss << f.rdbuf();
			Json::Reader reader;
			if (!reader.parse(ss.str(), root, false)) { error = "not valid JSON"; return false; }
			return root.isObject();
		}

		const Json::Value& Header() const { return root["header"]; }
		uint32_t SampleCount() const { return Header()["sampleCount"].asUInt(); }
		uint32_t AggregateFirst() const { return Header()["aggregateFirst"].asUInt(); }
		uint32_t CellFirst() const { return Header()["cellFirst"].asUInt(); }

		static bool DecodeF32(const Json::Value& v, uint32_t n, std::vector<float>& out)
		{
			out.assign(n, 0.f);
			if (!v.isString()) { return false; }
			const std::string& s = v.asString();
			return n == 0 ? s.empty() : DecodeFloat32(s.c_str(), uint32_t(s.size()), out.data(), n);
		}
		static bool DecodeU16(const Json::Value& v, uint32_t n, std::vector<uint16_t>& out)
		{
			out.assign(n, 0);
			if (!v.isString()) { return false; }
			const std::string& s = v.asString();
			return n == 0 ? s.empty() : DecodeUInt16(s.c_str(), uint32_t(s.size()), out.data(), n);
		}
		static bool DecodeU8(const Json::Value& v, uint32_t n, std::vector<uint8_t>& out)
		{
			out.assign(n, 0);
			if (!v.isString()) { return false; }
			const std::string& s = v.asString();
			return n == 0 ? s.empty() : DecodeUInt8(s.c_str(), uint32_t(s.size()), out.data(), n);
		}

		// Group sizes cover [aggregateFirst, sampleCount).
		bool GroupSizes(const char* group, std::vector<uint16_t>& out) const
		{
			return DecodeU16(root["groupSizes"][group], SampleCount() - AggregateFirst(), out);
		}
		// Float layer series for one group: covers [first, sampleCount).
		bool FloatSeries(const char* layer, const char* group, const char* which, std::vector<float>& out, uint32_t& first) const
		{
			const Json::Value& l = root["aggregates"][layer];
			if (!l.isObject() || l["kind"].asString() != "float") { return false; }
			first = l["first"].asUInt();
			return DecodeF32(l["groups"][group][which], SampleCount() - first, out);
		}
		// Category counts: sample-major, enumCount per sample.
		bool CategorySeries(const char* layer, const char* group, std::vector<uint16_t>& out, uint32_t& first, uint32_t& enumCount) const
		{
			const Json::Value& l = root["aggregates"][layer];
			if (!l.isObject() || l["kind"].asString() != "category") { return false; }
			first = l["first"].asUInt();
			enumCount = l["values"].size();
			return DecodeU16(l["groups"][group], (SampleCount() - first) * enumCount, out);
		}
		// Per-cell: the quantised codes of one sample (uint16 for float layers, uint8 widened for category layers).
		bool CellCodes(const char* layer, uint32_t sample, std::vector<uint16_t>& out) const
		{
			const Json::Value& l = root["cells"][layer];
			if (!l.isObject()) { return false; }
			const uint32_t first = l["first"].asUInt();
			const uint32_t count = l["count"].asUInt();
			if (sample < first || sample - first >= count) { return false; }
			const uint32_t cells = l["width"].asUInt() * l["height"].asUInt();
			const Json::Value& d = l["data"][sample - first];
			if (l["kind"].asString() == "float") { return DecodeU16(d, cells, out); }
			std::vector<uint8_t> b;
			if (!DecodeU8(d, cells, b)) { return false; }
			out.assign(b.begin(), b.end());
			return true;
		}
		// Dequantised per-cell values: v = min + q / 65534 * (max - min) for float layers (0xFFFF = NaN); category ids as floats.
		bool CellValues(const char* layer, uint32_t sample, std::vector<float>& out) const
		{
			std::vector<uint16_t> q;
			if (!CellCodes(layer, sample, q)) { return false; }
			const Json::Value& l = root["cells"][layer];
			const bool isFloat = l["kind"].asString() == "float";
			const float mn = float(l["min"].asDouble());
			const float mx = float(l["max"].asDouble());
			out.resize(q.size());
			for (size_t i = 0; i < q.size(); ++i)
			{
				if (!isFloat) { out[i] = float(q[i]); }
				else if (q[i] == 0xFFFF) { out[i] = std::numeric_limits<float>::quiet_NaN(); }
				else { out[i] = mn + float(q[i]) / 65534.0f * (mx - mn); }
			}
			return true;
		}
	};
}
