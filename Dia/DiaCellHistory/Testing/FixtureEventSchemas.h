#pragma once

// Test helper (Testing/ convention): fixture dia.value_schema text with kind:event entries, plus a loader.
//   test.drought : payload { region: string, severity: float }, vocabulary { dry, severe }
//   test.simple  : no payload
//   test.flood   : payload { region: string }
// All three live under `for: test.events`.

#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <memory>

namespace Dia::CellHistory::Testing
{
	inline const char* FixtureEventSchemaText()
	{
		return R"JSON({
  "format": "dia.value_schema",
  "version": 1,
  "for": "test.events",
  "entries": [
    {
      "key": "test.drought", "kind": "event", "type": "string",
      "label": "Drought", "description": "A drought began in a region",
      "payload": [
        { "name": "region", "type": "string", "description": "Region name" },
        { "name": "severity", "type": "float", "description": "0..1 severity" }
      ],
      "vocabulary": [
        { "id": "dry", "label": "Dry", "description": "Mild" },
        { "id": "severe", "label": "Severe", "description": "Harsh" }
      ]
    },
    {
      "key": "test.simple", "kind": "event", "type": "string",
      "label": "Simple", "description": "An event with no payload"
    },
    {
      "key": "test.flood", "kind": "event", "type": "string",
      "label": "Flood", "description": "A flood hit a region",
      "payload": [
        { "name": "region", "type": "string", "description": "Region name" }
      ]
    }
  ]
})JSON";
	}

	// Loads the fixture entries into a published registry and exposes a service over it.
	class FixtureSchemaSet
	{
	public:
		// True when every entry loaded and registered.
		bool Load()
		{
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(FixtureEventSchemaText(), "fixture.diavalues");
			if (!r.mOk) { return false; }
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i)
			{
				if (mRegistry.Register(r.mEntries[i]) != Dia::ValueSchema::RegisterResult::Ok) { return false; }
			}
			mRegistry.Publish();
			mService = std::make_unique<Dia::ValueSchema::ValueSchemaService>(mRegistry);
			return true;
		}
		const Dia::ValueSchema::ValueSchemaService*	Service() const { return mService.get(); }

	private:
		Dia::ValueSchema::ValueSchemaRegistry						mRegistry;
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService>		mService;
	};
}
