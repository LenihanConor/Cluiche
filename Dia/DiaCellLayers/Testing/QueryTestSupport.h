#pragma once

// Test helpers for the cell query resolver (Testing/ convention): a schema + registry fixture with layers on two
// grids, an enum-entry builder, and a parse-and-resolve helper.

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/QueryParser.h>
#include <DiaCellLayers/QueryResolver.h>
#include <DiaCellLayers/QueryEvaluator.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>

#include <deque>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

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

	// Evaluator fixture: a w*h grid whose layers return fixed value vectors (layer id == schema key short name).
	// Add layers first (AddValues), then Eval(text). Category layers need the enum entry added to q.fx before Publish.
	struct QueryEvalFixture
	{
		QueryFixture							q;
		std::deque<CountingLayerProvider>		providers;	// stable addresses
		QueryParser								parser;
		QueryResolver							resolver;
		QueryEvaluator							evaluator;
		std::unique_ptr<LayerReader>			reader;

		QueryEvalFixture(uint16_t w, uint16_t h)
		{
			q.gridA.width = w;
			q.gridA.height = h;
		}

		uint32_t CellCount() const { return static_cast<uint32_t>(q.gridA.width) * q.gridA.height; }

		// Registers a layer (id = name, schema key cells/<name>) with the given values. Call Publish via PublishSchema first.
		bool AddValues(const char* name, std::vector<float> values, CellValueKind kind = CellValueKind::Float)
		{
			providers.emplace_back();
			providers.back().values = std::move(values);
			CellLayerDesc d = MakeCountingLayer(name, &q.gridA, providers.back(), kind);
			const std::string key = std::string("cells/") + name;
			d.schemaKey = Dia::Core::StringCRC(key.c_str());
			return q.registry.Register(d) == CellRegisterResult::Ok;
		}

		// Adds float schema entries for the given names (range 0..1), then publishes the schema.
		void PublishSchema(std::initializer_list<const char*> floatNames, std::initializer_list<const char*> categoryNames = {})
		{
			for (const char* n : floatNames) { q.fx.Add(UploaderSchemaFixture::FloatEntry(n, 0, 1)); }
			for (const char* n : categoryNames) { q.fx.Add(QueryFixture::Category(n, { "Grass", "Water", "Rock", "Sand" })); }
			q.Publish();
		}

		LayerReader& Reader()
		{
			if (!reader) { reader.reset(new LayerReader(q.registry)); }
			return *reader;
		}

		// Parse + resolve + evaluate. False on any failure (parser/resolver/evaluator message in Msg()).
		bool Eval(const char* text)
		{
			if (!q.Run(text, parser, resolver)) { return false; }
			return evaluator.Evaluate(parser, resolver, Reader(), CellCount());
		}

		std::string Msg() const
		{
			if (parser.Error().mMessage[0] != '\0') { return parser.Error().mMessage; }
			if (resolver.Error().mMessage[0] != '\0') { return resolver.Error().mMessage; }
			return evaluator.Error().mMessage;
		}

		// Result cell indices in ascending order.
		std::vector<uint32_t> Result() const
		{
			std::vector<uint32_t> out;
			for (uint32_t i = 0; i < evaluator.CellCount(); ++i) { if (evaluator.Test(i)) { out.push_back(i); } }
			return out;
		}
	};

	// Expected cell set from a predicate over the cell index (hand-computed sets or lambdas over layer values).
	template <typename Pred>
	inline std::vector<uint32_t> ExpectedCells(uint32_t cellCount, Pred pred)
	{
		std::vector<uint32_t> out;
		for (uint32_t i = 0; i < cellCount; ++i) { if (pred(i)) { out.push_back(i); } }
		return out;
	}
}
