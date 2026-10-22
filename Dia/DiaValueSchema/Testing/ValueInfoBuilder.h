#pragma once

// Test helper: builds a ValueInfo quickly. Ships with DiaValueSchema (Testing/ convention).

#include <DiaValueSchema/ValueInfo.h>

namespace Dia::ValueSchema::Testing
{
	inline ValueInfo MakeValueInfo(const char* forFormat, const char* key, ValueKind kind = ValueKind::Parameter, const char* sourceFile = "test.diavalues")
	{
		ValueInfo info;
		SetText(info.mFor, forFormat);
		SetText(info.mKey, key);
		info.mKind = kind;
		info.mType = ValueType::Float;
		SetText(info.mSourceFile, sourceFile);
		return info;
	}
}
