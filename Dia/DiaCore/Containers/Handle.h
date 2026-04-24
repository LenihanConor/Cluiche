#pragma once

#include <cstdint>

namespace Dia
{
	namespace Core
	{
		// Typed handle: index + generation counter for safe object identity.
		// A stale handle (pointing at a recycled slot) is detectable via generation mismatch.
		template<typename T>
		class Handle
		{
		public:
			static const uint32_t kInvalidIndex = 0xFFFFFFFFu;
			static const uint32_t kInvalidGeneration = 0u;

			Handle()
				: mIndex(kInvalidIndex)
				, mGeneration(kInvalidGeneration)
			{}

			Handle(uint32_t index, uint32_t generation)
				: mIndex(index)
				, mGeneration(generation)
			{}

			static Handle<T> Invalid() { return Handle<T>(); }

			bool IsValid() const { return mIndex != kInvalidIndex && mGeneration != kInvalidGeneration; }

			uint32_t GetIndex() const { return mIndex; }
			uint32_t GetGeneration() const { return mGeneration; }

			bool operator==(const Handle<T>& rhs) const
			{
				return mIndex == rhs.mIndex && mGeneration == rhs.mGeneration;
			}
			bool operator!=(const Handle<T>& rhs) const { return !(*this == rhs); }

		private:
			uint32_t mIndex;
			uint32_t mGeneration;
		};
	}
}
