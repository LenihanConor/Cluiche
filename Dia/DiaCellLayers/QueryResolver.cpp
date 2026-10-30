#include <DiaCellLayers/QueryResolver.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace Dia::CellLayers
{
	namespace
	{
		constexpr float		kSuggestionThreshold = 0.6f;
		constexpr uint32_t	kMaxName = 128;		// longest name compared for similarity; longer names get no suggestion

		// Text after the last '/' of a schema key.
		const char* ShortKey(const char* key)
		{
			const char* slash = strrchr(key, '/');
			return slash != nullptr ? slash + 1 : key;
		}

		// Case-insensitive similarity: 1 - levenshtein / max(len). 0 when either string is too long to compare.
		float Similarity(const char* a, const char* b)
		{
			const size_t la = strlen(a);
			const size_t lb = strlen(b);
			if (la == 0 || lb == 0 || la >= kMaxName || lb >= kMaxName)
			{
				return 0.0f;
			}
			uint32_t prev[kMaxName + 1];
			uint32_t cur[kMaxName + 1];
			for (size_t j = 0; j <= lb; ++j) { prev[j] = static_cast<uint32_t>(j); }
			for (size_t i = 1; i <= la; ++i)
			{
				cur[0] = static_cast<uint32_t>(i);
				const int ca = tolower(static_cast<unsigned char>(a[i - 1]));
				for (size_t j = 1; j <= lb; ++j)
				{
					const int cb = tolower(static_cast<unsigned char>(b[j - 1]));
					uint32_t best = prev[j - 1] + (ca == cb ? 0u : 1u);
					if (prev[j] + 1 < best) { best = prev[j] + 1; }
					if (cur[j - 1] + 1 < best) { best = cur[j - 1] + 1; }
					cur[j] = best;
				}
				memcpy(prev, cur, (lb + 1) * sizeof(uint32_t));
			}
			const size_t longest = la > lb ? la : lb;
			return 1.0f - static_cast<float>(prev[lb]) / static_cast<float>(longest);
		}

		// Keeps the best-scoring candidate at or above the suggestion threshold.
		struct BestMatch
		{
			float	mScore = 0.0f;
			char	mText[QueryError::kSuggestionSize] = {};

			void Offer(const char* query, const char* candidate)
			{
				const float s = Similarity(query, candidate);
				if (s >= kSuggestionThreshold && s > mScore)
				{
					mScore = s;
					snprintf(mText, sizeof(mText), "%s", candidate);
				}
			}
		};

		void CopySpan(const char* text, QuerySpan span, char* out, uint32_t cap)
		{
			uint32_t n = span.mLength < cap - 1 ? span.mLength : cap - 1;
			memcpy(out, text + span.mColumn, n);
			out[n] = '\0';
		}
	}

	const QueryBinding& QueryResolver::Binding(uint32_t nodeIndex) const
	{
		static const QueryBinding sNone;
		return nodeIndex < kMaxBindings ? mBindings[nodeIndex] : sNone;
	}

	bool QueryResolver::Fail(const char* message, QuerySpan span, const char* suggestion)
	{
		mError = QueryError();
		snprintf(mError.mMessage, sizeof(mError.mMessage), "%s", message);
		mError.mColumn = span.mColumn;
		mError.mLength = span.mLength;
		if (suggestion != nullptr)
		{
			snprintf(mError.mSuggestion, sizeof(mError.mSuggestion), "%s", suggestion);
		}
		return false;
	}

	bool QueryResolver::Resolve(const char* text, const QueryParser& parser, const CellLayerRegistry& registry,
		const Dia::Graphics::CellGridDesc* grid, const Dia::ValueSchema::ValueSchemaService* schema)
	{
		mError = QueryError();
		for (QueryBinding& b : mBindings) { b = QueryBinding(); }
		if (text == nullptr || grid == nullptr)
		{
			return Fail("no active grid", QuerySpan());
		}

		const uint32_t layerCount = registry.LayerCountForGrid(grid);
		const uint32_t nodeCount = parser.NodeCount() < kMaxBindings ? parser.NodeCount() : kMaxBindings;
		for (uint32_t n = 0; n < nodeCount; ++n)
		{
			const QueryNode& node = parser.Node(n);
			if (node.mKind != QueryNodeKind::Compare)
			{
				continue;
			}

			char name[kMaxName];
			CopySpan(text, node.mName, name, sizeof(name));

			// 1. Registered id on the active grid.
			const CellLayerDesc* layer = nullptr;
			const Dia::Core::StringCRC nameCrc(name);
			for (uint32_t i = 0; i < layerCount && layer == nullptr; ++i)
			{
				const CellLayerDesc* l = registry.LayerOfGrid(grid, i);
				if (l != nullptr && l->id == nameCrc) { layer = l; }
			}

			// 2. Short schema key (ambiguity is an error).
			if (layer == nullptr && schema != nullptr)
			{
				uint32_t matches = 0;
				char list[QueryError::kMessageSize] = {};
				size_t used = 0;
				for (uint32_t i = 0; i < layerCount; ++i)
				{
					const CellLayerDesc* l = registry.LayerOfGrid(grid, i);
					const Dia::ValueSchema::ValueInfo* info = l != nullptr ? schema->Find(l->schemaKey) : nullptr;
					if (info == nullptr || strcmp(ShortKey(info->mKey.AsCStr()), name) != 0)
					{
						continue;
					}
					if (matches == 0) { layer = l; }
					++matches;
					const int w = snprintf(list + used, sizeof(list) - used, "%s%s", matches > 1 ? ", " : "", info->mFullKey.AsCStr());
					if (w > 0) { used = used + static_cast<size_t>(w) < sizeof(list) ? used + static_cast<size_t>(w) : sizeof(list) - 1; }
				}
				if (matches > 1)
				{
					char msg[QueryError::kMessageSize];
					snprintf(msg, sizeof(msg), "ambiguous layer name '%s': %s", name, list);
					return Fail(msg, node.mName);
				}
			}

			if (layer == nullptr)
			{
				BestMatch best;
				if (schema != nullptr)
				{
					for (uint32_t i = 0; i < layerCount; ++i)
					{
						const CellLayerDesc* l = registry.LayerOfGrid(grid, i);
						const Dia::ValueSchema::ValueInfo* info = l != nullptr ? schema->Find(l->schemaKey) : nullptr;
						if (info != nullptr)
						{
							best.Offer(name, ShortKey(info->mKey.AsCStr()));
						}
					}
				}
				char msg[QueryError::kMessageSize];
				snprintf(msg, sizeof(msg), "unknown layer '%s'", name);
				return Fail(msg, node.mName, best.mText[0] != '\0' ? best.mText : nullptr);
			}

			QueryBinding& binding = mBindings[n];
			binding.mLayer = layer;
			binding.mLayerId = layer->id;
			binding.mKind = layer->kind;
			binding.mOp = node.mOp;

			if (layer->kind == CellValueKind::Float)
			{
				if (node.mRhsKind != QueryRhsKind::Number)
				{
					return Fail("expected a number", node.mRhs);
				}
				binding.mThreshold = node.mValue;
				binding.mBound = true;
				continue;
			}

			// Category layer: equality only, against an enum id.
			if (node.mOp != QueryCompareOp::Eq && node.mOp != QueryCompareOp::Ne)
			{
				return Fail("numeric comparison on category layer", node.mOpSpan);
			}
			if (node.mRhsKind != QueryRhsKind::EnumId)
			{
				return Fail("expected a category id", node.mRhs);
			}
			const Dia::ValueSchema::ValueInfo* info = schema != nullptr ? schema->Find(layer->schemaKey) : nullptr;
			if (info == nullptr || info->mEnumValues.Size() == 0)
			{
				return Fail("category layer has no enum values in the schema", node.mName);
			}
			char id[kMaxName];
			CopySpan(text, node.mRhs, id, sizeof(id));
			bool found = false;
			BestMatch best;
			for (unsigned i = 0; i < info->mEnumValues.Size(); ++i)
			{
				const char* candidate = info->mEnumValues[i].mId.AsCStr();
				if (strcmp(candidate, id) == 0)
				{
					binding.mThreshold = static_cast<double>(i);	// the float a category layer stores (SelectionTools)
					found = true;
					break;
				}
				best.Offer(id, candidate);
			}
			if (!found)
			{
				char msg[QueryError::kMessageSize];
				snprintf(msg, sizeof(msg), "unknown value '%s' for layer %s", id, name);
				return Fail(msg, node.mRhs, best.mText[0] != '\0' ? best.mText : nullptr);
			}
			binding.mBound = true;
		}
		return true;
	}
}
