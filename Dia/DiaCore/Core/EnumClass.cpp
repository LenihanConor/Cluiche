#include "DiaCore/Core/EnumClass.h"

#include "DiaCore/Type/BasicTypeDefines.h"
#include "DiaCore/Memory/Memory.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace Dia
{
	namespace Core
	{
		void EnumDescription::GenerateList(char* str, int numberItems, EnumDescription* pDescriptions)
		{
			char* currentCurserChar = str;
			char* openBracketChar = NULL;
 			char* closeBracketChar = NULL;
 			int value = -1;

			int numberItemsFound = 0;

 			do 
			{
 				openBracketChar = strchr( currentCurserChar, '(' );
 				closeBracketChar = strchr( currentCurserChar,')' );
				
				if (openBracketChar == nullptr)
				{
					break;
				}

				if (closeBracketChar == nullptr)
				{
					break;
				}

				currentCurserChar = closeBracketChar+1;

 				if (closeBracketChar!=NULL)
 				{
 					*closeBracketChar = '\0';
 				}
 				if (openBracketChar!=NULL)
 				{
 					char * commaChar = strchr(openBracketChar, ',');
 					if (commaChar)
 					{
 						value = static_cast<int>(strtol(commaChar+1, NULL, 0));			
						*commaChar = '\0';
 						char* endChar = commaChar-1;
						while (*endChar==' ' || *endChar=='\t')
 						{
 							*endChar = '\0';
 							endChar--;
 						}
 					}
 					else
 					{
 						value++;
 					}
 
 					const char* nameStart = openBracketChar+1;
					while (*nameStart==' ' || *nameStart=='\t')
					{
						nameStart++;
					}
							
					if (numberItemsFound == numberItems)
					{
						pDescriptions[numberItemsFound].Set(nameStart, value, 0);
					}
					else
					{
						pDescriptions[numberItemsFound].Set(nameStart, value, &pDescriptions[numberItemsFound+1]);
					}

					numberItemsFound++;
 				}
 			} while(numberItemsFound <= numberItems || openBracketChar == NULL);
		}
		
		EnumDescription::EnumDescription()
			: mValue( 0 )
			, mNext( NULL )
			, mName( NULL )
		{}

		void EnumDescription::Set(const char* name, int value, const EnumDescription* pNext)
		{
			DIA_ASSERT( name, "Name is null" );
	
			mValue = value;
			mNext = pNext;
			mName = name;
		}

		const EnumDescription* EnumDescription::FindByValue(int val)const
		{
			if (mValue == val)
			{
				return this;
			}
			if (mNext == NULL)
			{
				return NULL;
			}

			return mNext->FindByValue(val);
		}

		void EnumDescription::GetNameWithClassName(const char* enumName, char* buffer, unsigned int bufferSize)const
		{
			sprintf_s (buffer, bufferSize, "%s::%s", enumName, mName); 
		}
	}
}