#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>
#include <string>

namespace Dia::ValueSchema
{
	namespace
	{
		typedef Dia::Core::Containers::String128 Str128;

		bool Empty(const char* s) { return s == nullptr || s[0] == '\0'; }
		bool Fits(const char* s, unsigned int maxLen) { return s == nullptr || std::strlen(s) <= maxLen; }
		const char* Safe(const char* s) { return s ? s : ""; }

		template<class T>
		void Append(ValueList<T>& list, const T& item)
		{
			if (list.Size() == list.Capacity()) { list.Reserve(list.Size() + 16); }
			list.Add(item);
		}

		void AddUndescribed(ValueList<UndescribedItem>& list, const char* name, const char* owner)
		{
			// SetText, never `str = cstr`: String128/String64 assignment from const char* is printf-style.
			UndescribedItem item;
			SetText(item.mName, name);
			SetText(item.mOwner, owner);
			Append(list, item);
		}

		struct Ctx
		{
			ValueSchemaRegistry*	mRegistry;
			CodeSideReport*			mReport;
		};

		// Returns true when registered. Duplicates and other rejections are counted, never fatal.
		bool RegisterCounted(Ctx& ctx, const ValueInfo& info)
		{
			const RegisterResult r = ctx.mRegistry->Register(info);
			if (r == RegisterResult::Ok) { return true; }
			if (r == RegisterResult::DuplicateKey)
			{
				++ctx.mReport->mDuplicateCount;
			}
			else
			{
				++ctx.mReport->mRejectedCount;
				DIA_LOG_ERROR("VSC", "Code-side entry '%s' rejected by the registry (result %d)", info.mKey.AsCStr(), static_cast<int>(r));
			}
			return false;
		}

		void OnMetric(const CodeSideMetric& m, void* user)
		{
			Ctx& ctx = *static_cast<Ctx*>(user);
			CodeSideReport& rep = *ctx.mReport;
			if (Empty(m.mName)) { ++rep.mRejectedCount; DIA_LOG_ERROR("VSC", "Metric with empty name skipped"); return; }

			const char* label = (m.mHasInfo && !Empty(m.mLabel)) ? m.mLabel : m.mName;
			const bool described = m.mHasInfo && !Empty(m.mDescription);
			// Description and unit only exist when the metric carries info.
			const char* description = m.mHasInfo ? m.mDescription : nullptr;
			const char* unit = m.mHasInfo ? m.mUnit : nullptr;
			if (!Fits(m.mName, ValueInfo::kMaxKeyLength) || !Fits(label, ValueInfo::kMaxLabelLength)
				|| !Fits(description, ValueInfo::kMaxDescriptionLength) || !Fits(unit, ValueInfo::kMaxUnitLength))
			{
				++rep.mRejectedCount;
				DIA_LOG_ERROR("VSC", "Metric '%s' rejected: text exceeds ValueInfo capacity", m.mName);
				return;
			}

			ValueInfo info;
			SetText(info.mFor, CodeSideCollector::kMetricFor);
			SetText(info.mKey, m.mName);
			info.mKind = ValueKind::Metric;
			info.mType = ValueType::Float;
			SetText(info.mLabel, label);
			SetText(info.mDescription, description);
			SetText(info.mUnit, unit);
			SetText(info.mSourceFile, "code:MetricRegistry");

			if (!RegisterCounted(ctx, info)) { return; }
			++rep.mMetricsCollected;
			if (described)
			{
				++rep.mMetricsDescribed;
			}
			else
			{
				++rep.mMetricsUndescribed;
				char owner[64];
				const char* dot = std::strchr(m.mName, '.');
				const size_t n = dot ? static_cast<size_t>(dot - m.mName) : std::strlen(m.mName);
				const size_t c = n < sizeof(owner) - 1 ? n : sizeof(owner) - 1;
				std::memcpy(owner, m.mName, c);
				owner[c] = '\0';
				AddUndescribed(rep.mUndescribedMetrics, m.mName, owner);
			}
		}

		bool ParseType(const char* t, ValueType& out)
		{
			if (t == nullptr) { return false; }
			if (std::strcmp(t, "string") == 0) { out = ValueType::String; return true; }
			if (std::strcmp(t, "int") == 0)    { out = ValueType::Int;    return true; }
			if (std::strcmp(t, "float") == 0)  { out = ValueType::Float;  return true; }
			if (std::strcmp(t, "bool") == 0)   { out = ValueType::Bool;   return true; }
			if (std::strcmp(t, "enum") == 0)   { out = ValueType::Enum;   return true; }
			return false;
		}

		// Builds the arg ValueInfo; false (with an Error) when it exceeds capacity.
		bool BuildArg(const Dia::API::ValueParam& p, const std::string& key, ValueInfo& info, bool& badType)
		{
			ValueType type = ValueType::String;
			badType = !ParseType(p.type, type);
			if (badType) { type = ValueType::String; }

			if (key.size() > ValueInfo::kMaxKeyLength || !Fits(p.name, ValueInfo::kMaxLabelLength)
				|| !Fits(p.description, ValueInfo::kMaxDescriptionLength) || !Fits(p.unit, ValueInfo::kMaxUnitLength))
			{
				DIA_LOG_ERROR("VSC", "Command arg '%s' rejected: text exceeds ValueInfo capacity", key.c_str());
				return false;
			}

			info = ValueInfo();
			SetText(info.mFor, CodeSideCollector::kCommandFor);
			SetText(info.mKey, key.c_str());
			info.mKind = ValueKind::Arg;
			info.mType = type;
			SetText(info.mLabel, p.name);
			SetText(info.mDescription, p.description);
			SetText(info.mUnit, p.unit);
			info.mRequired = p.required;
			SetText(info.mSourceFile, "code:CommandRegistry");
			if (p.hasRange)
			{
				info.mHasMin = true; info.mHasMax = true;
				info.mMin = p.min; info.mMax = p.max;
			}
			if (type == ValueType::Enum && !Empty(p.enumValues))
			{
				const char* s = p.enumValues;
				while (true)
				{
					const char* bar = std::strchr(s, '|');
					const size_t n = bar ? static_cast<size_t>(bar - s) : std::strlen(s);
					if (n > 0)
					{
						if (n > 63)
						{
							DIA_LOG_ERROR("VSC", "Command arg '%s' rejected: enum value exceeds capacity", key.c_str());
							return false;
						}
						const std::string id(s, n);
						EnumValue ev;
						SetText(ev.mId, id.c_str());
						SetText(ev.mLabel, id.c_str());
						Append(info.mEnumValues, ev);
					}
					if (!bar) { break; }
					s = bar + 1;
				}
			}
			return true;
		}

		void OnCommand(const CodeSideCommand& c, void* user)
		{
			Ctx& ctx = *static_cast<Ctx*>(user);
			CodeSideReport& rep = *ctx.mReport;
			if (Empty(c.mName) || !Fits(c.mName, 63) || !Fits(c.mDescription, ValueInfo::kMaxDescriptionLength))
			{
				++rep.mRejectedCount;
				DIA_LOG_ERROR("VSC", "Command '%s' rejected: empty name or text exceeds ValueInfo capacity", Safe(c.mName));
				return;
			}

			const char* owner = Safe(c.mOwner);
			const bool hasParams = c.mParams != nullptr && c.mParamCount > 0;

			// Build arg entries first so the command entry can list their keys.
			ValueList<ValueInfo> args;
			unsigned int badTypeCount = 0;
			if (hasParams)
			{
				for (unsigned int i = 0; i < c.mParamCount; ++i)
				{
					const Dia::API::ValueParam& p = c.mParams[i];
					if (Empty(p.name))
					{
						++rep.mRejectedCount;
						DIA_LOG_ERROR("VSC", "Command '%s' has a param with no name; skipped", c.mName);
						continue;
					}
					const std::string key = std::string(c.mName) + "." + p.name;
					ValueInfo info;
					bool badType = false;
					if (!BuildArg(p, key, info, badType)) { ++rep.mRejectedCount; continue; }
					if (badType) { ++badTypeCount; }
					Append(args, info);
				}
			}

			ValueInfo cmd;
			SetText(cmd.mFor, CodeSideCollector::kCommandFor);
			SetText(cmd.mKey, c.mName);
			cmd.mKind = ValueKind::Command;
			cmd.mType = ValueType::String;
			SetText(cmd.mLabel, c.mName);
			SetText(cmd.mDescription, c.mDescription);
			SetText(cmd.mSourceFile, "code:CommandRegistry");
			cmd.mUi["owner"] = owner;
			cmd.mUi["category"] = Safe(c.mCategory);
			cmd.mUi["example"] = Safe(c.mExample);
			if (c.mNoArgs) { cmd.mUi["no_args"] = true; }
			// Each key once: a repeated param name produces a duplicate arg (counted when registered) but one related key.
			for (unsigned int i = 0; i < args.Size(); ++i)
			{
				bool listed = false;
				for (unsigned int j = 0; j < cmd.mRelated.Size() && !listed; ++j)
				{
					listed = std::strcmp(cmd.mRelated[j].AsCStr(), args[i].mKey.AsCStr()) == 0;
				}
				if (!listed)
				{
					const Str128 k = args[i].mKey;
					Append(cmd.mRelated, k);
				}
			}

			if (!RegisterCounted(ctx, cmd)) { return; }	// duplicate: first wins, its args are skipped too
			++rep.mCommandsCollected;

			if (badTypeCount > 0)
			{
				++rep.mWarningCount;
				DIA_LOG_WARNING("VSC", "Command '%s': %u param(s) with an unknown type; treated as string", c.mName, badTypeCount);
			}
			if (!hasParams && !c.mNoArgs)
			{
				AddUndescribed(rep.mUndescribedCommands, c.mName, owner);
			}

			for (unsigned int i = 0; i < args.Size(); ++i)
			{
				if (!RegisterCounted(ctx, args[i])) { continue; }
				++rep.mArgsCollected;
				if (!Empty(args[i].mDescription.AsCStr()))
				{
					++rep.mArgsDescribed;
				}
				else
				{
					++rep.mArgsUndescribed;
					AddUndescribed(rep.mUndescribedParams, args[i].mKey.AsCStr(), owner);
				}
			}
		}

		struct MetricCtx { ICodeSideSource::MetricVisitor mVisitor; void* mUser; };

		void LiveMetric(const Dia::Core::StringCRC& name, Dia::Observation::Metric::MetricKind,
			const Dia::Observation::Metric::MetricInfo* info, void* user)
		{
			MetricCtx* ctx = static_cast<MetricCtx*>(user);
			CodeSideMetric m;
			m.mName = name.AsChar();
			if (info)
			{
				m.mHasInfo = true;
				m.mLabel = info->label;
				m.mDescription = info->description;
				m.mUnit = info->unit;
			}
			ctx->mVisitor(m, ctx->mUser);
		}
	}

	void LiveCodeSideSource::ForEachMetric(MetricVisitor visitor, void* user)
	{
		MetricCtx ctx{ visitor, user };
		Dia::Observation::Metric::MetricRegistry::Instance().ForEachMetric(&LiveMetric, &ctx);
	}

	void LiveCodeSideSource::ForEachCommand(CommandVisitor visitor, void* user)
	{
		// std::function members make these arrays large; keep them off the stack.
		auto* plain = new Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 256>();
		Dia::API::ListCommandInfos(*plain);
		for (unsigned int i = 0; i < plain->Size(); ++i)
		{
			const Dia::API::CommandInfo& ci = (*plain)[i];
			CodeSideCommand c;
			c.mName = ci.name.AsChar();
			c.mDescription = ci.description;
			c.mCategory = ci.category.AsChar();
			c.mOwner = ci.owner;
			c.mExample = ci.example;
			c.mParams = ci.params;
			c.mParamCount = ci.paramCount;
			c.mNoArgs = ci.noArgs;
			visitor(c, user);
		}
		delete plain;

		auto* json = new Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfoJson, 256>();
		Dia::API::ListJsonCommandInfos(*json);
		for (unsigned int i = 0; i < json->Size(); ++i)
		{
			const Dia::API::CommandInfoJson& ci = (*json)[i];
			CodeSideCommand c;
			c.mName = ci.name.AsChar();
			c.mDescription = ci.description;
			c.mCategory = ci.category.AsChar();
			c.mOwner = ci.owner;
			c.mParams = ci.params;
			c.mParamCount = ci.paramCount;
			c.mNoArgs = ci.noArgs;
			visitor(c, user);
		}
		delete json;
	}

	unsigned int LiveCodeSideSource::MetricGeneration()
	{
		return Dia::Observation::Metric::MetricRegistry::Instance().Generation();
	}

	unsigned int LiveCodeSideSource::CommandGeneration()
	{
		return Dia::API::Generation();
	}

	CodeSideReport CodeSideCollector::Collect(ICodeSideSource& source, ValueSchemaRegistry& registry)
	{
		CodeSideReport report;
		report.mMetricGeneration = source.MetricGeneration();
		report.mCommandGeneration = source.CommandGeneration();
		Ctx ctx{ &registry, &report };
		source.ForEachMetric(&OnMetric, &ctx);
		source.ForEachCommand(&OnCommand, &ctx);
		return report;
	}
}
