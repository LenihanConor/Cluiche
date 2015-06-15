////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameDataBase.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace Graphics
	{
		// Forward declerations
		class DebugFrameDataVisitor;

		///
		/// DebugFrameDataBase - base class for all debug data
		///
		class DebugFrameDataBase
		{
		public:
			virtual ~DebugFrameDataBase(){};
			virtual void AcceptVisitor(const DebugFrameDataVisitor& visitor)const = 0;
		};
	}
}
