#pragma once

// ValueSchemaExporter: builds the dia.value_schema_export v1 document (DiaValueSchema Feature 6, AC 2, 3, 6) from the
// same published snapshot the live service holds, so the file is exactly what the running app would serve.
//
//   { format, version:1,
//     header:  { app, config, generated_at (UTC ISO-8601), code_side:true },   // git_commit / dirty are added by the CLI
//     entries: [ ValueInfoToJson + "origin" ],                                  // registration order
//     ramps:   [ RampToJson ],                                                  // same array as valueschema.get
//     undescribed: { metrics:[{name,owner}], commands:[{name,owner,args:[...]}], component_fields:[{name,owner}] } }
//
// Undescribed commands = the collector's commands with neither params nor noArgs (args empty) plus commands that have
// parameters lacking a description (args = those parameter names). noArgs commands are never listed.
// Independent of ValueSchemaModule: the describe-mode wiring passes the module's service and reports in.
// Observation: Info log on write, Error on failed write, profile scope "ValueSchema.Export", gauges
// valueschema.export_entries and valueschema.export_undescribed.

#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ComponentFieldCollector.h>
#include <json/json.h>

namespace Dia::ValueSchema
{
	struct ExportHeader
	{
		const char*	mApp = "";
		const char*	mConfig = "";
	};

	class ValueSchemaExporter
	{
	public:
		static constexpr const char* kFormat = "dia.value_schema_export";
		static constexpr int kVersion = 1;

		// Either report may be null (no undescribed items of that kind).
		Json::Value Build(const ValueSchemaService& service, const CodeSideReport* codeSide,
			const ComponentFieldReport* components, const ExportHeader& header) const;

		// Atomic: writes "<path>.tmp" then renames over `path`. False (with an Error log) on any failure.
		static bool WriteFile(const char* path, const Json::Value& doc);
	};
}
