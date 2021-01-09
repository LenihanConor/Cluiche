#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestBitArray8: public UnitTestCore
	{
	public:
		UnitTestBitArray8(const Dia::Core::Containers::String32& name);
		UnitTestBitArray8(void);
		
		void DoTest();
	};

	class UnitTestBitArray16: public UnitTestCore
	{
	public:
		UnitTestBitArray16(const Dia::Core::Containers::String32& name);
		UnitTestBitArray16(void);

		void DoTest();
	};

	class UnitTestBitArray32: public UnitTestCore
	{
	public:
		UnitTestBitArray32(const Dia::Core::Containers::String32& name);
		UnitTestBitArray32(void);

		void DoTest();
	};

	class UnitTestBitArray64: public UnitTestCore
	{
	public:
		UnitTestBitArray64(const Dia::Core::Containers::String32& name);
		UnitTestBitArray64(void);

		void DoTest();
	};
}