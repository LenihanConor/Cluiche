#include <DiaValueSchema/ComponentFieldCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaEntity/ComponentDescribe.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <cstring>

namespace Dia::ValueSchema
{
	namespace
	{
		bool Empty(const char* s) { return s == nullptr || s[0] == '\0'; }
		bool Fits(const char* s, unsigned int maxLen) { return s == nullptr || std::strlen(s) <= maxLen; }

		void AddUndescribed(ValueList<UndescribedItem>& list, const char* name, const char* owner)
		{
			// SetText, never `str = cstr`: printf-style assignment.
			UndescribedItem item;
			SetText(item.mName, name);
			SetText(item.mOwner, owner);
			if (list.Size() == list.Capacity()) { list.Reserve(list.Size() + 16); }
			list.Add(item);
		}

		// Chooses the ValueType and fills the default; false when the info has no usable default.
		void ApplyTypeAndDefault(const Dia::Entity::ComponentFieldInfo& f, ValueInfo& info)
		{
			using Dia::Entity::FieldKind;
			const Json::Value& d = f.defaultValue;

			if (f.kind == FieldKind::Nested)
			{
				info.mType = ValueType::String;
				info.mUi["nested"] = true;
				return;
			}

			if (f.kind == FieldKind::StringId)
			{
				info.mType = ValueType::String;
				if (f.hasDefault && d.isString()) { info.mHasDefault = true; SetText(info.mDefaultString, d.asCString()); }
				return;
			}

			if (f.hasDefault && !d.isNull())
			{
				if (d.isBool())
				{
					info.mType = ValueType::Bool; info.mHasDefault = true; info.mDefaultNumber = d.asBool() ? 1.0 : 0.0;
				}
				else if (d.isInt() || d.isUInt() || d.isInt64() || d.isUInt64())
				{
					info.mType = ValueType::Int; info.mHasDefault = true; info.mDefaultNumber = d.asDouble();
				}
				else if (d.isDouble())
				{
					info.mType = ValueType::Float; info.mHasDefault = true; info.mDefaultNumber = d.asDouble();
				}
				else if (d.isString())
				{
					info.mType = ValueType::String; info.mHasDefault = true; SetText(info.mDefaultString, d.asCString());
				}
				else
				{
					info.mType = ValueType::String;	// arrays / objects (math types, containers): no scalar default
				}
				return;
			}

			info.mType = ValueType::Float;
		}
	}

	ComponentFieldReport ComponentFieldCollector::Collect(const Dia::Entity::ComponentTypeDesc* const* descs,
		unsigned int count, ValueSchemaRegistry& registry)
	{
		ComponentFieldReport rep;
		if (descs == nullptr) { return rep; }

		Dia::Entity::ComponentFieldInfo* fields = new Dia::Entity::ComponentFieldInfo[kMaxFieldsPerComponent];
		for (unsigned int c = 0; c < count; ++c)
		{
			const Dia::Entity::ComponentTypeDesc* desc = descs[c];
			if (desc == nullptr) { continue; }
			const unsigned int n = Dia::Entity::DescribeComponentFields(*desc, fields, kMaxFieldsPerComponent);
			for (unsigned int i = 0; i < n; ++i)
			{
				const Dia::Entity::ComponentFieldInfo& f = fields[i];
				if (Empty(f.typeId) || Empty(f.name)
					|| !Fits(f.typeId, ValueInfo::kMaxForLength) || !Fits(f.name, ValueInfo::kMaxKeyLength)
					|| !Fits(f.label, ValueInfo::kMaxLabelLength) || !Fits(f.description, ValueInfo::kMaxDescriptionLength)
					|| !Fits(f.unit, ValueInfo::kMaxUnitLength) || !Fits(f.hint, ValueInfo::kMaxHintLength))
				{
					++rep.mRejectedCount;
					DIA_LOG_ERROR("VSC", "Component field '%s/%s' rejected: empty name or text exceeds ValueInfo capacity",
						f.typeId ? f.typeId : "", f.name ? f.name : "");
					continue;
				}

				ValueInfo info;
				SetText(info.mFor, f.typeId);
				SetText(info.mKey, f.name);
				info.mKind = ValueKind::ComponentField;
				SetText(info.mLabel, f.label);
				SetText(info.mDescription, f.description);
				SetText(info.mUnit, f.unit);
				SetText(info.mHint, f.hint);
				SetText(info.mSourceFile, "code:ComponentRegistry");
				if (f.hasRange)
				{
					info.mHasMin = true; info.mHasMax = true;
					info.mMin = f.min; info.mMax = f.max;
				}
				ApplyTypeAndDefault(f, info);

				const RegisterResult r = registry.Register(info);
				if (r == RegisterResult::DuplicateKey) { ++rep.mDuplicateCount; continue; }
				if (r != RegisterResult::Ok)
				{
					++rep.mRejectedCount;
					DIA_LOG_ERROR("VSC", "Component field '%s/%s' rejected by the registry (result %d)",
						f.typeId, f.name, static_cast<int>(r));
					continue;
				}

				++rep.mCollected;
				if (f.described)
				{
					++rep.mDescribed;
				}
				else if (f.hasRange)
				{
					++rep.mUndescribed;
					char full[ValueInfo::kMaxFullKeyLength + 1];
					snprintf(full, sizeof(full), "%s/%s", f.typeId, f.name);
					AddUndescribed(rep.mUndescribedFields, full, f.typeId);
				}
			}
		}
		delete[] fields;
		return rep;
	}

	ComponentFieldReport ComponentFieldCollector::Collect(ValueSchemaRegistry& registry)
	{
		Dia::Entity::ComponentRegistry& components = Dia::Entity::ComponentRegistry::Get();
		const unsigned int count = components.GetCount();
		const Dia::Entity::ComponentTypeDesc** descs = new const Dia::Entity::ComponentTypeDesc*[count > 0 ? count : 1];
		for (unsigned int i = 0; i < count; ++i) { descs[i] = &components.GetByIndex(i); }
		ComponentFieldReport rep = Collect(descs, count, registry);
		delete[] descs;
		return rep;
	}
}
