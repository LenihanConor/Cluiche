#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Dia
{
	namespace Observation { namespace Profile
	{
		class UnitTimingTable;

		// Perf Inspector A4b: end-of-run perf summary (perf-summary.json). Pure data, no clock, no I/O
		// (except WritePerfSummary, which the shutdown path calls once).

		// One module's stats row. Survives the module (snapshotted when the module stops).
		struct PerfModuleRow
		{
			char     name[48];
			uint32_t crc;
			uint32_t entryCount;   // samples recorded for this module
			float    p50Ms;
			float    p95Ms;
			float    maxMs;
			float    ewmaMs;
		};

		// Fixed-capacity per-unit set of module rows, keyed by module CRC (upsert).
		// Past kCapacity new modules are dropped and counted (GetDropped()).
		class PerfSummaryAccumulator
		{
		public:
			static constexpr uint32_t kCapacity = 128;

			PerfSummaryAccumulator() { Reset(); }

			void Reset() { mCount = 0; mDropped = 0; }

			// Insert, or replace the existing row with the same crc. Returns false (and counts a drop)
			// if the row is new and the accumulator is full.
			bool Upsert(const PerfModuleRow& row);

			uint32_t             GetCount() const   { return mCount; }
			uint32_t             GetDropped() const { return mDropped; }
			const PerfModuleRow& GetRow(uint32_t i) const { return mRows[i]; }

		private:
			PerfModuleRow mRows[kCapacity];
			uint32_t      mCount;
			uint32_t      mDropped;
		};

		// Fills `out` from a table slot. A slot with no samples yields zeroed stats (the module still gets a row).
		void BuildPerfModuleRow(PerfModuleRow& out, const UnitTimingTable& table, uint32_t slot, const char* name, uint32_t crc);

		struct PerfUnitSummary
		{
			char     unit[48] = {};
			float    budgetMs = 0.0f;
			uint32_t entryCount = 0;
			float    unitP50Ms = 0.0f;
			float    unitP95Ms = 0.0f;
			float    unitMaxMs = 0.0f;
			float    unitEwmaMs = 0.0f;
			uint32_t tripCount = 0;
			uint32_t suppressedCount = 0;
			uint32_t overBudgetFrames = 0;
			PerfSummaryAccumulator modules;
		};

		struct PerfSummary
		{
			char buildConfig[16] = {};
			std::vector<PerfUnitSummary> units;
		};

		// Serialise to pretty JSON text.
		std::string SerializePerfSummary(const PerfSummary& summary);

		// Writes <sessionDir>/perf-summary.json. Returns false on I/O failure or empty dir.
		bool WritePerfSummary(const PerfSummary& summary, const char* sessionDir);

		// Gate: writes only when enabled (perf mode). Returns true iff a file was written.
		bool WritePerfSummaryIfEnabled(bool perfEnabled, const PerfSummary& summary, const char* sessionDir);
	}}
}
