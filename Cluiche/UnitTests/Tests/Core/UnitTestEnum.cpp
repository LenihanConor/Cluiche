
#include "UnitTests/Tests/Core/UnitTestEnum.h"

#include <DiaCore/Core/EnumClass.h>
#include <DiaCore/Strings/String32.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	UnitTestEnum::UnitTestEnum(const Dia::Core::Containers::String32& name)
		: UnitTestCore(name)
	{}

	UnitTestEnum::UnitTestEnum(void)
		: UnitTestCore()
	{}

	void UnitTestEnum::DoTest()
	{
		UNIT_TEST_BLOCK_START()
			
			CLASSEDENUM (EStyle,\
				CE_ITEMVAL(None, -1)\
				CE_ITEM(Attacking)\
				CE_ITEM(Defending)\
				, None \
			);

			EStyle style1;
			EStyle style2 = EStyle::None;
			EStyle style3 = EStyle::Defending;
			EStyle style4 = EStyle::Attacking;
			
			UNIT_TEST_POSITIVE(style1 == EStyle::None, "Enum");
			UNIT_TEST_POSITIVE(style2 == EStyle::None, "Enum");
			UNIT_TEST_POSITIVE(style3 == EStyle::Defending, "Enum");
			UNIT_TEST_POSITIVE(style4 == EStyle::Attacking, "Enum");

			int a = style1;
			int b = style2;
			int c = style3;
			int d = style4;

			UNIT_TEST_POSITIVE(a == -1, "Enum");
			UNIT_TEST_POSITIVE(b == -1, "Enum");
			UNIT_TEST_POSITIVE(c == 1, "Enum");
			UNIT_TEST_POSITIVE(d == 0, "Enum");

 			Dia::Core::Containers::String32 styleName1 = style1.AsString();
 			Dia::Core::Containers::String32 styleName2 = style2.AsString();
 			Dia::Core::Containers::String32 styleName3 = style3.AsString();
 			Dia::Core::Containers::String32 styleName4 = style4.AsString();
 
 			UNIT_TEST_POSITIVE(styleName1 == "None", "Enum");
			UNIT_TEST_POSITIVE(styleName2 == "None", "Enum");
			UNIT_TEST_POSITIVE(styleName3 == "Defending", "Enum");
			UNIT_TEST_POSITIVE(styleName4 == "Attacking", "Enum");

			Dia::Core::Containers::String32 styleEnumName1 = style1.AsStringEnumClass();
			Dia::Core::Containers::String32 styleEnumName2 = style2.AsStringEnumClass();
			Dia::Core::Containers::String32 styleEnumName3 = style3.AsStringEnumClass();
			Dia::Core::Containers::String32 styleEnumName4 = style4.AsStringEnumClass();

			UNIT_TEST_POSITIVE(styleEnumName1 == "EStyle", "Enum");
			UNIT_TEST_POSITIVE(styleEnumName2 == "EStyle", "Enum");
			UNIT_TEST_POSITIVE(styleEnumName3 == "EStyle", "Enum");
			UNIT_TEST_POSITIVE(styleEnumName4 == "EStyle", "Enum");
			
			Dia::Core::Containers::String32 styleWithEnumName1;
			Dia::Core::Containers::String32 styleWithEnumName2;
			Dia::Core::Containers::String32 styleWithEnumName3;
			Dia::Core::Containers::String32 styleWithEnumName4;
			
			style1.AsStringWithEnumName(styleWithEnumName1.AsCStr(), styleWithEnumName1.Size());
			style2.AsStringWithEnumName(styleWithEnumName2.AsCStr(), styleWithEnumName1.Size());
			style3.AsStringWithEnumName(styleWithEnumName3.AsCStr(), styleWithEnumName1.Size());
			style4.AsStringWithEnumName(styleWithEnumName4.AsCStr(), styleWithEnumName1.Size());

			UNIT_TEST_POSITIVE(styleWithEnumName1 == "EStyle::None", "Enum");
			UNIT_TEST_POSITIVE(styleWithEnumName2 == "EStyle::None", "Enum");
			UNIT_TEST_POSITIVE(styleWithEnumName3 == "EStyle::Defending", "Enum");
			UNIT_TEST_POSITIVE(styleWithEnumName4 == "EStyle::Attacking", "Enum");

		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
