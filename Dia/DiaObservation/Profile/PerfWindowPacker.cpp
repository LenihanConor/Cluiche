#include "PerfWindowPacker.h"

#include "CoarseRing.h"
#include <DiaCore/Strings/Base64.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		namespace
		{
			void AppendEscaped(std::string& out, const char* text)
			{
				out.push_back('"');
				if (text)
				{
					for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
					{
						switch (*p)
						{
						case '"':  out += "\\\""; break;
						case '\\': out += "\\\\"; break;
						case '\n': out += "\\n"; break;
						case '\r': out += "\\r"; break;
						case '\t': out += "\\t"; break;
						default:
							if (*p < 0x20)
							{
								char esc[8];
								snprintf(esc, sizeof(esc), "\\u%04x", *p);
								out += esc;
							}
							else
							{
								out.push_back(static_cast<char>(*p));
							}
						}
					}
				}
				out.push_back('"');
			}

			void AppendNumber(std::string& out, const char* fmt, double value)
			{
				char buf[48];
				snprintf(buf, sizeof(buf), fmt, value);
				out += buf;
			}

			void AppendUnsigned(std::string& out, uint64_t value)
			{
				char buf[24];
				snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
				out += buf;
			}

			void AppendU16Base64(std::string& out, const std::vector<uint16_t>& values)
			{
				std::vector<uint8_t> bytes(values.size() * 2);
				for (size_t i = 0; i < values.size(); ++i)
				{
					bytes[i * 2]     = static_cast<uint8_t>(values[i] & 0xFF);
					bytes[i * 2 + 1] = static_cast<uint8_t>(values[i] >> 8);
				}
				out.push_back('"');
				Dia::Core::Base64::Encode(bytes.data(), bytes.size(), out);
				out.push_back('"');
			}

			void PackUnit(const PerfWindowUnitInput& unit, float seconds, PerfNameResolver resolver, std::string& out)
			{
				const CoarseRing* ring = unit.ring;
				const uint32_t moduleCount = ring ? ring->GetModuleCount() : 0;

				uint32_t count = 0;
				uint64_t first = 0;
				if (ring && moduleCount > 0)
				{
					const uint64_t oldest = ring->GetOldestAvailable();
					const uint64_t end = ring->GetEntryCount();
					const uint64_t available = end - oldest;
					const uint32_t wanted = CoarseRing::CapacityFor(unit.unitHz, seconds);
					count = static_cast<uint32_t>(std::min<uint64_t>(available, wanted));
					first = end - count;
				}

				out += "{\"unit\":";
				AppendEscaped(out, unit.name);
				out += ",\"unitCrc\":";
				AppendUnsigned(out, unit.unitCrc);
				out += ",\"hz\":";
				AppendNumber(out, "%g", unit.unitHz);
				out += ",\"firstEntry\":";
				AppendUnsigned(out, first);
				out += ",\"count\":";
				AppendUnsigned(out, count);

				out += ",\"modules\":[";
				for (uint32_t m = 0; m < moduleCount; ++m)
				{
					const uint32_t crc = ring->GetModuleCrc(m);
					char nameBuf[128];
					const char* name = resolver ? resolver(crc, nameBuf, sizeof(nameBuf)) : nullptr;
					if (!name)
					{
						snprintf(nameBuf, sizeof(nameBuf), "0x%08x", crc);
						name = nameBuf;
					}
					if (m > 0)
						out.push_back(',');
					out += "{\"crc\":";
					AppendUnsigned(out, crc);
					out += ",\"name\":";
					AppendEscaped(out, name);
					out.push_back('}');
				}
				out.push_back(']');

				std::vector<uint16_t> values;
				std::vector<uint8_t> flags;
				bool anyFlag = false;
				if (count > 0)
				{
					values.resize(count);
					ring->CopyRange(CoarseRing::kUnitSlot, first, count, values.data());
				}
				out += ",\"unitMs\":";
				AppendU16Base64(out, values);

				values.clear();
				if (count > 0)
				{
					values.resize(static_cast<size_t>(count) * moduleCount);
					for (uint32_t m = 0; m < moduleCount; ++m)
						ring->CopyRange(m, first, count, values.data() + static_cast<size_t>(m) * count);
				}
				out += ",\"moduleMs\":";
				AppendU16Base64(out, values);

				if (count > 0)
				{
					flags.resize(count);
					ring->CopyFlags(first, count, flags.data());
					anyFlag = std::any_of(flags.begin(), flags.end(), [](uint8_t f) { return f != 0; });
				}
				if (anyFlag)
				{
					out += ",\"flags\":\"";
					Dia::Core::Base64::Encode(flags.data(), flags.size(), out);
					out.push_back('"');
				}
				out.push_back('}');
			}
		}

		void PackPerfWindow(const PerfWindowUnitInput* units, uint32_t unitCount, float seconds,
			PerfNameResolver resolver, std::string& outJson)
		{
			if (!(seconds >= 1.0f))
				seconds = 1.0f;
			if (seconds > 60.0f)
				seconds = 60.0f;

			outJson.clear();
			outJson += "{\"v\":1,\"seconds\":";
			AppendNumber(outJson, "%g", seconds);
			outJson += ",\"units\":[";
			for (uint32_t i = 0; i < unitCount; ++i)
			{
				if (i > 0)
					outJson.push_back(',');
				PackUnit(units[i], seconds, resolver, outJson);
			}
			outJson += "],\"markers\":[],\"trips\":[],\"truncated\":false}";
		}

		void PackPerfFrameUnavailable(std::string& outJson)
		{
			outJson = "{\"v\":1,\"available\":false,\"reason\":\"profiling off\"}";
		}

		void PackPerfFrameSections(const char* unit, uint64_t frame, const PerfFrameSection* sections,
			uint32_t count, std::string& outJson)
		{
			outJson = "{\"v\":1,\"available\":true,\"unit\":";
			AppendEscaped(outJson, unit);
			outJson += ",\"frame\":";
			AppendUnsigned(outJson, frame);
			outJson += ",\"sections\":[";
			for (uint32_t i = 0; i < count; ++i)
			{
				if (i > 0)
					outJson.push_back(',');
				outJson += "{\"name\":";
				AppendEscaped(outJson, sections[i].name);
				outJson += ",\"ms\":";
				AppendNumber(outJson, "%.3f", sections[i].ms);
				outJson.push_back('}');
			}
			outJson += "]}";
		}
	}}
}
