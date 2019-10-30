#pragma once

#pragma warning( disable : 26812 )

#pragma warning( push )
#pragma warning( disable : 6011 )


#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Core
	{
		class EnumDescription
		{
		public:
			static void GenerateList(char* str, int numberItems, EnumDescription* pStartDescription);
			
			EnumDescription();
			
			void Set(const char* name, int value, const EnumDescription* pNext);
			const EnumDescription* FindByValue(int val)const;
			
			const char* GetName()const{return &mName[0];};
			void GetNameWithClassName(const char* enumName, char* buffer, unsigned int bufferSize)const;

		private: 
			static const int kMaxNameSize = 32;	

			int mValue;
			const EnumDescription* mNext;
			const char* mName;
		};
	}
} 

#undef CE_ITEMSTART
#undef CE_ITEMVAL
#undef CE_ITEM
#undef CLASSEDENUM

#define CE_ITEMSTART(A) A=0,
#define CE_ITEMVAL(A,B) A=B,
#define CE_ITEM(A) A,
#define ENUM_ASSERT(exp, str) do { if(!(exp)){ Dia::Core::g_pAssertFunc(#exp, __FILE__, -1, str);}} while (0)	

#define CLASSEDENUM(A, B, C)	\
								class A \
								{\
								public:\
									enum EnumType\
									{\
										B\
										NumberOfItems\
									};\
									union \
									{ \
										EnumType	m_Value;\
										int			m_IntValue; \
									}; \
									A() { ENUM_ASSERT(C<=NumberOfItems, "Default_parameter_to_large");  m_Value=C; }\
									A(const EnumType value){ ENUM_ASSERT(value<=NumberOfItems, "Invalid item"); m_Value=value; }\
									A(const A& value): m_Value(value.m_Value) {};\
									\
									static A CreateFromInt(int intValue) { A temp; temp.m_IntValue = (intValue); return temp; }\
									static A CreateFromUnsignedInt(unsigned int intValue) { A temp; temp.m_IntValue = static_cast<int>(intValue); return temp; }\
									\
									operator int() { return static_cast<int>(m_Value); }\
									\
									A& operator=(const A& RHS) { m_Value = RHS.m_Value; return *this; }\
									A& operator=(const int& RHS) { m_Value = static_cast<EnumType>(RHS); return *this; }\
									\
									bool operator==(const A& RHS) const { return m_Value == RHS.m_Value; }\
									bool operator==(const int& RHS) const { return m_Value == RHS; }\
									bool operator!=(const A& RHS) const { return m_Value != RHS.m_Value; }\
									bool operator!=(const int& RHS) const { return m_Value != RHS; }\
									\
									const char* AsStringEnumClass() const \
									{ \
										static char path[] = #A;\
										return path;\
									} \
									const char* AsString() const \
									{ \
										const Dia::Core::EnumDescription* desc = GetDescription()->FindByValue(m_Value); \
										if (desc == 0){ ENUM_ASSERT(desc != 0, "Could not find Current Enum"); return nullptr; } \
										return desc->GetName(); \
									} \
									void AsStringWithEnumName(char* buffer, unsigned int bufferSize) const \
									{ \
										const Dia::Core::EnumDescription* desc = GetDescription()->FindByValue(m_Value); \
										if (desc == 0){ ENUM_ASSERT(desc != 0, "Could not find Current Enum"); return; } \
										desc->GetNameWithClassName(AsStringEnumClass(), buffer, bufferSize); \
									} \
								private:\
									static const Dia::Core::EnumDescription* GetDescription()\
									{\
										static Dia::Core::EnumDescription* sDescription = 0;\
										static Dia::Core::EnumDescription sDescriptions[NumberOfItems];\
										static char names[] = #B;\
										if (sDescription == 0)\
										{\
											Dia::Core::EnumDescription::GenerateList(names, NumberOfItems, sDescriptions);\
											sDescription = &sDescriptions[0];\
										}\
										return sDescription;\
									}\
								};
#pragma warning( pop )