#pragma once

// ValueInfo: one schema entry (parameter, field, metric, arg, command, event, component_field).
//
// Capacity policy: all text fields are fixed-size DiaCore strings. Nothing is silently truncated
// by this module; the loader (later task) must reject input longer than the k*Length constants.
//   for          : kMaxForLength (63)         key : kMaxKeyLength (127)
//   full key     : kMaxFullKeyLength (255)    "<for>/<key>"
//   label        : kMaxLabelLength (127)
//   description  : kMaxDescriptionLength (1023)
//   hint         : kMaxHintLength (511)
//   unit         : kMaxUnitLength (31)
//   ref / source : kMaxRefLength (127), kMaxSourceLength (127); sourceFile kMaxSourceFileLength (255)
// Lengths are in characters, excluding the terminator.

#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Strings/String1024.h>
#include <DiaCore/Containers/Arrays/DynamicArray.h>
#include <json/json.h>

namespace Dia::ValueSchema
{
	// DynamicArray with safe copy semantics (DynamicArray's own copy asserts on empty sources).
	// Elements are copied one by one via Add. Call Reserve(n) before Add.
	template<class T>
	class ValueList : public Dia::Core::Containers::DynamicArray<T>
	{
	public:
		ValueList() = default;
		ValueList(const ValueList& other) { CopyFrom(other); }
		ValueList& operator=(const ValueList& other)
		{
			if (this != &other) { this->RemoveAll(); CopyFrom(other); }
			return *this;
		}
	private:
		void CopyFrom(const ValueList& other)
		{
			if (other.Size() > this->Capacity()) { this->Reserve(other.Size()); }
			for (unsigned int i = 0; i < other.Size(); ++i) { this->Add(other[i]); }
		}
	};
	enum class ValueKind
	{
		Parameter, Field, Metric, Arg, Command, Event, ComponentField
	};

	enum class ValueType
	{
		Float, Int, Bool, Enum, String, Ref, RefList, Map
	};

	struct EnumValue
	{
		Dia::Core::Containers::String64		mId;
		Dia::Core::Containers::String128	mLabel;
		Dia::Core::Containers::String512	mDescription;
		Dia::Core::Containers::String32		mColour;
		bool								mHasColour = false;
	};

	struct PayloadField
	{
		Dia::Core::Containers::String64		mName;
		ValueType							mType = ValueType::Float;
		Dia::Core::Containers::String512	mDescription;
		Dia::Core::Containers::String32		mUnit;
	};

	struct VocabularyEntry
	{
		Dia::Core::Containers::String64		mId;
		Dia::Core::Containers::String128	mLabel;
		Dia::Core::Containers::String512	mDescription;
		Dia::Core::Containers::String32		mUnit;
	};

	struct ValueInfo
	{
		static constexpr unsigned int kMaxForLength			= 63;
		static constexpr unsigned int kMaxKeyLength			= 127;
		static constexpr unsigned int kMaxFullKeyLength		= 255;
		static constexpr unsigned int kMaxLabelLength		= 127;
		static constexpr unsigned int kMaxDescriptionLength	= 1023;
		static constexpr unsigned int kMaxHintLength		= 511;
		static constexpr unsigned int kMaxUnitLength		= 31;
		static constexpr unsigned int kMaxRefLength			= 127;
		static constexpr unsigned int kMaxSourceLength		= 127;
		static constexpr unsigned int kMaxSourceFileLength	= 255;

		Dia::Core::Containers::String256	mFullKey;		// "<for>/<key>"; filled by the registry on Register
		Dia::Core::Containers::String64		mFor;
		Dia::Core::Containers::String128	mKey;
		ValueKind							mKind = ValueKind::Parameter;
		Dia::Core::Containers::String128	mLabel;
		Dia::Core::Containers::String1024	mDescription;
		Dia::Core::Containers::String512	mHint;
		Dia::Core::Containers::String32		mUnit;
		ValueType							mType = ValueType::Float;

		bool								mHasMin = false;
		bool								mHasMax = false;
		double								mMin = 0.0;
		double								mMax = 0.0;
		bool								mHasDefault = false;
		double								mDefaultNumber = 0.0;	// float/int/bool (0/1)
		Dia::Core::Containers::String128	mDefaultString;			// string/enum/ref

		ValueList<EnumValue>							mEnumValues;
		ValueList<Dia::Core::Containers::String128>	mAffects;
		ValueList<Dia::Core::Containers::String128>	mRelated;
		Dia::Core::Containers::String128	mSource;
		bool								mUnique = false;
		bool								mRequired = false;
		Dia::Core::Containers::String128	mRef;

		// map type
		Dia::Core::Containers::String128	mMapKeyRef;
		ValueType							mMapValueType = ValueType::Float;

		// record paths like "species[].growth_rate": stored as written (mKey) and split here.
		ValueList<Dia::Core::Containers::String64>	mPathSegments;

		Json::Value							mUi;		// open blocks
		Json::Value							mHistory;

		ValueList<PayloadField>		mPayload;		// events
		ValueList<VocabularyEntry>	mVocabulary;

		Dia::Core::Containers::String256	mSourceFile;	// file the entry came from (diagnostics)
	};
}
