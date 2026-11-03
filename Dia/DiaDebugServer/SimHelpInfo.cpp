#include "DiaDebugServer/SimHelpInfo.h"

#include <cstring>
#include <memory>

namespace Dia
{
	namespace DebugServer
	{
		namespace
		{
			// Length of s, or kBad if s is longer than max. Null is length 0.
			constexpr size_t kBad = static_cast<size_t>(-1);
			size_t BoundedLength(const char* s, uint32_t max)
			{
				if (s == nullptr)
					return 0;
				const char* nul = static_cast<const char*>(std::memchr(s, 0, static_cast<size_t>(max) + 1));
				return nul ? static_cast<size_t>(nul - s) : kBad;
			}

			template <size_t N>
			void CopyText(char (&dst)[N], const char* src)
			{
				const size_t len = (src != nullptr) ? std::strlen(src) : 0;   // already validated <= N-1
				if (len > 0)
					std::memcpy(dst, src, len);
				dst[len] = '\0';
			}
		}

		const char* SimHelpResultText(SimHelpResult result)
		{
			switch (result)
			{
			case SimHelpResult::Ok:                      return "ok";
			case SimHelpResult::MissingDescription:      return "missing or empty description";
			case SimHelpResult::DescriptionTooLong:      return "description over 256 bytes";
			case SimHelpResult::CategoryTooLong:         return "category over 64 bytes";
			case SimHelpResult::OwnerTooLong:            return "owner over 64 bytes";
			case SimHelpResult::TooManyParams:           return "more than 16 params";
			case SimHelpResult::NullParams:              return "paramCount > 0 but params is null";
			case SimHelpResult::ParamNameMissing:        return "param with missing or empty name";
			case SimHelpResult::ParamNameTooLong:        return "param name over 64 bytes";
			case SimHelpResult::ParamTypeTooLong:        return "param type over 16 bytes";
			case SimHelpResult::ParamDescriptionTooLong: return "param description over 192 bytes";
			case SimHelpResult::ParamUnitTooLong:        return "param unit over 16 bytes";
			case SimHelpResult::ParamEnumTooLong:        return "param enum list over 128 bytes";
			}
			return "unknown";
		}

		SimHelpResult CopyHelp(const SimHelpInfo& info, SimHelpStored& out)
		{
			if (info.description == nullptr || info.description[0] == '\0')
				return SimHelpResult::MissingDescription;
			if (BoundedLength(info.description, kSimHelpMaxDescription) == kBad)
				return SimHelpResult::DescriptionTooLong;
			if (BoundedLength(info.category, kSimHelpMaxCategory) == kBad)
				return SimHelpResult::CategoryTooLong;
			if (BoundedLength(info.owner, kSimHelpMaxOwner) == kBad)
				return SimHelpResult::OwnerTooLong;
			if (info.paramCount > kSimHelpMaxParams)
				return SimHelpResult::TooManyParams;
			if (info.paramCount > 0 && info.params == nullptr)
				return SimHelpResult::NullParams;

			for (unsigned int i = 0; i < info.paramCount; ++i)
			{
				const Dia::API::ValueParam& p = info.params[i];
				if (p.name == nullptr || p.name[0] == '\0')
					return SimHelpResult::ParamNameMissing;
				if (BoundedLength(p.name, kSimHelpMaxParamName) == kBad)
					return SimHelpResult::ParamNameTooLong;
				if (BoundedLength(p.type, kSimHelpMaxParamType) == kBad)
					return SimHelpResult::ParamTypeTooLong;
				if (BoundedLength(p.description, kSimHelpMaxParamDescription) == kBad)
					return SimHelpResult::ParamDescriptionTooLong;
				if (BoundedLength(p.unit, kSimHelpMaxParamUnit) == kBad)
					return SimHelpResult::ParamUnitTooLong;
				if (BoundedLength(p.enumValues, kSimHelpMaxParamEnum) == kBad)
					return SimHelpResult::ParamEnumTooLong;
			}

			// Everything validated: copy (out is only written on success).
			std::memset(&out, 0, sizeof(out));
			CopyText(out.description, info.description);
			CopyText(out.category, info.category);
			CopyText(out.owner, info.owner);
			out.noArgs = info.noArgs;
			out.paramCount = info.paramCount;
			for (unsigned int i = 0; i < info.paramCount; ++i)
			{
				const Dia::API::ValueParam& p = info.params[i];
				SimParamStored& d = out.params[i];
				CopyText(d.name, p.name);
				CopyText(d.type, p.type);
				CopyText(d.description, p.description);
				CopyText(d.unit, p.unit);
				CopyText(d.enumValues, p.enumValues);
				d.required = p.required;
				d.hasRange = p.hasRange;
				d.min = p.min;
				d.max = p.max;
			}
			return SimHelpResult::Ok;
		}
	}
}
