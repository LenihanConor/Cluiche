#pragma once

#include "UnitTests/Tests/Collections/UnitTestCollections.h"

namespace UnitTests
{
	class UnitTestString8: public UnitTestCoreContainers
	{
	public:
		UnitTestString8(const Dia::Core::Containers::String32& name);
		UnitTestString8(void);

		void DoTest();
	};

	class UnitTestString32: public UnitTestCoreContainers
	{
	public:
		UnitTestString32(const Dia::Core::Containers::String32& name);
		UnitTestString32(void);

		void DoTest();
	};

	class UnitTestString64: public UnitTestCoreContainers
	{
	public:
		UnitTestString64(const Dia::Core::Containers::String32& name);
		UnitTestString64(void);

		void DoTest();
	};

	class UnitTestString128: public UnitTestCoreContainers
	{
	public:
		UnitTestString128(const Dia::Core::Containers::String32& name);
		UnitTestString128(void);

		void DoTest();
	};

	class UnitTestString1024: public UnitTestCoreContainers
	{
	public:
		UnitTestString1024(const Dia::Core::Containers::String32& name);
		UnitTestString1024(void);

		void DoTest();
	};

	class UnitTestStringCommon: public UnitTestCoreContainers
	{
	public:
		UnitTestStringCommon(const Dia::Core::Containers::String32& name);
		UnitTestStringCommon(void);

		void DoTest();
	};
}