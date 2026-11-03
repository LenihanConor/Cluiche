#include <gtest/gtest.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaCellHistory/Testing/FixtureEventSchemas.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <memory>
#include <string>

using namespace Dia::CellHistory;
using Dia::CellLayers::Testing::LogCapture;
using Dia::Core::StringCRC;

namespace
{
	// Extra schema with an enum payload field that uses the event vocabulary.
	const char* kAlertSchema = R"JSON({
  "format": "dia.value_schema", "version": 1, "for": "test.alerts",
  "entries": [
    { "key": "test.alert", "kind": "event", "type": "string", "label": "Alert", "description": "d",
      "payload": [ { "name": "level", "type": "enum", "description": "d" }, { "name": "on", "type": "bool", "description": "d" },
                   { "name": "n", "type": "int", "description": "d" } ],
      "vocabulary": [ { "id": "low", "label": "Low", "description": "d" }, { "id": "high", "label": "High", "description": "d" } ] }
  ]
})JSON";

	struct Rig
	{
		Dia::CellLayers::CellLayerRegistry registry;
		Dia::CellHistory::Testing::FixtureSchemaSet fixture;
		Dia::ValueSchema::ValueSchemaRegistry alertReg;
		std::unique_ptr<Dia::ValueSchema::ValueSchemaService> alertService;
		std::unique_ptr<CellHistoryRecorder> rec;
		HistoryConfig cfg;

		void Start(bool withSchema = true)
		{
			ASSERT_TRUE(fixture.Load());
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(kAlertSchema, "alert.json");
			ASSERT_TRUE(r.mOk);
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i) { alertReg.Register(r.mEntries[i]); }
			alertReg.Publish();
			alertService = std::make_unique<Dia::ValueSchema::ValueSchemaService>(alertReg);
			rec = std::make_unique<CellHistoryRecorder>(registry, withSchema ? fixture.Service() : nullptr, cfg, 1);
		}
		void StartAlert()
		{
			ASSERT_TRUE(fixture.Load());
			Dia::ValueSchema::LoadResult r = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(kAlertSchema, "alert.json");
			ASSERT_TRUE(r.mOk);
			for (uint32_t i = 0; i < r.mEntries.Size(); ++i) { alertReg.Register(r.mEntries[i]); }
			alertReg.Publish();
			alertService = std::make_unique<Dia::ValueSchema::ValueSchemaService>(alertReg);
			rec = std::make_unique<CellHistoryRecorder>(registry, alertService.get(), cfg, 1);
		}
		EmitResult Emit(const char* type, const char* payload)
		{
			HistoryEvent e;
			e.type = StringCRC(type);
			e.payloadJson = payload;
			return rec->Emit(e);
		}
		uint32_t Pending() const { return rec->EventsLog().PendingCount(); }
	};

	// Validation is compiled out of release builds; these tests assert the debug behaviour.
#ifdef DIA_DEBUG
	const bool kValidates = true;
#else
	const bool kValidates = false;
#endif
	unsigned W(bool expectIfValidating) { return (kValidates && expectIfValidating) ? 1u : 0u; }
}

TEST(EventPayloadValidation, MatchingPayloadDoesNotWarn)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	EXPECT_EQ(r.Emit("test.drought", "{\"region\":\"north\",\"severity\":0.5}"), EmitResult::Ok);
	EXPECT_EQ(log.Warnings(), 0u);
	EXPECT_EQ(r.rec->EventTypeWarningCount(), 0u);
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, UnknownTypeWarnsOncePerTypeAndStillRecords)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.nope", nullptr);
	EXPECT_EQ(log.Warnings("test.nope"), W(true));
	r.Emit("test.nope", nullptr);
	EXPECT_EQ(log.Warnings("test.nope"), W(true));
	r.Emit("test.other", nullptr);
	EXPECT_EQ(log.Warnings("test.other"), W(true));
	EXPECT_EQ(r.rec->EventTypeWarningCount(), kValidates ? 2u : 0u);
	EXPECT_EQ(r.Pending(), 3u);
}

TEST(EventPayloadValidation, WrongFieldTypeWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.drought", "{\"region\":5}");
	EXPECT_EQ(log.Warnings("test.drought"), W(true));
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, WrongNumericFieldTypeWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.drought", "{\"severity\":\"high\"}");
	EXPECT_EQ(log.Warnings("test.drought"), W(true));
}

TEST(EventPayloadValidation, UndeclaredFieldWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.flood", "{\"region\":\"x\",\"extra\":1}");
	EXPECT_EQ(log.Warnings("test.flood"), W(true));
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, UnparseableJsonWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.flood", "{\"region\":");
	EXPECT_EQ(log.Warnings("test.flood"), W(true));
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, NonObjectPayloadWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.flood", "[1,2]");
	EXPECT_EQ(log.Warnings("test.flood"), W(true));
}

TEST(EventPayloadValidation, PayloadOnNoPayloadTypeWarns)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.simple", "{\"a\":1}");
	EXPECT_EQ(log.Warnings("test.simple"), W(true));
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, EmptyObjectOnNoPayloadTypeIsFine)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.simple", "{}");
	EXPECT_EQ(log.Warnings(), 0u);
}

TEST(EventPayloadValidation, NullAndEmptyPayloadAreFine)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.drought", nullptr);
	r.Emit("test.drought", "");
	r.Emit("test.simple", nullptr);
	EXPECT_EQ(log.Warnings(), 0u);
	EXPECT_EQ(r.Pending(), 3u);
}

TEST(EventPayloadValidation, MissingDeclaredFieldIsFine)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.drought", "{\"severity\":0.1}");
	EXPECT_EQ(log.Warnings(), 0u);
}

TEST(EventPayloadValidation, VocabularyValueInSetIsFine)
{
	LogCapture log("HIS");
	Rig r; r.StartAlert();
	r.Emit("test.alert", "{\"level\":\"low\",\"on\":true,\"n\":3}");
	EXPECT_EQ(log.Warnings(), 0u);
}

TEST(EventPayloadValidation, BadVocabularyValueWarns)
{
	LogCapture log("HIS");
	Rig r; r.StartAlert();
	r.Emit("test.alert", "{\"level\":\"medium\"}");
	EXPECT_EQ(log.Warnings("test.alert"), W(true));
	EXPECT_EQ(r.Pending(), 1u);
}

TEST(EventPayloadValidation, BoolAndIntFieldTypesAreChecked)
{
	LogCapture log("HIS");
	Rig r; r.StartAlert();
	r.Emit("test.alert", "{\"on\":\"yes\"}");
	EXPECT_EQ(log.Warnings("test.alert"), W(true));
}

TEST(EventPayloadValidation, SecondReasonForSameTypeDoesNotWarnAgain)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	r.Emit("test.flood", "{\"region\":5}");
	r.Emit("test.flood", "{\"bogus\":1}");
	r.Emit("test.flood", "{\"region\":");
	EXPECT_EQ(log.Warnings("test.flood"), W(true));
	EXPECT_EQ(r.rec->EventTypeWarningCount(), kValidates ? 1u : 0u);
	EXPECT_EQ(r.Pending(), 3u);
}

TEST(EventPayloadValidation, NullSchemaServiceDoesNoValidation)
{
	LogCapture log("HIS");
	Rig r; r.Start(false);
	r.Emit("test.unknown", "{garbage");
	r.Emit("test.flood", "{\"region\":5}");
	EXPECT_EQ(log.Warnings(), 0u);
	EXPECT_EQ(r.rec->EventTypeWarningCount(), 0u);
	EXPECT_EQ(r.Pending(), 2u);
}

TEST(EventPayloadValidation, WarnedSetCapacityStopsWarningAndCounts)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	const uint32_t cap = CellHistoryRecorder::kMaxWarnedTypes;
	for (uint32_t i = 0; i < cap; ++i) { r.Emit(("unknown." + std::to_string(i)).c_str(), nullptr); }
	EXPECT_EQ(r.rec->EventTypeWarningCount(), kValidates ? cap : 0u);
	EXPECT_EQ(r.rec->EventValidationWarningsSuppressed(), 0u);
	const unsigned before = log.Warnings();
	r.Emit("unknown.extra", nullptr);
	r.Emit("unknown.extra", nullptr);
	EXPECT_EQ(log.Warnings(), before);
	EXPECT_EQ(r.rec->EventTypeWarningCount(), kValidates ? cap : 0u);
	EXPECT_EQ(r.rec->EventValidationWarningsSuppressed(), kValidates ? 2u : 0u);
	EXPECT_EQ(r.Pending(), cap + 2);
}

TEST(EventPayloadValidation, PayloadOverOneKilobyteIsRejectedWithErrorAndNotRecorded)
{
	LogCapture log("HIS");
	Rig r; r.Start();
	const std::string big = "{\"region\":\"" + std::string(1100, 'x') + "\"}";
	EXPECT_EQ(r.Emit("test.flood", big.c_str()), EmitResult::PayloadTooLarge);
	EXPECT_GE(log.Errors(), 1u);
	EXPECT_EQ(r.Pending(), 0u);
}
