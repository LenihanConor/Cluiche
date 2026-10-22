#pragma once

// Shared fixtures for the exhaustive ramp tests (resolver, legend, integration). Header-only, test-private.

#include <gtest/gtest.h>
#include "ValueSchemaTestSupport.h"
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/RampLegend.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/RampResolver.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <memory>
#include <string>

namespace VsTest
{
	inline std::string ReadShippedRampsText(const char* sourceFile)
	{
		std::string text;
		ReadFileBytes(FindUpwards(sourceFile, "Dia/DiaValueSchema/Data/ramps.json"), text);
		return text;
	}

	// Published schema registry (empty) + ramp registry + service. Ramps: the real shipped set by default.
	struct RampFixture
	{
		Dia::ValueSchema::ValueSchemaRegistry					mSchema;
		Dia::ValueSchema::RampRegistry							mRamps;
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService>	mService;

		// shipped = load ramps.json except `skipId` (null = keep all). finish = publish now.
		explicit RampFixture(bool shipped = true, bool finish = true, const char* skipId = nullptr, const char* sourceFile = __FILE__)
		{
			if (shipped)
			{
				const std::string text = ReadShippedRampsText(sourceFile);
				Dia::ValueSchema::RampLoadResult r = Dia::ValueSchema::RampLoader::LoadFromText(text.c_str(), "ramps.json");
				EXPECT_TRUE(r.mOk);
				for (unsigned i = 0; i < r.mRamps.Size(); ++i)
				{
					if (skipId && std::string(skipId) == r.mRamps[i].mName.AsCStr()) { continue; }
					mRamps.Register(r.mRamps[i], true, "ramps.json");
				}
			}
			if (finish) { Finish(); }
		}
		void Finish()
		{
			mSchema.Publish();
			mRamps.Publish();
			mService.reset(new Dia::ValueSchema::ValueSchemaService(mSchema, &mRamps));
		}
		const Dia::ValueSchema::ValueSchemaService& Svc() const { return *mService; }
	};

	inline Dia::ValueSchema::ValueInfo FloatEntry(const char* key, const char* ramp = nullptr)
	{
		Dia::ValueSchema::ValueInfo info = Dia::ValueSchema::Testing::MakeValueInfo("test", key);
		Dia::ValueSchema::SetText(info.mFullKey, (std::string("test/") + key).c_str());
		if (ramp) { info.mUi["ramp"] = ramp; }
		return info;
	}

	// Enum entry with `count` values v0..vN-1; bit i of givenMask (i < 32) gives value i the colour #102030.
	inline Dia::ValueSchema::ValueInfo EnumEntry(const char* key, unsigned count, unsigned givenMask = 0)
	{
		using namespace Dia::ValueSchema;
		ValueInfo info = Testing::MakeValueInfo("test", key);
		SetText(info.mFullKey, (std::string("test/") + key).c_str());
		info.mType = ValueType::Enum;
		info.mEnumValues.Reserve(count > 0 ? count : 1);
		for (unsigned i = 0; i < count; ++i)
		{
			EnumValue v;
			SetText(v.mId, ("v" + std::to_string(i)).c_str());
			if (i < 32 && (givenMask & (1u << i))) { SetText(v.mColour, "#102030"); v.mHasColour = true; }
			info.mEnumValues.Add(v);
		}
		return info;
	}
}
