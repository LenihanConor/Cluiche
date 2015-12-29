#include "DiaCore/Type/TypeTextSerializer.h"

#include "DiaCore/Type/TypeInstance.h"
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Strings/stringutils.h"
#include "DiaCore/Containers/Strings/StringWriter.h"
#include "DiaCore/Containers/Strings/StringReader.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Strings/String128.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Core/EnumClass.h"
#include "DiaCore/Core/Log.h"

#include <ctype.h>


namespace Dia
{
	namespace Core
	{
		namespace Types
		{

			//------------------------------------------------------------------------------------
			//	HeaderStrings
			//------------------------------------------------------------------------------------

			class HeaderStrings
			{
			public:
				
				CLASSEDENUM (EFlagName,\
					CE_ITEMVAL(Unknown, -1)\
					CE_ITEMVAL(ClassName, 0)\
					CE_ITEM(CRC)\
					CE_ITEM(NumberElements),\
					Unknown \
					);

				struct HeaderStringParse
				{
					HeaderStringParse() : mFlag(EFlagName::Unknown), mHeaderStr(){}

					EFlagName mFlag;
					Containers::String64 mHeaderStr;
				};
				
				void AddToList(const Containers::String32& flag, const Containers::String64& str)
				{
					bool foundFlag = false;
					for (unsigned int i = 0; i < EFlagName::NumberOfItems; i++)
					{
						if (flag == msFlagArray[i])
						{
							foundFlag = true;
							AddToList(EFlagName::CreateFromUnsignedInt(i), str);
						}
					}

					DIA_ASSERT(foundFlag, "Could not find flag %c", flag);
				}

				void AddToList(EFlagName flagName, const Containers::String64& str)
				{
					DIA_ASSERT(FindIndexToFlag(flagName) == -1, "Can not add %s twice, First [%s] Second [%s]", flagName.AsString(), mList[FindIndexToFlag(flagName)].mHeaderStr.AsCStr(),  str.AsCStr());
				
					HeaderStringParse newHeaderStringParse;
					newHeaderStringParse.mFlag = flagName;
					newHeaderStringParse.mHeaderStr = str;
					mList.Add(newHeaderStringParse);
				}

				int FindIndexToFlag(EFlagName flagName)const
				{
					for (int i = 0; i < static_cast<int>(mList.Size()); i++)
					{
						if (mList[i].mFlag == flagName)
						{
							return i;
						}
					}

					return -1;
				}
				
				const Containers::String64& GetString(EFlagName flagName)
				{
					int index = FindIndexToFlag(flagName);

					DIA_ASSERT(FindIndexToFlag(flagName) != -1, "Could not find %s", flagName.AsString());

					return mList[index].mHeaderStr;
				}

				static const Containers::String32& GetFlag(EFlagName flag)
				{
					return msFlagArray[flag];
				}
				
			private:
				static const Containers::String32 msFlagArray[EFlagName::NumberOfItems];
				Dia::Core::Containers::DynamicArrayC< HeaderStringParse, EFlagName::NumberOfItems > mList;
			};
			
			const Containers::String32 HeaderStrings::msFlagArray[] = {"class_name", "crc", "number_element"};

			//------------------------------------------------------------------------------------
			//	TextSerializerInternal
			//------------------------------------------------------------------------------------
			class TextSerializerInternal
			{
				DIA_NON_COPYABLE(TextSerializerInternal);

			public:
				
				TextSerializerInternal(Containers::StringWriter& buffer)
					: mBuffer(buffer)
					, mDepth(0)
				{}
					
				void Serialize(const TypeInstance& instance)
				{
					WriteInstanceDeclaration(instance);

					WriteOpenScope();		// Initial Instance Scope
					
					WriteVariables(instance);

					WriteCloseScope();		// Initial Instance Scope
				}
			
			private:

				//------------------------------------------------------------------------------------
				void WriteInstanceDeclaration(const TypeInstance& instance)
				{
					unsigned int hashID = instance.GetTypeDescriptor()->GetUniqueCRC().Value();
					const char* name = instance.GetTypeDescriptor()->GetName();
					
					Containers::String32 hexHashID;
					StringConvertFromUInt(hexHashID.AsCStr(), hashID, 16);

					mBuffer << name  << " @" << HeaderStrings::GetFlag( HeaderStrings::EFlagName::CRC).AsCStr() << ":" << hexHashID.AsCStr()  << "@\n";
				}

				//------------------------------------------------------------------------------------
				void WriteVariables(const TypeInstance& instance)
				{
					const TypeDefinition::VariableLinkList& variables = instance.GetVariables();
					const TypeDefinition::VariableLinkListNode* currentNode = variables.HeadConst(); 

					while (currentNode != NULL)
					{
						const TypeVariable& currentTypeVariable = *currentNode->GetPayloadConst();

						bool isArithmeticType = currentTypeVariable.IsArithmeticType();
						bool isClassType = currentTypeVariable.IsClassType();
						bool isPointerType = currentTypeVariable.IsPointerType();

						DIA_ASSERT(isArithmeticType || isClassType || isPointerType, "Do not recognize type info on %s", currentTypeVariable.GetName());
			
						// Open Array Scope
						if (currentTypeVariable.IsArrayType())
						{
							const char* name = currentTypeVariable.GetName();
							
							Containers::String32 tabs;
							CreateTabs(tabs);

							mBuffer << tabs.AsCStr() << name  << " @" << HeaderStrings::GetFlag( HeaderStrings::EFlagName::NumberElements ).AsCStr() << ":" << currentTypeVariable.GetNumberOfElements() << "@\n";
							WriteOpenScope();
						}

						if ( isArithmeticType )
						{
							WriteArithmeticType(instance, currentTypeVariable);
						}
						else if ( isClassType )
						{
							WriteClassType(instance, currentTypeVariable);
						}
						else if ( isPointerType )
						{
							if (currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>())
							{
								bool isArithmiticPtrType = currentTypeVariable.IsPointerArthmeticType();
								bool isClassPtrType = currentTypeVariable.IsPointerClassType();
								
								if (isArithmiticPtrType)
								{
									WriteArithmeticType(instance, currentTypeVariable);
								}
								else
								{
									WriteClassType(instance, currentTypeVariable);	
								}
							}
							else
							{
								WritePointerType(instance, currentTypeVariable);
							}
						}
			
						// End array scope
						if (currentTypeVariable.IsArrayType())
						{
							WriteCloseScope();
						}

						currentNode = currentNode->GetNextConst();
					}
				}

				//------------------------------------------------------------------------------------
				void WriteClassType(const TypeInstance& instance, const TypeVariable& currentTypeVariable)
				{
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
 						unsigned int hashID = currentTypeVariable.GetClassDefinition()->GetUniqueCRC();
  						const char* name = currentTypeVariable.GetName();
						const char* className = currentTypeVariable.GetClassDefinition()->GetName();

  						Containers::String32 hexHashID;
  						StringConvertFromUInt(hexHashID.AsCStr(), hashID, 16);
	 					
 						Containers::String32 tabs;
 						CreateTabs(tabs);
	 
  						mBuffer << tabs.AsCStr() << name  << " @" << HeaderStrings::GetFlag( HeaderStrings::EFlagName::ClassName ).AsCStr() << ":" << className << "," << HeaderStrings::GetFlag( HeaderStrings::EFlagName::CRC).AsCStr() << ":" << hexHashID.AsCStr()  << "@\n";
	 
 						WriteOpenScope();
	 					
						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i) );

 						WriteVariables(newInstance);
						
						if (currentTypeVariable.IsArrayType())
						{
							WriteCloseScope('}', "", ",");	
						}
						else
						{
							WriteCloseScope();	
						}					
					}
				}

				//------------------------------------------------------------------------------------
				void WritePointerType(const TypeInstance& instance, const TypeVariable& currentTypeVariable)
				{
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						unsigned int address = currentTypeVariable.GetVariableAddress(instance, i);

						TypeInstance::VariablePath variablePath;
						instance.FindVariablePathFromPointerAddress(address, variablePath);
						
						Containers::String32 tabs;
						CreateTabs(tabs);

						const char* name = currentTypeVariable.GetName();

						mBuffer << tabs.AsCStr() << name << " = " << instance.GetTypeDescriptor()->GetName();
						
						for (unsigned int i = 0; i < variablePath.size(); i++)
						{
							mBuffer << "." << variablePath[i]->GetName();
						}

						if (currentTypeVariable.IsArrayType())
						{
							mBuffer << tabs.AsCStr() << ",\n";
						}
						else
						{
							mBuffer << tabs.AsCStr() << "\n";
						}
					}
				}

				//------------------------------------------------------------------------------------
				void WriteArithmeticType(const TypeInstance& instance, const TypeVariable& currentTypeVariable)
				{
					bool isUnsigned = currentTypeVariable.IsArithmeticUnsigned();

					Containers::String64 valueString;
					
					TypeVariableDataArithmetic::ArithmeticType type = currentTypeVariable.GetArithmeticType();
					
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						if (isUnsigned)
						{
							switch (type)
							{
								case TypeVariableDataArithmetic::kIsArithmeticBool: Convert(currentTypeVariable.GetArithmeticValue<bool>(instance, i), valueString); break;	
								case TypeVariableDataArithmetic::kIsArithmeticChar: Convert(currentTypeVariable.GetArithmeticValue<unsigned char>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticShort: Convert(currentTypeVariable.GetArithmeticValue<unsigned short>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticInt: Convert(currentTypeVariable.GetArithmeticValue<unsigned int>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticLong: Convert(currentTypeVariable.GetArithmeticValue<unsigned long>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticLongLong: Convert(currentTypeVariable.GetArithmeticValue<unsigned long long>(instance, i), valueString); break;
								default: DIA_ASSERT(false, "Could not recognize unsigned type"); break;
							}
						}
						else
						{
							switch (type)
							{
								case TypeVariableDataArithmetic::kIsArithmeticChar: Convert(currentTypeVariable.GetArithmeticValue<char>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticShort: Convert(currentTypeVariable.GetArithmeticValue<short>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticInt: Convert(currentTypeVariable.GetArithmeticValue<int>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticLong: Convert(currentTypeVariable.GetArithmeticValue<long>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticLongLong: Convert(currentTypeVariable.GetArithmeticValue<long long>(instance, i), valueString); break;							
								case TypeVariableDataArithmetic::kIsArithmeticFloat: Convert(currentTypeVariable.GetArithmeticValue<float>(instance, i), valueString); break;
								case TypeVariableDataArithmetic::kIsArithmeticDouble: Convert(currentTypeVariable.GetArithmeticValue<double>(instance, i), valueString); break;
								default: DIA_ASSERT(false, "Could not recognize type"); break;
							}
						}
			
						Containers::String32 tabs;
						CreateTabs(tabs);
						
						if (currentTypeVariable.IsArrayType())
						{
							mBuffer << tabs.AsCStr() << valueString.AsCStr() << ",\n";
						}
						else
						{
							const char* name = currentTypeVariable.GetName();

							mBuffer << tabs.AsCStr() << name << " = " << valueString.AsCStr() << "\n";
						}	
					}
				}
				
				//------------------------------------------------------------------------------------
				void WriteOpenScope(char scopeElement = '{')
				{
					Containers::String32 tabs;
					CreateTabs(tabs);

					mBuffer << tabs.AsCStr() << scopeElement << "\n";

					mDepth++;
				}

				//------------------------------------------------------------------------------------
				void WriteCloseScope(char scopeElement = '}', const char* appendToFront = "", const char* appendToEnd = "")
				{
					mDepth--;

					Containers::String32 tabs;
					CreateTabs(tabs);

					mBuffer << tabs.AsCStr() << appendToFront << scopeElement << appendToEnd <<"\n";

					DIA_ASSERT(mDepth >= 0, "Incorrect scope");
				}

				//------------------------------------------------------------------------------------
				void CreateTabs(Containers::String32& tabs)const
				{
					for (int i = 0; i < mDepth; i++)
					{
						tabs += "\t";
					}
				}
								
				//------------------------------------------------------------------------------------
				void Convert(bool value, Containers::String64& valueString)
				{
					if (value)
					{
						valueString.Append("true");
					}
					else
					{
						valueString.Append("false");
					}
				}

				//------------------------------------------------------------------------------------
				void Convert(unsigned char value, Containers::String64& valueString)
				{		
					StringFormat(valueString.AsCStr(), valueString.Size(), "%c", value);
				}

				//------------------------------------------------------------------------------------
				void Convert(char value, Containers::String64& valueString)
				{
					StringFormat(valueString.AsCStr(), valueString.Size(), "%c", value);
				}

				//------------------------------------------------------------------------------------
				void Convert(unsigned short value, Containers::String64& valueString)
				{
					StringConvertFromUShort(valueString.AsCStr(), 64, value);
				}

				//------------------------------------------------------------------------------------
				void Convert(short value, Containers::String64& valueString)
				{
					StringConvertFromShort(valueString.AsCStr(), 64, value);
				}

				//------------------------------------------------------------------------------------
				void Convert(unsigned int value, Containers::String64& valueString)
				{
					StringConvertFromUInt(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(int value, Containers::String64& valueString)
				{
					StringConvertFromInt(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(unsigned long value, Containers::String64& valueString)
				{
					StringConvertFromUInt(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(long value, Containers::String64& valueString)
				{
					StringConvertFromInt(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(unsigned long long value, Containers::String64& valueString)
				{
					StringConvertFromUInt64(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(long long value, Containers::String64& valueString)
				{
					StringConvertFromInt64(valueString.AsCStr(), value);
				}

				//------------------------------------------------------------------------------------
				void Convert(float value, Containers::String64& valueString)
				{
					StringConvertFromFloat(valueString.AsCStr(), 64, value);
				}

				//------------------------------------------------------------------------------------
				void Convert(double value, Containers::String64& valueString)
				{
					StringConvertFromDouble(valueString.AsCStr(), 64, value);
				}

				int							mDepth;
				Containers::StringWriter	mBuffer;
			};

			//------------------------------------------------------------------------------------
			//	TextDeserializerInternal
			//------------------------------------------------------------------------------------
			class TextDeserializerInternal
			{
				DIA_NON_COPYABLE(TextDeserializerInternal);

			public:

				TextDeserializerInternal(Containers::StringReader& buffer)
					: mBuffer(buffer)
					, mDepth(0)
				{
					static bool populateOnce = true;

					if (populateOnce)
					{
						populateOnce = false;

						msWhitespaceChars.Add(' ');
						msWhitespaceChars.Add('\t');
						msWhitespaceChars.Add('\n');
						msWhitespaceChars.Add('\r');

						msSpecialChars.Add(' ');
						msSpecialChars.Add('\t');
						msSpecialChars.Add('\n');
						msSpecialChars.Add('\r');
						msSpecialChars.Add('\"');
						msSpecialChars.Add('{');
						msSpecialChars.Add('}');
						msSpecialChars.Add('[');
						msSpecialChars.Add(']');
					}
				}
				
				void Deserialize(TypeInstance& instance)
				{
					mPtrFixupList.RemoveAll();

					ConsumeSpecialCharacter();

					ReadInstanceDeclaration(instance);
			
					while (mDepth > 0)
					{
						ReadVariableType(instance);

						ConsumeSpecialCharacter();
					}

					PointerFixup( instance );
				}

			private:

				//------------------------------------------------------------------------------------
				void ReadInstanceDeclaration(const TypeInstance& instance)
				{
					const char* startPtr = mBuffer.GetCurrentPtr();

					Containers::String128 nameBuffer;
					ConsumeCharacterString(nameBuffer);
						
					ConsumeSpecialCharacter();

					HeaderStrings headerStrings;
					ConsumeHeaderStrings(headerStrings);
					
					const Containers::String64& crcBuffer = headerStrings.GetString(HeaderStrings::EFlagName::CRC);
							
					ConsumeSpecialCharacter();

					DIA_ASSERT_SUPPORT( unsigned int crc = StringConvertToUInt(crcBuffer.AsCStr(), 16) );
					
					DIA_ASSERT(nameBuffer == instance.GetTypeDescriptor()->GetName(), "An object of [%s] is trying to deserialise into object of [%s]", nameBuffer.AsCStr(), instance.GetTypeDescriptor()->GetName());
					DIA_ASSERT(crc == instance.GetTypeDescriptor()->GetUniqueCRC().Value(), "%s crc is not correct", nameBuffer.AsCStr());
				}
				
				//------------------------------------------------------------------------------------
				void ReadVariableType(TypeInstance& instance)
				{
					class VariableNameSearchFunctor
					{
					public:
						bool Equal(const TypeVariable* object1, const CRC& object2)const
						{
							return (object1->GetNameCRC() == (object2));
						}
					};

					static VariableNameSearchFunctor variableNameSearchFunctor;

					Containers::String128 variableName;
					ConsumeCharacterString(variableName);
					
					ConsumeWhiteSpace();

					CRC variableNameCRC(variableName.AsCStr());

					const TypeDefinition::VariableLinkListNode* variableLink = instance.GetVariables().FindConst(variableNameCRC, variableNameSearchFunctor);

					DIA_ASSERT(variableLink, "Could not find %s", variableName.AsCStr());
					
					const TypeVariable& currentTypeVariable = *variableLink->GetPayloadConst();
					bool isArithmeticType = currentTypeVariable.IsArithmeticType();
					bool isClassType = currentTypeVariable.IsClassType();
					bool isPointerType = currentTypeVariable.IsPointerType();
					bool isPointerActingAsArthmethicObject = currentTypeVariable.IsPointerArthmeticType() && currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>();
					bool isPointerActingAsClassObject = currentTypeVariable.IsPointerClassType() && currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>();

					if (isArithmeticType || isPointerActingAsArthmethicObject)
					{
						ReadArithmeticType( instance, currentTypeVariable, isPointerActingAsArthmethicObject );
					}
					else if (isClassType || isPointerActingAsClassObject)
					{
						ReadClassType( instance, currentTypeVariable );
					}
					else if (isPointerType)
					{
						DIA_ASSERT(currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>() == false, "Has PointerAsObject Attribute but is treating like pointer");
						ReadPointerType( instance, currentTypeVariable );
					}
				}
				
				//------------------------------------------------------------------------------------
				void PointerFixup(TypeInstance& instance)
				{
					for (unsigned int i =0; i < mPtrFixupList.Size(); i++)
					{
						PtrFixupStruct tempStruct = mPtrFixupList[i];

						Containers::StringReader reader( tempStruct.mPath );
						
						TypeInstance::VariableCRCPath crcPath;

						const char* startPtr = tempStruct.mPath;
						unsigned int numberCharacters = 0;
						bool lastCharWasFullStop = false;
						while ( !IsWhiteSpace( reader.GetCurrent() ) )
						{
							if (lastCharWasFullStop)
							{
								lastCharWasFullStop = false;
								numberCharacters = 0;
								startPtr = reader.GetCurrentPtr();
							}

							if (reader.GetCurrent() == '.')
							{
								lastCharWasFullStop = true;
								CRC newCrc( startPtr, numberCharacters );
								crcPath.push_back(newCrc);
							}

							numberCharacters++;
							reader.Advance();
						}					

						CRC newCrc( startPtr, numberCharacters);
						crcPath.push_back(newCrc);

						unsigned int addressResult = 0;
						instance.FindPointerAddressFromVariableCRCPath( crcPath, addressResult );
						
						char* pointeeAsType = reinterpret_cast<char*>(tempStruct.mPointeeAsType);
						void** pointeeValue = reinterpret_cast<void**>(((tempStruct.mOffsetFromPointee / sizeof (char)) + pointeeAsType));

						*pointeeValue = reinterpret_cast<void*>(addressResult);
					}

					mPtrFixupList.RemoveAll();
				}

				//------------------------------------------------------------------------------------
				void ReadClassType( TypeInstance& instance, const TypeVariable& currentTypeVariable )
				{
					ConsumeArrayHeader(currentTypeVariable);
					
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i) );

						int startingDepth = mDepth;
						
						if (currentTypeVariable.IsArrayType())
						{
							ConsumeWhiteSpace();
							ConsumeAllUntilFirst('@');
						}

						ConsumeSpecialCharacter();
						
						HeaderStrings headerStrings;
						ConsumeHeaderStrings(headerStrings);

						ConsumeSpecialCharacter();

						while (mDepth > startingDepth)
						{
							ReadVariableType(newInstance);

							ConsumeSpecialCharacter();
						}

						ConsumeArraySeperator(currentTypeVariable, i);
					}
				}
				
				//------------------------------------------------------------------------------------
				void ReadPointerType( TypeInstance& instance, const TypeVariable& currentTypeVariable )
				{
					ConsumeArrayHeader(currentTypeVariable);

					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						while (mBuffer.IsInBounds() && mBuffer.GetCurrent() != '=')
						{
							mBuffer.Advance();
						}

						mBuffer.Advance();

						ConsumeWhiteSpace();
					
						PtrFixupStruct newPtrFixupStruct;

						newPtrFixupStruct.mPath = mBuffer.GetCurrentPtr();	
						newPtrFixupStruct.mPointeeAsType = instance.Pointee();

						newPtrFixupStruct.mOffsetFromPointee = currentTypeVariable.GetOffsetFromParent(i);
						
						mPtrFixupList.Add(newPtrFixupStruct);

						while (mBuffer.IsInBounds() && ( IsAlphabeticalCharacter( mBuffer.GetCurrent() ) || IsDigitCharacter( mBuffer.GetCurrent() ) || mBuffer.GetCurrent() == '.' ) )
						{
							mBuffer.Advance();
						}

						ConsumeArraySeperator(currentTypeVariable, i);
					}
				}

				//------------------------------------------------------------------------------------
				void ReadArithmeticType( TypeInstance& instance, const TypeVariable& currentTypeVariable, bool isArthmeticPointer )
				{
					ConsumeArrayHeader(currentTypeVariable);
					
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						while (mBuffer.IsInBounds())
						{
							if (mBuffer.GetCurrent() == '=')
							{
								mBuffer.Advance();
						
								ConsumeWhiteSpace();

								break;
							}

							if (IsDigitCharacter(mBuffer.GetCurrent()) || mBuffer.GetCurrent() == '-')
							{
								break;
							}

							mBuffer.Advance();
						}
						
						bool isBool = currentTypeVariable.IsArithmeticBool();
						bool isChar = currentTypeVariable.IsArithmeticChar();
						
						if (isBool)
						{
							Containers::String128 variableName;
							ConsumeCharacterString(variableName);

							variableName.ToLowerCase();
							bool result = ( variableName == "true" );
							currentTypeVariable.SetArithmeticValue(result, instance, i, isArthmeticPointer);
						}
						else if (isChar)
						{
							currentTypeVariable.SetArithmeticValue(mBuffer.GetCurrent(), instance, i, isArthmeticPointer);
							mBuffer.Advance();
						}
						else 
						{
							bool isUnsigned = currentTypeVariable.IsArithmeticUnsigned();
							
							TypeVariableDataArithmetic::ArithmeticType type = currentTypeVariable.GetArithmeticType();
				
							Containers::String64 variableName;
							ConsumeNumberString(variableName);
							
							if (isUnsigned)
							{
								switch (type)
								{
									case TypeVariableDataArithmetic::kIsArithmeticShort: currentTypeVariable.SetArithmeticValue(StringConvertToUShort( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: currentTypeVariable.SetArithmeticValue(StringConvertToUInt( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: currentTypeVariable.SetArithmeticValue(StringConvertToULong( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: currentTypeVariable.SetArithmeticValue(StringConvertToUInt64( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									default: DIA_ASSERT(false, "Could not recognize unsigned type"); break;
								}
							}
							else
							{
								switch (type)
								{
									case TypeVariableDataArithmetic::kIsArithmeticShort: currentTypeVariable.SetArithmeticValue(StringConvertToShort( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: currentTypeVariable.SetArithmeticValue(StringConvertToInt( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: currentTypeVariable.SetArithmeticValue(StringConvertToLong( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: currentTypeVariable.SetArithmeticValue(StringConvertToInt64( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;							
									case TypeVariableDataArithmetic::kIsArithmeticFloat: currentTypeVariable.SetArithmeticValue(StringConvertToFloat( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									case TypeVariableDataArithmetic::kIsArithmeticDouble: currentTypeVariable.SetArithmeticValue(StringConvertToDouble( variableName.AsCStr() ), instance, i, isArthmeticPointer); break;
									default: DIA_ASSERT(false, "Could not recognize type"); break;
								}
							}
						}		

						ConsumeArraySeperator(currentTypeVariable, i);
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeAllUntilFirst(char untilCharachter)
				{
					char current = mBuffer.GetCurrent();
					while (current != untilCharachter)
					{
						mBuffer.Advance();
						current = mBuffer.GetCurrent();
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeArrayHeader(const TypeVariable& currentTypeVariable)
				{
					if (currentTypeVariable.IsArrayType())
					{
						ConsumeSpecialCharacter();
						
						HeaderStrings headerStrings;
						ConsumeHeaderStrings(headerStrings);

						ConsumeSpecialCharacter();
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeArraySeperator(const TypeVariable& currentTypeVariable, unsigned int element)
				{
					if (currentTypeVariable.IsArrayType())
					{
						ConsumeWhiteSpace();
							
						unsigned int lastElement = currentTypeVariable.GetNumberOfElements() - 1;
						if (element == lastElement && mBuffer.GetCurrent() == ',')
						{
							mBuffer.Advance();
						}
						else
						{
							DIA_ASSERT(mBuffer.GetCurrent() == ',', "Must end each line of an array with \",\"");
							mBuffer.Advance();
						}

						ConsumeWhiteSpace();
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeCharacterString(Containers::String128& variableName)
				{
					DIA_ASSERT(IsAlphabeticalCharacter( mBuffer.GetCurrent() ) || IsDigitCharacter( mBuffer.GetCurrent() ), "Char must be on digit or alphabetic char");

					const char* startPtr = mBuffer.GetCurrentPtr();
					
					unsigned int numberChars = 0;
					while ( IsAlphabeticalCharacter( mBuffer.GetCurrent() ) || IsDigitCharacter( mBuffer.GetCurrent() ))
					{
						mBuffer.Advance();
						numberChars++;
					}

					variableName.Append( startPtr, numberChars );
				}

				//------------------------------------------------------------------------------------
				void ConsumeNumberString(Containers::String64& variableName)
				{
					DIA_ASSERT(IsAlphabeticalCharacter( mBuffer.GetCurrent() ) || IsDigitCharacter( mBuffer.GetCurrent() ) || mBuffer.GetCurrent() == '.' || mBuffer.GetCurrent() == '-', "Char [%s] must be on digit or alphabetic char", mBuffer.AsCStr());

					const char* startPtr = mBuffer.GetCurrentPtr();

					unsigned int numberChars = 0;
					while ( IsDigitCharacter( mBuffer.GetCurrent() ) || mBuffer.GetCurrent() == '.' || mBuffer.GetCurrent() == '-' )
					{
						mBuffer.Advance();
						numberChars++;
					}

					variableName.Append( startPtr, numberChars );
				}

				//------------------------------------------------------------------------------------
				void ConsumeHeaderStrings( HeaderStrings& headerStrings)
				{
					ConsumeWhiteSpace();

					DIA_ASSERT(mBuffer.GetCurrent() == '@', "Current buffer char is %c, not expected @", mBuffer.GetCurrent());

					mBuffer.Advance();

					const char* startPtr = mBuffer.GetCurrentPtr();

					unsigned int numberChars = 0;

					int numberOfCharFound = 0;
					while (1)
					{	
						DIA_ASSERT(numberOfCharFound < (64*1024), "Header Data reader is caught in infinite loop");
						ConsumeWhiteSpace();

						if(mBuffer.GetCurrent() == '@')
						{
							Containers::String64 tempStr;
							tempStr.Append( startPtr, numberChars );

							unsigned int colon_index = tempStr.Find(':');
							DIA_ASSERT(colon_index != -1, "Could not find : in %s", tempStr.AsCStr());
							
							Containers::String32 key;
							Containers::String64 value;

							tempStr.Split(colon_index, key, value);
							value.Trim(1);

							headerStrings.AddToList( key, value );
							
							mBuffer.Advance();
							break;
						}

						if (mBuffer.GetCurrent() == ',')
						{
							Containers::String64 tempStr;
							tempStr.Append( startPtr, numberChars );

							unsigned int colon_index = tempStr.Find(':');
							DIA_ASSERT(colon_index != -1, "Could not find : in %s", tempStr.AsCStr());
							
							Containers::String32 key;
							Containers::String64 value;

							tempStr.Split(colon_index, key, value);
							value.Trim(1);

							headerStrings.AddToList( key, value );

							startPtr = mBuffer.GetCurrentPtr() + 1;
							numberChars = 0;
							
							mBuffer.Advance();

							while (mBuffer.IsInBounds() && (mBuffer.GetCurrent() == ',' || IsWhiteSpace(mBuffer.GetCurrent())))
							{
								mBuffer.Advance();
							}
						}
						else
						{
							mBuffer.Advance();
							numberChars++;
						}
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeSpecialCharacter()
				{
					while (mBuffer.IsInBounds() && IsSpecialCharacter(mBuffer.GetCurrent()))
					{
						if (mBuffer.GetCurrent() == '{' || mBuffer.GetCurrent() == '[')
						{
							mDepth++;
						}
						else if (mBuffer.GetCurrent() == '}' || mBuffer.GetCurrent() == ']')
						{
							mDepth--;
						}

						mBuffer.Advance();
					}
				}

				//------------------------------------------------------------------------------------
				void ConsumeWhiteSpace()
				{
					while (mBuffer.IsInBounds() && IsWhiteSpace(mBuffer.GetCurrent()))
					{
						mBuffer.Advance();
					}
				}

				//------------------------------------------------------------------------------------
				bool IsWhiteSpace(char value)
				{
					return (msWhitespaceChars.FindIndex( value ) != -1);
				}

				//------------------------------------------------------------------------------------
				bool IsSpecialCharacter(char value)
				{
					return (msSpecialChars.FindIndex( value ) != -1);
				}

				//------------------------------------------------------------------------------------
				bool IsAlphabeticalCharacter(char value)
				{
					return ((isalpha(value)) != 0);
				}

				//------------------------------------------------------------------------------------
				bool IsDigitCharacter(char value)
				{
					return ((isdigit (value)) != 0);
				}
				
				struct PtrFixupStruct
				{
					const char* mPath;

					void* mPointeeAsType;
					unsigned int mOffsetFromPointee;
				};

				static Containers::DynamicArrayC<char, 4> msWhitespaceChars;
				static Containers::DynamicArrayC<char, 16> msSpecialChars;
				
				int mDepth;
				Containers::StringReader	mBuffer;
				Containers::DynamicArrayC<PtrFixupStruct, 1024> mPtrFixupList;
			};
			
			Containers::DynamicArrayC<char, 4> TextDeserializerInternal::msWhitespaceChars;
			Containers::DynamicArrayC<char, 16> TextDeserializerInternal::msSpecialChars;

			//------------------------------------------------------------------------------------
			//	TypeTextSerializer
			//------------------------------------------------------------------------------------

			TypeTextSerializer::TypeTextSerializer()
			{}

			//------------------------------------------------------------------------------------
			void TypeTextSerializer::Initilize(const TypeRegistry* registry)
			{
				mRegistry = registry;
			}

			//------------------------------------------------------------------------------------
			void TypeTextSerializer::Serialize(const TypeInstance& instance, Containers::StringWriter& buffer)
			{
				TextSerializerInternal serializer(buffer);

				serializer.Serialize(instance);
			}

			//------------------------------------------------------------------------------------
			void TypeTextSerializer::Deserialize(TypeInstance& instance, Containers::StringReader& buffer)
			{
				TextDeserializerInternal deserializer(buffer);		
				deserializer.Deserialize(instance);
			}
		}
	}
}