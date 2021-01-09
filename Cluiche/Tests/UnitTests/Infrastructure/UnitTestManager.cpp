
#include "UnitTestManager.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Time/SystemClock.h>
#include <DiaCore/Strings/String128.h>

#include "UnitTests/Infrastructure/UnitTestInterface.h"
#include "UnitTests/Infrastructure/UnitTestMacros.h"

#include "UnitTests/Tests/Application/UnitTestApplication.h"

#include "UnitTests/Tests/Core/UnitTestTimeAbsolute.h"
#include "UnitTests/Tests/Core/UnitTestTimeRelative.h"
#include "UnitTests/Tests/Core/UnitTestTimeServer.h"
#include "UnitTests/Tests/Core/UnitTestTimer.h"
#include "UnitTests/Tests/Core/UnitTestTimerExpiry.h"
#include "UnitTests/Tests/Core/UnitTestTimerSystem.h"
#include "UnitTests/Tests/Core/UnitTestEnum.h"
#include "UnitTests/Tests/Core/UnitTestBitFlag.h"
#include "UnitTests/Tests/Core/UnitTestCRC.h"
#include "UnitTests/Tests/Core/UnitTestMetaLogic.h"
#include "UnitTests/Tests/Core/UnitTestTypeTraits.h"
#include "UnitTests/Tests/Core/UnitTestTypes.h"
#include "UnitTests/Tests/Core/UnitTestJsonTypes.h"

#include "UnitTests/Tests/Collections/UnitTestArray.h"
#include "UnitTests/Tests/Collections/UnitTestArrayC.h"
#include "UnitTests/Tests/Collections/UnitTestDynamicArrayC.h"
#include "UnitTests/Tests/Collections/UnitTestDynamicArray.h"
#include "UnitTests/Tests/Collections/UnitTestArrayIterators.h"
#include "UnitTests/Tests/Collections/UnitTestLinkListC.h"
#include "UnitTests/Tests/Collections/UnitTestCircularBuffer.h"
#include "UnitTests/Tests/Collections/UnitTestSingleton.h"
#include "UnitTests/Tests/Collections/UnitTestComponent.h"
#include "UnitTests/Tests/Collections/UnitTestString.h"
#include "UnitTests/Tests/Collections/UnitTestStringWriter.h"
#include "UnitTests/Tests/Collections/UnitTestHashTable.h"

#include "UnitTests/Tests/Maths/UnitTestCoreMaths.h"
#include "UnitTests/Tests/Maths/UnitTestFloatMaths.h"
#include "UnitTests/Tests/Maths/UnitTestHalfFloat.h"
#include "UnitTests/Tests/Maths/UnitTestAngle.h"
#include "UnitTests/Tests/Maths/UnitTestTrigonometry.h"
#include "UnitTests/Tests/Maths/UnitTestMatrix22.h"
#include "UnitTests/Tests/Maths/UnitTestMatrix33.h"
#include "UnitTests/Tests/Maths/UnitTestMatrix44.h"
#include "UnitTests/Tests/Maths/UnitTestVector2D.h"
#include "UnitTests/Tests/Maths/UnitTestVector3D.h"
#include "UnitTests/Tests/Maths/UnitTestVector4D.h"
#include "UnitTests/Tests/Maths/UnitTestVectorUtil.h"
#include "UnitTests/Tests/Maths/UnitTestHalfVector2D.h"
#include "UnitTests/Tests/Maths/UnitTestIntersectionTest.h"
#include "UnitTests/Tests/Maths/UnitTestIntersectionClassify.h"
#include "UnitTests/Tests/Maths/UnitTestCircle2D.h"

#include "UnitTests/Tests/Graphics/UnitTestRGBA.h"

namespace UnitTests
{
	UnitTestManager::UnitTestManager(void)
	{
		mCurrentTest = NULL;
	}

	UnitTestManager::~UnitTestManager(void)
	{}

	void UnitTestManager::Init()
	{
		Flush();
		
		mUnitTestArray.Add( DIA_NEW( UnitTestTimeAbsolute("TimeAbsolute")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTimeRelative("TimeRelative")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTimeServer("TimeServer")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTimer("Timer")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTimerExpiry("TimerExpiry")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTimerSystem("TimerSystem")));
		mUnitTestArray.Add( DIA_NEW( UnitTestEnum("Enum")));
		mUnitTestArray.Add( DIA_NEW( UnitTestBitArray8("BitArray8")));
		mUnitTestArray.Add( DIA_NEW( UnitTestBitArray16("BitArray16")));
		mUnitTestArray.Add( DIA_NEW( UnitTestBitArray32("BitArray32")));
		mUnitTestArray.Add( DIA_NEW( UnitTestBitArray64("BitArray64")));
		mUnitTestArray.Add( DIA_NEW( UnitTestCRC("CRC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestStringCRC("StringCRC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestStripStringCRC("StripStringCRC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestMetaLogic("MetaLogic")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTypeTraits("TypeTextTraits")));
		mUnitTestArray.Add( DIA_NEW( UnitTestJsonTypes("TypeJsonTraits")));

		Dia::Core::Log::OutputLine("TODO: Types - DEFAULT VALUE ATTRIBUTE");
		Dia::Core::Log::OutputLine("TODO: Types - FLOATING POINT PRESCION ATTRIBUTE");
		Dia::Core::Log::OutputLine("TODO: Types - DESCRIPTION COMMENT ATTRIBUTE"); 
		Dia::Core::Log::OutputLine("TODO: Types - RANGE ATTRIBUTE");  
		Dia::Core::Log::OutputLine("TODO: Types - REMOVE ALL POINTER MATHS FROM TEXTSERIALIZER");
		Dia::Core::Log::OutputLine("TODO: Types - BUFFER SIZED ESTIMATES");

		mUnitTestArray.Add( DIA_NEW( UnitTestTypes("-> NF, Types")));

		mUnitTestArray.Add( DIA_NEW( UnitTestApplication("Application System")));
		
		mUnitTestArray.Add( DIA_NEW( UnitTestArray("Array")));
		mUnitTestArray.Add( DIA_NEW( UnitTestArrayC("ArrayC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestDynamicArrayC("DynamicArrayC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestDynamicArray("DynamicArray")));
		mUnitTestArray.Add( DIA_NEW( UnitTestArrayIterators("ArrayIterators")));
		mUnitTestArray.Add( DIA_NEW( UnitTestLinkLIstC("LinkListC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestCircularBuffer("UnitTestCircularBuffer")));
		mUnitTestArray.Add( DIA_NEW( UnitTestString8("String8")));
		mUnitTestArray.Add( DIA_NEW( UnitTestString32("String32")));
		mUnitTestArray.Add( DIA_NEW( UnitTestString64("String64")));
		mUnitTestArray.Add( DIA_NEW( UnitTestString128("String128")));
		mUnitTestArray.Add( DIA_NEW( UnitTestString1024("String1024")));
		mUnitTestArray.Add( DIA_NEW( UnitTestStringCommon("StringCommon")));
		mUnitTestArray.Add( DIA_NEW( UnitTestStringWriter("StringWriter")));
		mUnitTestArray.Add( DIA_NEW( UnitTestSingleton("Singleton")));
		mUnitTestArray.Add( DIA_NEW( UnitTestComponent("Component")));
		mUnitTestArray.Add( DIA_NEW( UnitTestHashTableC("HashTableC")));
		mUnitTestArray.Add( DIA_NEW( UnitTestHashTable("HashTable")));

		mUnitTestArray.Add( DIA_NEW( UnitTestCoreMaths("CoreMaths")));
		mUnitTestArray.Add( DIA_NEW( UnitTestFloatMaths("FloatMaths")));
		mUnitTestArray.Add( DIA_NEW( UnitTestHalfFloat("HalfFloat")));
		mUnitTestArray.Add( DIA_NEW( UnitTestAngle("Angle")));
		mUnitTestArray.Add( DIA_NEW( UnitTestTrigonometry("Trigonometry")));
		mUnitTestArray.Add( DIA_NEW( UnitTestMatrix22("Matrix22")));
	//	mUnitTestArray.Add( DIA_NEW( UnitTestMatrix22("Matrix33")));
	//	mUnitTestArray.Add( DIA_NEW( UnitTestMatrix22("Matrix44")));
		mUnitTestArray.Add( DIA_NEW( UnitTestVector2D("Vector2D")));
	//	mUnitTestArray.Add( DIA_NEW( UnitTestVector3D("Vector3D")));
	//	mUnitTestArray.Add( DIA_NEW( UnitTestVector4D("Vector4D")));
	//	mUnitTestArray.Add( DIA_NEW( UnitTestVectorUtil("VectorUtil")));
		Dia::Core::Log::OutputLine("TODO: VectorHalf2D - REWRITE TO BE MORE LIKE A CONVERSION CLASS");
		mUnitTestArray.Add( DIA_NEW( UnitTestVectorHalf2D("-> NF, VectorHalf2D")));

		//	mUnitTestArray.Add( DIA_NEW( UnitTestIntersectionPoint2D("IntersectionPoint2D")));
		mUnitTestArray.Add( DIA_NEW( UnitTestIntersectionClassify("IntersectionClassify")));
mUnitTestArray.Add( DIA_NEW( UnitTestIntersectionTest("->NF, IntersectionTests")));

		//	mUnitTestArray.Add( DIA_NEW( UnitTestAARect2D("AARect2D")));
		//	mUnitTestArray.Add( DIA_NEW( UnitTestArc2D("Arc2D")));
		//	mUnitTestArray.Add( DIA_NEW( UnitTestCapsule2D("Capsule2D")));
mUnitTestArray.Add( DIA_NEW( UnitTestCircle2D("->NF Circle2D")));
		//	mUnitTestArray.Add( DIA_NEW( UnitTestLine2D("Line2D")));
		//	mUnitTestArray.Add( DIA_NEW( UnitTestOORect2D("OORect2D")));
		//	mUnitTestArray.Add( DIA_NEW( UnitTestTriangle2D("Triangle2D")));

		mUnitTestArray.Add(DIA_NEW(UnitTestRGBA("RGBA")));
	}

	void UnitTestManager::Test(const Dia::Core::SystemClock& clock)
	{
		Dia::Core::Log::OutputLine("\n-------------------- UnitTesting Start Tests --------------------\n");

		Dia::Core::g_pAssertFunc = UnitTestAsserts::AssertDefaultUnitTest;

		for (unsigned int i = 0; i < mUnitTestArray.Size(); i++)
		{
			mCurrentTest = mUnitTestArray[i]; 
			
			Dia::Core::Containers::String128 str("Testing: %s::%s", mCurrentTest->TypeName(), mCurrentTest->Name());
			Dia::Core::Log::OutputLine(str.AsCStr());

			mCurrentTest->Start(clock);
			while (!mCurrentTest->IsFinished())
			{
				mCurrentTest->Test();
			}
			mCurrentTest->Stop(clock);	
		}

		Dia::Core::Log::OutputLine("\n--------------------UnitTesting End Test--------------------\n");
	}

	void UnitTestManager::Flush()
	{
		for (unsigned int i = 0; i < mUnitTestArray.Size(); i++)
		{
			mUnitTestArray[i]->Flush();
			DIA_DELETE(mUnitTestArray[i]);
		}
	}

	const UnitTestInterface* UnitTestManager::CurrentUnitTest()const
	{
		return mCurrentTest;
	}

	UnitTestInterface* UnitTestManager::CurrentUnitTest()
	{
		return mCurrentTest;
	}
}