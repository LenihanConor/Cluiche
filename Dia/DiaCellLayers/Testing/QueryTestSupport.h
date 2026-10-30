#pragma once

// Test helpers for the cell query resolver (Testing/ convention): a schema + registry fixture with layers on two
// grids, an enum-entry builder, and a parse-and-resolve helper.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/QueryResolver.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

#include <initializer_list>

namespace Dia::CellLayers::Testing
{
	// Schema full keys are "cells/<key>". Layer id and schema key are independent so tests can exercise both paths.
	struct QueryFixture
	{
		UploaderSchemaFixture			fx;
		const Dia::ValueSchema::ValueSchemaService*	service = nullptr;
		Dia::Graphics::CellGridDesc		gridA;
		Dia::Graphics::CellGridDesc		gridB;
		CellLayerRegistry				registry{ nullptr, &FakePuIdFn };

		static Dia::ValueSchema::ValueInfo Category(const char* key, std::initializer_list<const char*> ids)
		{
			Dia::ValueSchema::ValueInfo i = Dia::ValueSchema::Testing::MakeValueInfo("cells", key);
			i.mType = Dia::ValueSchema::ValueType::Enum;
			i.mEnumValues.Reserve(static_cast<unsigned>(ids.size()));
			for (const char* id : ids)
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				i.mEnumValues.Add(v);
			}
			return i;
		}

		QueryFixture()
		{
			FakePuId() = 1;
			gridA.width = 8; gridA.height = 8;
			gridB.width = 4; gridB.height = 4;
		}

		void Publish() { service = &fx.Publish(); }

		bool AddLayer(const char* id, const Dia::Graphics::CellGridDesc* grid, const char* schemaKey, CellValueKind kind = CellValueKind::Float)
		{
			return registry.Register(MakeLayer(id, grid, schemaKey, kind)) == CellRegisterResult::Ok;
		}

		// Parses then resolves against grid (default gridA). False on parse or resolve failure.
		bool Run(const char* text, QueryParser& parser, QueryResolver& resolver, const Dia::Graphics::CellGridDesc* grid = nullptr)
		{
			if (!parser.Parse(text)) { return false; }
			return resolver.Resolve(text, parser, registry, grid ? grid : &gridA, service);
		}
	};
}
