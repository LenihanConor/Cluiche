////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldFrameData.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/DebugDraw/CellFieldDrawRequest.h>

#include <stdint.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// CellFieldFrameData - Per-frame store of CellField layer draw requests.
		/// Separate from the debug primitive buffers: consumes no primitive budget.
		///
		class CellFieldFrameData
		{
		public:
			static constexpr unsigned int kCapacity = 8u;       // Storage (Step 2 headroom)
			static constexpr unsigned int kStep1Limit = 1u;     // Step 1 accepts one request per frame

			typedef Dia::Core::Containers::DynamicArrayC<Dia::Core::CellFieldDrawRequest, kCapacity> RequestArray;

			CellFieldFrameData();

			void ClearCellFieldBuffer();
			void CopyCellFieldBuffer(const CellFieldFrameData& rhs);

			/// Stores the request, or drops and counts it if the per-frame limit is reached.
			void RequestDrawCellFieldData(const Dia::Core::CellFieldDrawRequest& request);

			const RequestArray& GetCellFieldFrameData() const { return mRequests; }
			uint32_t CellFieldDroppedCount() const { return mDroppedCount; }

		private:
			RequestArray mRequests;
			uint32_t mDroppedCount;
		};
	}
}
