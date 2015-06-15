#ifndef DIA_ICOMPONENT_OBJECT
#define DIA_ICOMPONENT_OBJECT

#include "DiaCore/Architecture/Components/Interface/ComponentID.h"
#include "DiaCore/Architecture/Components/Interface/IComponent.h"

namespace Dia
{
	namespace Core
	{
		class IComponentFactory;
	}
}

//------------------------------------------------------------------------------------
//	Example usage of this system
/*

	// The component class 
	class DonkeyFeetComponent: public Dia::Core::IComponent
	{
	public:
		COMPONENT_DECLARATION(0xDEADBEEF);

		struct DonkeyFeetCreateStruct: public Dia::Core::IComponent::CreateStruct
		{
			Colour mColourOfFeet;
		}

		virtual void DoCreate(CreateStruct* input)override
		{
			mHowManyFeet = 2;
			mColourOfFeet = input->Cast<DonkeyFeetCreateStruct>()->mColourOfFeet;
		};

		virtual void DoDestroy()
		{
			mHowManyFeet = 0;
			mColourOfFeet = BLACK;
		};

	private:
		int mHowManyFeet;;
		Colour mColourOfFeet;
	};

	// The component factory
	class DonkeyFeetFactory: public Dia::Core::StaticPooledComponentFactory<DonkeyFeetComponent, 4>
	{
		COMPONENT_FACTORY_DECLARATION(DonkeyFeetComponent);\
	};

	// The component object class
	class Donkey: public Dia::Core::StaticSizedComponentObject<2>
	{
	public:
	};

	// How it is all done together
	DonkeyFeetFactory donkeyFeetFactory;

	Donkey myDonkey;
	
	myDonkey.AddComponentFromFactory(&donkeyFeetFactory);

	Dia::Core::IComponent* donkeyFeetA = myDonkey.GetComponentPtr(DonkeyFeetComponent::ID);
	DonkeyFeetComponent* donkeyFeetB = myDonkey.GetCastComponentPtr<DonkeyFeetComponent>();

	myDonkey.RemoveComponentFromFactory(&donkeyFeetFactory);
	*/
//
		
//------------------------------------------------------------------------------------
namespace Dia
{
	namespace Core
	{
		/******************************************************************************/
		/*!
			\class  IComponentObject

			\brief  Interface for a component object
		*/
		/******************************************************************************/
		class IComponentObject
		{
		public:
			IComponentObject(){};
			virtual ~IComponentObject(){}

			virtual bool HasComponent(const ComponentClassID& id)const = 0;
			virtual IComponent* GetComponentPtr(const ComponentClassID& id) = 0;
			virtual const IComponent* GetComponentPtr(const ComponentClassID& id)const = 0;

			void AddComponentFromFactory(IComponentFactory* factory, IComponent::CreateStruct* input);
			void RemoveComponentFromFactory(IComponentFactory* factory);

			void RemoveComponentFromID(ComponentClassID);

			/******************************************************************************/
			/*!
				\brief  Get and cast of a specific component on the object
			*/
			/******************************************************************************/
			template <class T>
			T* GetCastComponentPtr()
			{
				IComponent* comp = GetComponentPtr(T::ID);
				if (comp == nullptr)
				{
					return nullptr;
				}

				return static_cast<T*>(comp);
			}

			/******************************************************************************/
			/*!
				\brief  Get and cast of a specific component on the object
			*/
			/******************************************************************************/
			template <class T>
			const T* GetCastComponentPtrConst()
			{
				const IComponent* comp = GetComponentPtr(T::ID);
				if (comp == nullptr)
				{
					return nullptr;
				}

				return static_cast<const T*>(comp);
			}

		protected:
			virtual void AssignComponent(IComponent* component) = 0;
			virtual void DeassignComponent(IComponent* component) = 0;
		};
	}
}

#endif