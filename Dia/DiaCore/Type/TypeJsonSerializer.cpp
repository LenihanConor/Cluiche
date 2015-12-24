#include "DiaCore/Type/TypeJsonSerializer.h"

#include "DiaCore/Type/TypeInstance.h"
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Strings/stringutils.h"
#include "DiaCore/Containers/Strings/StringWriter.h"
#include "DiaCore/Containers/Strings/StringReader.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Core/EnumClass.h"
#include "DiaCore/Core/Log.h"
#include "DiaCore/Json/external/json/json.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	MetaData
			//------------------------------------------------------------------------------------
			class MetaData
			{
			public:
				static const char sMetaDataPrefix = '_';

				CLASSEDENUM (EFlagName,\
					CE_ITEMVAL(Unknown, -1)\
					CE_ITEMVAL(ClassName, 0)\
					CE_ITEM(CRC)\
					CE_ITEM(NumberElements),\
					Unknown \
					);			
				
				static const Containers::String32& GetMetaData(EFlagName flag)
				{
					return sMetaDataArray[flag];
				}
				
			private:
				static const Containers::String32 sMetaDataArray[EFlagName::NumberOfItems];
			};
			
			const Containers::String32 MetaData::sMetaDataArray[] = {"_class_name", "_crc", "_number_element"};

			//------------------------------------------------------------------------------------
			//	JsonSerializerInternal
			//------------------------------------------------------------------------------------
			class JsonSerializerInternal
			{
				DIA_NON_COPYABLE(JsonSerializerInternal);

			public:
				
				JsonSerializerInternal()
				{}
					
				void Serialize(const TypeInstance& instance, Containers::StringWriter& outBuffer)
				{
					Json::Value jsonStructureToSerialize;

					WriteInstanceDeclaration(instance, jsonStructureToSerialize);
					WriteVariables(instance, jsonStructureToSerialize);

					// write in a nice readible way
					Json::StyledWriter styledWriter;
					outBuffer << styledWriter.write(jsonStructureToSerialize).c_str();
				}
			
			private:
				void WriteInstanceDeclaration(const TypeInstance& instance, Json::Value& jsonStructureToSerialize)
				{
					unsigned int hashID = instance.GetTypeDescriptor()->GetUniqueCRC().Value();
					const char* name = instance.GetTypeDescriptor()->GetName();

					jsonStructureToSerialize[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()] = name;
					jsonStructureToSerialize[MetaData::GetMetaData(MetaData::EFlagName::CRC).AsCStr()] = hashID;
				}

				//------------------------------------------------------------------------------------
				void WriteVariables(const TypeInstance& instance, Json::Value& jsonData)
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
				/*		if (currentTypeVariable.IsArrayType())
						{
							const char* name = currentTypeVariable.GetName();

							Containers::String32 tabs;
							CreateTabs(tabs);

							mBuffer << tabs.AsCStr() << name << " @" << HeaderStrings::GetFlag(HeaderStrings::EFlagName::NumberElements).AsCStr() << ":" << currentTypeVariable.GetNumberOfElements() << "@\n";
							WriteOpenScope();
						}*/

						if (isArithmeticType)
						{
							WriteArithmeticType(instance, currentTypeVariable, jsonData);
						}
						else if (isClassType)
						{
							WriteClassType(instance, currentTypeVariable, jsonData);
						}
						else if (isPointerType)
						{
							DIA_ASSERT(0, "Current do not support pointer types in Json serilaizer");

							/*if (currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>())
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
							}*/
						}
						
						currentNode = currentNode->GetNextConst();
					}
				}

				//------------------------------------------------------------------------------------
				void WriteArithmeticType(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData)
				{
					bool isUnsigned = currentTypeVariable.IsArithmeticUnsigned();

					TypeVariableDataArithmetic::ArithmeticType type = currentTypeVariable.GetArithmeticType();

					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						if (isUnsigned)
						{
							switch (type)
							{
							case TypeVariableDataArithmetic::kIsArithmeticBool: 
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<bool>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticChar: 
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned char>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticShort:
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned short>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticInt: 
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned int>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLong:
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<unsigned long>(instance, i), valueString);
									jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLongLong:
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<unsigned long long>(instance, i), valueString);
									jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							default: DIA_ASSERT(false, "Could not recognize unsigned type"); break;
							}
						}
						else
						{
							switch (type)
							{
							case TypeVariableDataArithmetic::kIsArithmeticChar:
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<char>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticShort:
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<short>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticInt: 
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<int>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLong: 
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<long>(instance, i), valueString);
									jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<long long>(instance, i), valueString);
									jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticFloat:
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<float>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticDouble:
								jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<double>(instance, i);
								break;
							default: DIA_ASSERT(false, "Could not recognize type"); break;
							}
						}
					}
				}

				//------------------------------------------------------------------------------------
				void WriteClassType(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData)
				{
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						unsigned int hashID = currentTypeVariable.GetClassDefinition()->GetUniqueCRC();
						const char* name = currentTypeVariable.GetName();
						const char* className = currentTypeVariable.GetClassDefinition()->GetName();

						Json::Value newClassJsonData;
						
						newClassJsonData[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()] = className;
						newClassJsonData[MetaData::GetMetaData(MetaData::EFlagName::CRC).AsCStr()] = hashID;

						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i));

						WriteVariables(newInstance, newClassJsonData);
						
						jsonData[name] = newClassJsonData;
					}
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
			};

			//------------------------------------------------------------------------------------
			//	JsonDeserializerInternal
			//------------------------------------------------------------------------------------
			class JsonDeserializerInternal
			{
				DIA_NON_COPYABLE(JsonDeserializerInternal);

			public:
				JsonDeserializerInternal()
				{}

				void Deserialize(TypeInstance& instance, Containers::StringReader& buffer)
				{
//					mPtrFixupList.RemoveAll();

					Json::Value parsedFromString;
					Json::Reader reader;
					bool parsingSuccessful = reader.parse(buffer.AsCStr(), parsedFromString, false);
					if (!parsingSuccessful)
					{
						DIA_ASSERT(0, "Faild to parse JSON. Error: %s", reader.getFormatedErrorMessages());
						return;
					}

					ReadInstanceDeclaration(instance, parsedFromString);
					
					ReadVariable(instance, parsedFromString);

					/*

					PointerFixup( instance );*/
				}

			private:

				//------------------------------------------------------------------------------------
				void ReadInstanceDeclaration(const TypeInstance& instance, const Json::Value& parsedFromString)
				{	
					DIA_ASSERT_SUPPORT(Dia::Core::Containers::String64 name(parsedFromString[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str()));
					DIA_ASSERT_SUPPORT(unsigned int hashID = parsedFromString[MetaData::GetMetaData(MetaData::EFlagName::CRC).AsCStr()].asUInt());
					DIA_ASSERT(name == instance.GetTypeDescriptor()->GetName(), "An object of [%s] is trying to deserialise into object of [%s]", parsedFromString[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str(), instance.GetTypeDescriptor()->GetName());
					DIA_ASSERT(hashID == instance.GetTypeDescriptor()->GetUniqueCRC().Value(), "%s crc is not correct", parsedFromString[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str());
				}
				
				//------------------------------------------------------------------------------------
				void ReadVariable(TypeInstance& instance, const Json::Value& data)
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
					
					for (Json::ValueConstIterator it = data.begin(); it != data.end(); ++it)
					{
						const Json::Value& currentJsonValue = *it;
						
						const char* memberName = it.memberName();

						// If it is meta data then ignore							
						bool isMetaData = memberName[0] == MetaData::sMetaDataPrefix;
						if (isMetaData)
						{
							continue;
						}

						CRC variableNameCRC(memberName);

						const TypeDefinition::VariableLinkListNode* variableLink = instance.GetVariables().FindConst(variableNameCRC, variableNameSearchFunctor);

						DIA_ASSERT(variableLink, "Could not find %s", it.memberName());
						
						const TypeVariable& currentTypeVariable = *variableLink->GetPayloadConst();

						bool isArithmeticType = currentTypeVariable.IsArithmeticType();
						bool isClassType = currentTypeVariable.IsClassType();
						bool isPointerType = currentTypeVariable.IsPointerType();
						bool isPointerActingAsArthmethicObject = currentTypeVariable.IsPointerArthmeticType() && currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>();
						bool isPointerActingAsClassObject = currentTypeVariable.IsPointerClassType() && currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>();

						if (isArithmeticType || isPointerActingAsArthmethicObject)
						{
							ReadArithmeticType(instance, currentTypeVariable, currentJsonValue, isPointerActingAsArthmethicObject);
						}
						else if (isClassType || isPointerActingAsClassObject)
						{
							ReadClassType( instance, currentTypeVariable, currentJsonValue );
						}
						else if (isPointerType)
						{
							DIA_ASSERT(currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>() == false, "Has PointerAsObject Attribute but is treating like pointer");
							ReadPointerType( instance, currentTypeVariable );
						}
					}
				}
				
				//------------------------------------------------------------------------------------
				void PointerFixup(TypeInstance& instance)
				{
			/*		for (unsigned int i =0; i < mPtrFixupList.Size(); i++)
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
								crcPath.Add(newCrc);
							}

							numberCharacters++;
							reader.Advance();
						}					

						CRC newCrc( startPtr, numberCharacters);
						crcPath.Add(newCrc);

						unsigned int addressResult = 0;
						instance.FindPointerAddressFromVariableCRCPath( crcPath, addressResult );
						
						char* pointeeAsType = reinterpret_cast<char*>(tempStruct.mPointeeAsType);
						void** pointeeValue = reinterpret_cast<void**>(((tempStruct.mOffsetFromPointee / sizeof (char)) + pointeeAsType));

						*pointeeValue = reinterpret_cast<void*>(addressResult);
					}

					mPtrFixupList.RemoveAll();*/
				}

				//------------------------------------------------------------------------------------
				void ReadClassType( TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData)
				{
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i) );

						DIA_ASSERT_SUPPORT(Dia::Core::Containers::String64 name(jsonData[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str()));
						DIA_ASSERT_SUPPORT(unsigned int hashID = jsonData[MetaData::GetMetaData(MetaData::EFlagName::CRC).AsCStr()].asUInt());
						DIA_ASSERT(name == newInstance.GetTypeDescriptor()->GetName(), "An object of [%s] is trying to deserialise into object of [%s]", jsonData[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str(), newInstance.GetTypeDescriptor()->GetName());
						DIA_ASSERT(hashID == newInstance.GetTypeDescriptor()->GetUniqueCRC().Value(), "%s crc is not correct", jsonData[MetaData::GetMetaData(MetaData::EFlagName::ClassName).AsCStr()].asString().c_str());
						
						ReadVariable(newInstance, jsonData);
					}
				}
				
				//------------------------------------------------------------------------------------
				void ReadPointerType( TypeInstance& instance, const TypeVariable& currentTypeVariable)
				{
					DIA_ASSERT(0, "Current do not support pointer types in Json deserilaizer");

//					ConsumeArrayHeader(currentTypeVariable);

/*					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{
						while (mBuffer.IsInBounds() && mBuffer.GetCurrent() != '=')
						{
							mBuffer.Advance();
						}

						mBuffer.Advance();

	//					ConsumeWhiteSpace();
					
						PtrFixupStruct newPtrFixupStruct;

						newPtrFixupStruct.mPath = mBuffer.GetCurrentPtr();	
						newPtrFixupStruct.mPointeeAsType = instance.Pointee();

						newPtrFixupStruct.mOffsetFromPointee = currentTypeVariable.GetOffsetFromParent(i);
						
						mPtrFixupList.Add(newPtrFixupStruct);

						while (mBuffer.IsInBounds() && ( IsAlphabeticalCharacter( mBuffer.GetCurrent() ) || IsDigitCharacter( mBuffer.GetCurrent() ) || mBuffer.GetCurrent() == '.' ) )
						{
							mBuffer.Advance();
						}

		//				ConsumeArraySeperator(currentTypeVariable, i);
					}*/
				}

				//------------------------------------------------------------------------------------
				void ReadArithmeticType( TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& val, bool isArthmeticPointer )
				{
					for (unsigned int i = 0; i < currentTypeVariable.GetNumberOfElements(); i++)
					{						
						bool isBool = currentTypeVariable.IsArithmeticBool();
						bool isChar = currentTypeVariable.IsArithmeticChar();
						
						if (isBool)
						{
							currentTypeVariable.SetArithmeticValue(val.asBool(), instance, i, isArthmeticPointer);
						}
						else if (isChar)
						{
							currentTypeVariable.SetArithmeticValue(static_cast<char>(val.asInt()), instance, i, isArthmeticPointer);
						}
						else 
						{
							bool isUnsigned = currentTypeVariable.IsArithmeticUnsigned();
							
							TypeVariableDataArithmetic::ArithmeticType type = currentTypeVariable.GetArithmeticType();
				
							if (isUnsigned)
							{
								switch (type)
								{
									case TypeVariableDataArithmetic::kIsArithmeticShort: 
										currentTypeVariable.SetArithmeticValue(static_cast<unsigned short>(val.asInt()), instance, i, isArthmeticPointer); 
										break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: 
										currentTypeVariable.SetArithmeticValue(val.asUInt(), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: 
										currentTypeVariable.SetArithmeticValue(StringConvertToULong( val.asString().c_str() ), instance, i, isArthmeticPointer); 
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
										currentTypeVariable.SetArithmeticValue(StringConvertToUInt64( val.asString().c_str()), instance, i, isArthmeticPointer);
										break;
									default: DIA_ASSERT(false, "Could not recognize unsigned type"); break;
								}
							}
							else
							{
								switch (type)
								{
									case TypeVariableDataArithmetic::kIsArithmeticShort: 
										currentTypeVariable.SetArithmeticValue(static_cast<short>(val.asInt()), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: 
										currentTypeVariable.SetArithmeticValue(val.asInt(), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: 
										currentTypeVariable.SetArithmeticValue(StringConvertToLong(val.asString().c_str()), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
										currentTypeVariable.SetArithmeticValue(StringConvertToInt64(val.asString().c_str()), instance, i, isArthmeticPointer);
										break;							
									case TypeVariableDataArithmetic::kIsArithmeticFloat: 
										currentTypeVariable.SetArithmeticValue(val.asFloat(), instance, i, isArthmeticPointer); 
										break;
									case TypeVariableDataArithmetic::kIsArithmeticDouble: 
										currentTypeVariable.SetArithmeticValue(val.asDouble(), instance, i, isArthmeticPointer); 
										break;
									default: DIA_ASSERT(false, "Could not recognize type"); break;
								}
							}
						}		
					}
				}
				
				struct PtrFixupStruct
				{
					const char* mPath;

					void* mPointeeAsType;
					unsigned int mOffsetFromPointee;
				};

				Containers::DynamicArrayC<PtrFixupStruct, 1024> mPtrFixupList;
			};

			//------------------------------------------------------------------------------------
			//	TypeJsonSerializer
			//------------------------------------------------------------------------------------

			TypeJsonSerializer::TypeJsonSerializer()
			{}

			//------------------------------------------------------------------------------------
			void TypeJsonSerializer::Initilize(const TypeRegistry* registry)
			{
				mRegistry = registry;
			}

			//------------------------------------------------------------------------------------
			void TypeJsonSerializer::Serialize(const TypeInstance& instance, Containers::StringWriter& buffer)
			{
				JsonSerializerInternal serializer;

				serializer.Serialize(instance, buffer);
			}

			//------------------------------------------------------------------------------------
			void TypeJsonSerializer::Deserialize(TypeInstance& instance, Containers::StringReader& buffer)
			{
				JsonDeserializerInternal deserializer;
				deserializer.Deserialize(instance, buffer);
			}
		}
	}
}