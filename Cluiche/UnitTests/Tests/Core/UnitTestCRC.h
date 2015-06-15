#pragma once

#include "UnitTests/Tests/Core/UnitTestCore.h"

namespace UnitTests
{
	class UnitTestCRC: public UnitTestCore
	{
	public:
		UnitTestCRC(const Dia::Core::Containers::String32& name);
		UnitTestCRC(void);
		
		void DoTest();
	};

	class UnitTestStringCRC: public UnitTestCore
	{
	public:
		UnitTestStringCRC(const Dia::Core::Containers::String32& name);
		UnitTestStringCRC(void);

		void DoTest();
	};

	class UnitTestStripStringCRC: public UnitTestCore
	{
	public:
		UnitTestStripStringCRC(const Dia::Core::Containers::String32& name);
		UnitTestStripStringCRC(void);

		void DoTest();
	};
}