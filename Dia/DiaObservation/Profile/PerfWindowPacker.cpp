#include "PerfWindowPacker.h"

#include "CoarseRing.h"
#include <DiaCore/Strings/Base64.h>

#include <algorithm>
#include <cmath>
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

			template <typename T>
			void AppendRawBase64(std::string& out, const std::vector<T>& values)
			{
				std::vector<uint8_t> bytes(values.size() * sizeof(T));
				for (size_t i = 0; i < values.size(); ++i)
					for (size_t b = 0; b < sizeof(T); ++b)
						bytes[i * sizeof(T) + b] = static_cast<uint8_t>((static_cast<uint64_t>(values[i]) >> (8 * b)) & 0xFF);
				out.push_back('"');
				Dia::Core::Base64::Encode(bytes.data(), bytes.size(), out);
				out.push_back('"');
			}

			uint16_t SaturateU16(double v)
			{
				if (!(v > 0.0)) return 0;
				if (v >= 65535.0) return 65535;
				return static_cast<uint16_t>(std::lround(v));
			}

			void PackJobsUnit(const PerfJobsUnitInput& unit, float seconds, PerfNameResolver resolver, std::string& out)
			{
				const float hz = unit.unitHz > 0.0f ? unit.unitHz : 60.0f;
				const uint32_t framesPerBucket = std::max<uint32_t>(1u, static_cast<uint32_t>(std::lround(hz * kJobsBucketMs / 1000.0f)));
				const bool have = unit.perJobRecording && unit.aggs && unit.aggCount > 0;

				std::vector<uint32_t> owners;
				uint32_t bucketCount = 0;
				uint64_t firstFrame = 0;
				if (have)
				{
					for (uint32_t i = 0; i < unit.aggCount; ++i)
						if (std::find(owners.begin(), owners.end(), unit.aggs[i].owner) == owners.end())
							owners.push_back(unit.aggs[i].owner);
					const uint64_t windowFrames = static_cast<uint64_t>(std::ceil(static_cast<double>(seconds) * hz));
					bucketCount = static_cast<uint32_t>((windowFrames + framesPerBucket - 1) / framesPerBucket);
					if (bucketCount == 0) bucketCount = 1;
					const uint64_t span = static_cast<uint64_t>(bucketCount) * framesPerBucket;
					const uint64_t end = static_cast<uint64_t>(unit.aggs[unit.aggCount - 1].frame) + 1;
					firstFrame = end > span ? end - span : 0;
				}

				std::vector<uint16_t> count, busy, wait;
				if (have)
				{
					const size_t cells = static_cast<size_t>(owners.size()) * bucketCount;
					std::vector<double> busyAcc(cells, 0.0);
					std::vector<uint32_t> countAcc(cells, 0);
					std::vector<float> waitAcc(cells, 0.0f);
					for (uint32_t i = 0; i < unit.aggCount; ++i)
					{
						const JobFrameAggregate& a = unit.aggs[i];
						if (a.frame < firstFrame) continue;
						const uint64_t b = (a.frame - firstFrame) / framesPerBucket;
						if (b >= bucketCount) continue;
						const size_t o = static_cast<size_t>(std::find(owners.begin(), owners.end(), a.owner) - owners.begin());
						const size_t cell = o * bucketCount + static_cast<size_t>(b);
						countAcc[cell] += a.count;
						busyAcc[cell] += a.busyMs;
						waitAcc[cell] = std::max(waitAcc[cell], a.worstWaitMs);
					}
					count.resize(cells); busy.resize(cells); wait.resize(cells);
					for (size_t c = 0; c < cells; ++c)
					{
						count[c] = SaturateU16(countAcc[c]);
						busy[c]  = SaturateU16(busyAcc[c] * 100.0);
						wait[c]  = SaturateU16(static_cast<double>(waitAcc[c]) * 100.0);
					}
				}

				out += "{\"unit\":";
				AppendEscaped(out, unit.name);
				out += ",\"unitCrc\":";
				AppendUnsigned(out, unit.unitCrc);
				out += unit.perJobRecording ? ",\"perJobRecording\":true" : ",\"perJobRecording\":false";
				out += ",\"hz\":";
				AppendNumber(out, "%g", hz);
				out += ",\"firstFrame\":";
				AppendUnsigned(out, firstFrame);
				out += ",\"framesPerBucket\":";
				AppendUnsigned(out, framesPerBucket);
				out += ",\"bucketCount\":";
				AppendUnsigned(out, bucketCount);
				out += ",\"owners\":[";
				for (size_t o = 0; o < owners.size(); ++o)
				{
					char nameBuf[128];
					const char* name = nullptr;
					if (owners[o] == 0)
						name = "(other)";
					else if (resolver)
						name = resolver(owners[o], nameBuf, sizeof(nameBuf));
					if (!name)
					{
						snprintf(nameBuf, sizeof(nameBuf), "0x%08x", owners[o]);
						name = nameBuf;
					}
					if (o > 0) out.push_back(',');
					out += "{\"crc\":";
					AppendUnsigned(out, owners[o]);
					out += ",\"name\":";
					AppendEscaped(out, name);
					out.push_back('}');
				}
				out += "],\"frameAgg\":{\"count\":";
				AppendU16Base64(out, count);
				out += ",\"busyMs\":";
				AppendU16Base64(out, busy);
				out += ",\"worstWaitMs\":";
				AppendU16Base64(out, wait);
				out += "}}";
			}

			void PackUnit(const PerfWindowUnitInput& unit, float seconds, PerfNameResolver resolver, std::string& out)
			{
				const CoarseRing* ring = unit.ring;
				const uint32_t slotCount = ring ? ring->GetModuleCount() : 0;
				// Only recorded slots (crc != 0) are packed: assembler rings always have kMaxModules slots.
				std::vector<uint32_t> slots;
				for (uint32_t s = 0; s < slotCount; ++s)
					if (ring->GetModuleCrc(s) != 0)
						slots.push_back(s);
				const uint32_t moduleCount = static_cast<uint32_t>(slots.size());

				uint32_t count = 0;
				uint64_t first = 0;
				if (ring && slotCount > 0)
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
				AppendNumber(out, "%g", std::isfinite(unit.unitHz) ? unit.unitHz : 0.0f);   // nan/inf are not JSON
				out += ",\"firstEntry\":";
				AppendUnsigned(out, first);
				out += ",\"count\":";
				AppendUnsigned(out, count);

				out += ",\"modules\":[";
				for (uint32_t m = 0; m < moduleCount; ++m)
				{
					const uint32_t crc = ring->GetModuleCrc(slots[m]);
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
						ring->CopyRange(slots[m], first, count, values.data() + static_cast<size_t>(m) * count);
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

		void PackPerfFrame(const PerfWindowUnitInput& unit, uint64_t frame, PerfNameResolver resolver,
			std::string& outJson)
		{
			const CoarseRing* ring = unit.ring;
			if (!ring || ring->GetModuleCount() == 0 || ring->GetEntryCount() == 0)
			{
				outJson = "{\"error\":\"no coarse data for unit\"}";
				return;
			}
			if (frame < ring->GetOldestAvailable() || frame >= ring->GetEntryCount())
			{
				outJson = "{\"error\":\"frame outside retained range\"}";
				return;
			}

			outJson = "{\"v\":1,\"unit\":";
			AppendEscaped(outJson, unit.name);
			outJson += ",\"frame\":";
			AppendUnsigned(outJson, frame);

			uint16_t value = 0;
			ring->CopyRange(CoarseRing::kUnitSlot, frame, 1, &value);
			outJson += ",\"unitMs\":";
			AppendNumber(outJson, "%.2f", CoarseRing::FromFixed(value));

			uint8_t flags = 0;
			ring->CopyFlags(frame, 1, &flags);
			if (flags != 0)
			{
				outJson += ",\"flags\":";
				AppendUnsigned(outJson, flags);
			}

			outJson += ",\"modules\":[";
			bool first = true;
			for (uint32_t m = 0; m < ring->GetModuleCount(); ++m)
			{
				const uint32_t crc = ring->GetModuleCrc(m);
				if (crc == 0)
					continue;
				char nameBuf[128];
				const char* name = resolver ? resolver(crc, nameBuf, sizeof(nameBuf)) : nullptr;
				if (!name)
				{
					snprintf(nameBuf, sizeof(nameBuf), "0x%08x", crc);
					name = nameBuf;
				}
				ring->CopyRange(m, frame, 1, &value);
				if (!first)
					outJson.push_back(',');
				first = false;
				outJson += "{\"crc\":";
				AppendUnsigned(outJson, crc);
				outJson += ",\"name\":";
				AppendEscaped(outJson, name);
				outJson += ",\"ms\":";
				AppendNumber(outJson, "%.2f", CoarseRing::FromFixed(value));
				outJson.push_back('}');
			}
			outJson += "],\"sections\":{\"available\":false,\"reason\":"
				"\"sections need profiling on and a per-frame stream; not wired\"}}";
		}

		void PackPerfWindowJobs(const PerfJobsUnitInput* units, uint32_t unitCount, float seconds,
			PerfNameResolver resolver, std::string& outJson)
		{
			if (!(seconds >= 1.0f))
				seconds = 1.0f;
			if (seconds > 60.0f)
				seconds = 60.0f;

			bool anyOn = false;
			for (uint32_t i = 0; i < unitCount; ++i)
				anyOn = anyOn || units[i].perJobRecording;

			outJson.clear();
			outJson += "{\"bucketMs\":";
			AppendUnsigned(outJson, kJobsBucketMs);
			outJson += (unitCount == 0 || anyOn) ? ",\"perJobRecording\":true" : ",\"perJobRecording\":false";
			outJson += ",\"units\":[";
			for (uint32_t i = 0; i < unitCount; ++i)
			{
				if (i > 0)
					outJson.push_back(',');
				PackJobsUnit(units[i], seconds, resolver, outJson);
			}
			outJson += "],\"steps\":[";
			bool firstStep = true;
			for (uint32_t i = 0; i < unitCount; ++i)
			{
				for (uint32_t s = 0; s < units[i].stepCount && units[i].steps; ++s)
				{
					const StepSummary& st = units[i].steps[s];
					char nameBuf[128];
					const char* channel = resolver ? resolver(st.channel, nameBuf, sizeof(nameBuf)) : nullptr;
					if (!channel)
					{
						snprintf(nameBuf, sizeof(nameBuf), "0x%08x", st.channel);
						channel = nameBuf;
					}
					if (!firstStep)
						outJson.push_back(',');
					firstStep = false;
					outJson += "{\"unit\":";
					AppendEscaped(outJson, units[i].name);
					outJson += ",\"channel\":";
					AppendEscaped(outJson, channel);
					outJson += ",\"kickKey\":";      AppendUnsigned(outJson, st.kickKey);
					outJson += ",\"applyKey\":";     AppendUnsigned(outJson, st.applyKey);
					outJson += ",\"kick\":";         AppendUnsigned(outJson, st.kickNs);
					outJson += ",\"firstStart\":";   AppendUnsigned(outJson, st.firstStartNs);
					outJson += ",\"ready\":";        AppendUnsigned(outJson, st.readyNs);
					outJson += ",\"apply\":";        AppendUnsigned(outJson, st.applyNs);
					outJson += ",\"stallMs\":";      AppendNumber(outJson, "%.3f", st.stallMs);
					outJson += ",\"cpuMs\":";        AppendNumber(outJson, "%.3f", st.cpuMs);
					outJson += ",\"capWaitMs\":";    AppendNumber(outJson, "%.3f", st.capWaitMs);
					outJson += ",\"workerWaitMs\":"; AppendNumber(outJson, "%.3f", st.workerWaitMs);
					outJson += ",\"ranges\":";       AppendUnsigned(outJson, st.ranges);
					outJson.push_back('}');
				}
			}
			outJson += "]}";
		}

		void PackPerfJobsRecords(const char* unit, uint32_t unitCrc, uint64_t fromNs, uint64_t toNs,
			const JobRecordPacked* records, uint32_t count, bool truncated, bool perJobRecording,
			std::string& outJson)
		{
			std::vector<uint64_t> start(count);
			std::vector<uint32_t> dur(count), owner(count), tag(count), waitUs(count);
			std::vector<uint16_t> worker(count), range(count), rangeCount(count), flags(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				start[i] = records[i].startNs;
				dur[i] = records[i].durNs;
				owner[i] = records[i].owner;
				tag[i] = records[i].tag;
				waitUs[i] = records[i].waitUs;
				worker[i] = records[i].worker;
				range[i] = records[i].range;
				rangeCount[i] = records[i].rangeCount;
				flags[i] = records[i].flags;
			}

			outJson = "{\"v\":1,\"unit\":";
			AppendEscaped(outJson, unit);
			outJson += ",\"unitCrc\":";
			AppendUnsigned(outJson, unitCrc);
			outJson += ",\"fromNs\":";
			AppendUnsigned(outJson, fromNs);
			outJson += ",\"toNs\":";
			AppendUnsigned(outJson, toNs);
			outJson += perJobRecording ? ",\"perJobRecording\":true" : ",\"perJobRecording\":false";
			outJson += truncated ? ",\"truncated\":true" : ",\"truncated\":false";
			outJson += ",\"count\":";
			AppendUnsigned(outJson, count);
			outJson += ",\"startNs\":";   AppendRawBase64(outJson, start);
			outJson += ",\"durNs\":";     AppendRawBase64(outJson, dur);
			outJson += ",\"owner\":";     AppendRawBase64(outJson, owner);
			outJson += ",\"tag\":";       AppendRawBase64(outJson, tag);
			outJson += ",\"waitUs\":";    AppendRawBase64(outJson, waitUs);
			outJson += ",\"worker\":";    AppendRawBase64(outJson, worker);
			outJson += ",\"range\":";     AppendRawBase64(outJson, range);
			outJson += ",\"rangeCount\":"; AppendRawBase64(outJson, rangeCount);
			outJson += ",\"flags\":";     AppendRawBase64(outJson, flags);
			outJson.push_back('}');
		}
	}}
}
