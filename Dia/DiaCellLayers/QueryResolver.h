#pragma once

// QueryResolver: binds a parsed cell query (QueryParser AST) to the active grid's layers and the value schema.
// Each Compare node gets a QueryBinding (layer, kind, op, numeric threshold) keyed by node index. Category layers
// compare against the enum value's index in the schema enum list (the float the layer stores, as SelectionTools).
//
// Name resolution: a query name matches a layer id (StringCRC of the name) on the active grid first; otherwise the
// short schema key (text after the last '/' of ValueInfo::mKey). Layers on other grids never resolve. Errors carry the
// column/length of the offending span and a "did you mean" suggestion (similarity ratio >= 0.6) where one exists.
// Fixed capacity, no heap, no STL. Generic (CL-005), no singletons (CL-006).

#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/QueryParser.h>

namespace Dia::ValueSchema { class ValueSchemaService; }

namespace Dia::CellLayers
{
	struct QueryBinding
	{
		bool					mBound = false;		// true for resolved Compare nodes
		const CellLayerDesc*	mLayer = nullptr;	// borrowed from the registry
		Dia::Core::StringCRC	mLayerId;
		CellValueKind			mKind = CellValueKind::Float;
		QueryCompareOp			mOp = QueryCompareOp::Lt;
		double					mThreshold = 0.0;	// number, or enum index for Category layers
	};

	class QueryResolver
	{
	public:
		static constexpr uint32_t kMaxBindings = QueryParser::kMaxNodes;

		// text is the string that was given to parser.Parse (spans index into it). schema may be null.
		bool				Resolve(const char* text, const QueryParser& parser, const CellLayerRegistry& registry,
								const Dia::Graphics::CellGridDesc* grid, const Dia::ValueSchema::ValueSchemaService* schema);

		// Binding for a node index (mBound false for And/Or/Not nodes or out of range).
		const QueryBinding&	Binding(uint32_t nodeIndex) const;
		const QueryError&	Error() const { return mError; }

	private:
		bool				Fail(const char* message, QuerySpan span, const char* suggestion = nullptr);

		QueryBinding		mBindings[kMaxBindings];
		QueryError			mError;
	};
}
