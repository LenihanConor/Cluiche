
#include "UnitTests/Tests/Collections/UnitTestComponent.h"

#include <DiaCore/Architecture/Components/Concrete/StaticPooledComponentFactory.h>
#include <DiaCore/Architecture/Components/Concrete/StaticSizedComponentObject.h>

#include "UnitTests/Infrastructure/UnitTestMacros.h"

namespace UnitTests
{	
	class DonkeyFeetComponent: public Dia::Core::IComponent
	{
	public:
		COMPONENT_DECLARATION(0xDEADBEEF);
	};

	CREATE_STATIC_SIZED_COMPONENT_FACTORY(DonkeyFeetComponentFactory, DonkeyFeetComponent, 4);

	class Donkey: public Dia::Core::StaticSizedComponentObject<2>
	{
	public:
	};

	UnitTestComponent::UnitTestComponent(const Dia::Core::Containers::String32& name)
		: UnitTestCoreContainers(name)
	{}

	UnitTestComponent::UnitTestComponent(void)
		: UnitTestCoreContainers()
	{}

	void UnitTestComponent::DoTest()
	{
		UNIT_TEST_BLOCK_START()

			DonkeyFeetComponentFactory donkeyFeetFactory;

			Dia::Core::IComponent* donkeyFeetA = nullptr;
			DonkeyFeetComponent* donkeyFeetB = nullptr;

			Donkey myDonkey;
	
			myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);

			donkeyFeetA = myDonkey.GetComponentPtr(DonkeyFeetComponent::ID);
			donkeyFeetB = myDonkey.GetCastComponentPtr<DonkeyFeetComponent>();
			UNIT_TEST_POSITIVE(myDonkey.HasComponent(DonkeyFeetComponent::ID), "Component");
			UNIT_TEST_POSITIVE(donkeyFeetA != nullptr, "Component");
			UNIT_TEST_POSITIVE(donkeyFeetB != nullptr, "Component");

			myDonkey.RemoveComponentFromFactory(&donkeyFeetFactory);

			donkeyFeetA = myDonkey.GetComponentPtr(DonkeyFeetComponent::ID);
			donkeyFeetB = myDonkey.GetCastComponentPtr<DonkeyFeetComponent>();

			UNIT_TEST_POSITIVE(!myDonkey.HasComponent(DonkeyFeetComponent::ID), "Component");
			UNIT_TEST_POSITIVE(donkeyFeetA == nullptr, "Component");
			UNIT_TEST_POSITIVE(donkeyFeetB == nullptr, "Component");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::IComponent* donkeyFeetA = nullptr;
			DonkeyFeetComponent* donkeyFeetB = nullptr;

			Donkey myDonkey;
			
			// Test the manager going out of scope
			{
				DonkeyFeetComponentFactory donkeyFeetFactory;

				myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);

				UNIT_TEST_POSITIVE(myDonkey.HasComponent(DonkeyFeetComponent::ID), "Component");
			}

			UNIT_TEST_POSITIVE(!myDonkey.HasComponent(DonkeyFeetComponent::ID), "Component");

		UNIT_TEST_BLOCK_END()

		UNIT_TEST_BLOCK_START()
		
			Dia::Core::IComponent* donkeyFeetA = nullptr;
			DonkeyFeetComponent* donkeyFeetB = nullptr;

			DonkeyFeetComponentFactory donkeyFeetFactory;
							
			// Test the object going out of scope
			{
				Donkey myDonkey;
				myDonkey.AddComponentFromFactory(&donkeyFeetFactory, nullptr);
				UNIT_TEST_POSITIVE(donkeyFeetFactory.GetNumberOfActiveComponents()  == 1, "Component");
			}

			UNIT_TEST_POSITIVE(donkeyFeetFactory.GetNumberOfActiveComponents() == 0, "Component");

		UNIT_TEST_BLOCK_END()
		
		mState = kFinished;
	}
}
