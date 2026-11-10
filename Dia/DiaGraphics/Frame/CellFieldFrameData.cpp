////////////////////////////////////////////////////////////////////////////////
// Filename: CellFieldFrameData.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/CellFieldFrameData.h"

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		CellFieldFrameData::CellFieldFrameData()
			: mDroppedCount(0)
		{}

		//------------------------------------------------------------------------------
		void CellFieldFrameData::ClearCellFieldBuffer()
		{
			mRequests.RemoveAll();
			mDroppedCount = 0;
		}

		//------------------------------------------------------------------------------
		void CellFieldFrameData::CopyCellFieldBuffer(const CellFieldFrameData& rhs)
		{
			if (this == &rhs) { return; }	// RemoveAll below would erase the source too
			mRequests.RemoveAll();
			for (unsigned int i = 0; i < rhs.mRequests.Size(); ++i)
			{
				mRequests.Add(rhs.mRequests[i]);
			}
			mDroppedCount = rhs.mDroppedCount;
		}

		//------------------------------------------------------------------------------
		void CellFieldFrameData::RequestDrawCellFieldData(const Dia::Core::CellFieldDrawRequest& request)
		{
			if (mRequests.Size() >= kStep1Limit)
			{
				++mDroppedCount;
				return;
			}
			mRequests.Add(request);
		}
	}
}
