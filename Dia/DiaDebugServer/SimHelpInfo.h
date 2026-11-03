#ifndef DIA_DEBUG_SERVER_SIM_HELP_INFO_H
#define DIA_DEBUG_SERVER_SIM_HELP_INFO_H

#include <DiaAPI/CommandRegistry/ValueParam.h>

#include <cmath>
#include <cstdint>
#include <type_traits>

namespace Dia
{
	namespace DebugServer
	{
		// Help for one sim-hosted name (query or command). Strings are copied at registration;
		// the caller's storage need not outlive the call.
		struct SimHelpInfo
		{
			const char*                 description = nullptr;   // required, non-empty
			const char*                 category    = nullptr;   // optional, e.g. "cells"
			const char*                 owner       = nullptr;   // optional, e.g. "DiaCellLayers"
			const Dia::API::ValueParam* params      = nullptr;   // optional; same struct DiaAPI commands use
			unsigned int                paramCount  = 0;
			bool                        noArgs      = false;
		};

		// Fixed limits (maximum string length in bytes, excluding the NUL).
		static constexpr uint32_t kSimHelpMaxDescription      = 256;
		static constexpr uint32_t kSimHelpMaxCategory         = 64;
		static constexpr uint32_t kSimHelpMaxOwner            = 64;
		static constexpr uint32_t kSimHelpMaxParams           = 16;
		static constexpr uint32_t kSimHelpMaxParamName        = 64;
		static constexpr uint32_t kSimHelpMaxParamType        = 16;
		static constexpr uint32_t kSimHelpMaxParamDescription = 192;
		static constexpr uint32_t kSimHelpMaxParamUnit        = 16;
		static constexpr uint32_t kSimHelpMaxParamEnum        = 128;   // the whole "a|b|c" string of ValueParam::enumValues

		// Stored copy of one ValueParam. A null optional string is stored as an empty string.
		struct SimParamStored
		{
			char  name[kSimHelpMaxParamName + 1];
			char  type[kSimHelpMaxParamType + 1];
			char  description[kSimHelpMaxParamDescription + 1];
			char  unit[kSimHelpMaxParamUnit + 1];
			char  enumValues[kSimHelpMaxParamEnum + 1];
			bool  required;
			bool  hasRange;
			float min;
			float max;
		};

		// Stored copy of a SimHelpInfo: fixed buffers, no heap, trivially copyable (usable in tables and event payloads).
		// A null optional string is stored as an empty string.
		struct SimHelpStored
		{
			char           description[kSimHelpMaxDescription + 1];
			char           category[kSimHelpMaxCategory + 1];
			char           owner[kSimHelpMaxOwner + 1];
			SimParamStored params[kSimHelpMaxParams];
			uint32_t       paramCount;
			bool           noArgs;
		};

		static_assert(std::is_trivially_copyable_v<SimHelpStored>, "SimHelpStored must be trivially copyable");

		enum class SimHelpResult : uint32_t
		{
			Ok = 0,
			MissingDescription,
			DescriptionTooLong,
			CategoryTooLong,
			OwnerTooLong,
			TooManyParams,
			NullParams,
			ParamNameMissing,
			ParamNameTooLong,
			ParamTypeTooLong,
			ParamDescriptionTooLong,
			ParamUnitTooLong,
			ParamEnumTooLong,
			ParamRangeInvalid
		};

		// A param range is valid when both bounds are finite and min <= max.
		inline bool SimHelpRangeValid(float min, float max)
		{
			return std::isfinite(min) && std::isfinite(max) && min <= max;
		}

		// Short human-readable reason for logs.
		const char* SimHelpResultText(SimHelpResult result);

		// Validates 'info' and copies it into 'out'. Over-limit data is rejected, never truncated.
		// On any result other than Ok, 'out' is left unchanged.
		SimHelpResult CopyHelp(const SimHelpInfo& info, SimHelpStored& out);
	}
}

#endif
