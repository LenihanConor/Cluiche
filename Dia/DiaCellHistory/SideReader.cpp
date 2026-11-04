#include <DiaCellHistory/SideReader.h>

#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellLayers/CellLayerRegistry.h>

#include <cstdio>
#include <cstring>

namespace Dia::CellHistory
{
	namespace
	{
		bool Fail(SideParseResult& err, const char* message)
		{
			err.ok = false;
			std::snprintf(err.error, SideParseResult::kErrorChars, "%s", message);
			return false;
		}
		bool StartsWith(const char* s, const char* prefix) { return std::strncmp(s, prefix, std::strlen(prefix)) == 0; }
	}

	bool ParseSide(const char* text, Side& out, SideParseResult& err)
	{
		err.ok = false;
		err.error[0] = '\0';
		if (text == nullptr || text[0] == '\0')
		{
			return Fail(err, "side is empty; expected now, sample:<i> or snapshot:<name>");
		}
		Side parsed;
		if (std::strcmp(text, "now") == 0)
		{
			parsed.kind = SideKind::Now;
		}
		else if (StartsWith(text, "sample:"))
		{
			const char* digits = text + 7;
			if (*digits == '\0') { return Fail(err, "sample side needs an index: sample:<i>"); }
			uint64_t value = 0;
			for (const char* c = digits; *c != '\0'; ++c)
			{
				if (*c < '0' || *c > '9') { return Fail(err, "sample index must be a non-negative integer: sample:<i>"); }
				value = value * 10 + static_cast<uint64_t>(*c - '0');
				if (value > 0xFFFFFFFFull) { return Fail(err, "sample index is out of range"); }
			}
			parsed.kind = SideKind::Sample;
			parsed.sample = static_cast<uint32_t>(value);
		}
		else if (StartsWith(text, "snapshot:"))
		{
			const char* name = text + 9;
			const size_t len = std::strlen(name);
			if (len == 0) { return Fail(err, "snapshot side needs a name: snapshot:<name>"); }
			if (len > 63) { return Fail(err, "snapshot name is too long (max 63 characters)"); }
			parsed.kind = SideKind::Snapshot;
			parsed.snapshot = Dia::Core::Containers::String64("%s", name);
		}
		else
		{
			return Fail(err, "unknown side; expected now, sample:<i> or snapshot:<name>");
		}
		out = parsed;
		err.ok = true;
		return true;
	}

	bool SidesEqual(const Side& a, const Side& b)
	{
		if (a.kind != b.kind) { return false; }
		switch (a.kind)
		{
		case SideKind::Now:		return true;
		case SideKind::Sample:	return a.sample == b.sample;
		case SideKind::Snapshot:	return std::strcmp(a.snapshot.AsCStr(), b.snapshot.AsCStr()) == 0;
		}
		return false;
	}

	SideReadStatus SideReader::Read(const Side& side, const Dia::CellLayers::CellLayerDesc& layer, float* out, uint32_t n) const
	{
		if (side.kind == SideKind::Snapshot) { return SideReadStatus::Unavailable; }
		if (out == nullptr || n == 0) { return SideReadStatus::NoData; }

		if (side.kind == SideKind::Now)
		{
			if (!layer.read) { return SideReadStatus::NoData; }
			layer.read(out, n);
			return SideReadStatus::Ok;
		}

		if (n != mRecorder.CellCount()) { return SideReadStatus::NoData; }
		if (side.sample < mRecorder.FirstRetained() || side.sample >= mRecorder.SampleCount()) { return SideReadStatus::NoData; }
		return mRecorder.CellValues(side.sample, layer.id, out, n) ? SideReadStatus::Ok : SideReadStatus::NoData;
	}
}
