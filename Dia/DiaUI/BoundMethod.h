//////////////////
#pragma once

#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>
#include <DiaCore/Core/EnumClass.h>

namespace Dia
{
	namespace UI
	{
		//------------------------------------------------------
		// This class represents data that can be tranmistted from UI to C++
		class BoundMethodValue
		{
		public:
			CLASSEDENUM(EType, \
				CE_ITEMVAL(kBoolean, 0)\
				CE_ITEM(kInteger)\
				CE_ITEM(kDouble)\
				CE_ITEM(kString)\
				, kBoolean \
				);

			BoundMethodValue();

			explicit BoundMethodValue(bool value);
			explicit BoundMethodValue(int value);
			explicit BoundMethodValue(double value);
			explicit BoundMethodValue(const Dia::Core::Containers::String64& value);

			BoundMethodValue(const BoundMethodValue& original);

			BoundMethodValue& operator=(const BoundMethodValue& rhs);

			bool IsBoolean() const;
			bool IsInteger() const;
			bool IsDouble() const;
			bool IsString() const;

			EType GetType()const { return mType; }
			bool GetBoolean() const;
			int GetInteger() const;
			double GetDouble() const;
			const Dia::Core::Containers::String64& GetString() const;

		private:
			EType mType;	

			// Union for each type of data that can be represented
			union
			{
				bool mBoolean;
				int mInt;
				double mDouble;
				Dia::Core::Containers::String64 mString;
			};
		};

		//------------------------------------------------------
		// This class represents the values from the UI to the C++ method
		class BoundMethodArgs
		{
		public:
			unsigned int Size() const;
			const BoundMethodValue& At(unsigned int idx) const;
			void Add(const BoundMethodValue& value);
			void Clear();

		private:
			static const int kMaxArgs = 8;
			typedef Dia::Core::Containers::DynamicArrayC<BoundMethodValue, kMaxArgs> ArgList;

			ArgList mArgs;	// List of BoundMethodValue that are being passed from UI to C++
		};

		//------------------------------------------------------
		// This class represents a method that is called from UI into C++
		class BoundMethod
		{
		public:
			typedef fastdelegate::FastDelegate1<const Dia::UI::BoundMethodArgs&> MethodPtr;
			typedef fastdelegate::FastDelegate1<const Dia::UI::BoundMethodArgs&, BoundMethodValue> MethodPtrWithRetVal;

			typedef void(*FunctionPtr)(const BoundMethodArgs *);

			BoundMethod() {};

			BoundMethod(const char* name)
				: mName(name)
			{}

			BoundMethod( MethodPtr methodPtr)
				:  mMethodPtr(methodPtr)
			{}

			BoundMethod(const char* name, MethodPtr methodPtr)
				: mName(name)
				, mMethodPtr(methodPtr)
			{}

			const Dia::Core::Containers::String32& GetName()const { return mName; };
			MethodPtr& GetMethodPtr() { return mMethodPtr;  }

		private:
			Dia::Core::Containers::String32 mName;		// Key value used to find this from the UI
			MethodPtr mMethodPtr;						// Function that will be called when invoked from UI
		};

		static const int kMaxBoundMethod = 32;
		typedef Dia::Core::Containers::DynamicArrayC<BoundMethod, kMaxBoundMethod> BoundMethodList;
	}
}