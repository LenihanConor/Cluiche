#include "DiaCore/Type/TypeJsonSerializer.h"

#include "DiaCore/Type/TypeInstance.h"
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Strings/stringutils.h"
#include "DiaCore/Containers/Strings/StringWriter.h"
#include "DiaCore/Containers/Strings/StringReader.h"
#include "DiaCore/Strings/String32.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/Core/Log.h"
#include "DiaCore/Json/external/json/json.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{

			const char* TypeJsonSerializer::MetaData::sMetaDataArray[] = { "_class_name", "_crc", "_number_element" };

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
					unsigned int hashID = instance.GetTypeDescriptor()->GetUniqueCRC();
					const char* name = instance.GetTypeDescriptor()->GetName();

					jsonStructureToSerialize[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)] = name;
					jsonStructureToSerialize[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::CRC)] = hashID;
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
						bool isCustomSerializer = currentTypeVariable.HasAttribute<TypeVariableAttributesCustomJsonSerializer>();

						DIA_ASSERT(isArithmeticType || isClassType || isPointerType, "Do not recognize type info on %s", currentTypeVariable.GetName());
	
						if(isCustomSerializer)
						{ 
							currentTypeVariable.GetAttributeConst<TypeVariableAttributesCustomJsonSerializer>()->Serialize(instance, currentTypeVariable, jsonData);
						}
						else if (isArithmeticType)
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
				
					unsigned int size = currentTypeVariable.GetNumberOfElements();
					for (unsigned int i = 0; i < size; i++)
					{
						if (isUnsigned)
						{
							switch (type)
							{
							case TypeVariableDataArithmetic::kIsArithmeticBool: 
								if(size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<bool>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<bool>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticChar: 
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<unsigned char>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned char>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticShort:
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<unsigned short>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned short>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticInt: 
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<unsigned int>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<unsigned int>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLong:
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<unsigned long>(instance, i), valueString);
									if (size > 1)
										jsonData[currentTypeVariable.GetName()][i] = valueString.AsCStr();
									else
										jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLongLong:
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<unsigned long long>(instance, i), valueString);
									if (size > 1)
										jsonData[currentTypeVariable.GetName()][i] = valueString.AsCStr();
									else
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
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<char>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<char>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticShort:
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<short>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<short>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticInt: 
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<int>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<int>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLong: 
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<long>(instance, i), valueString);
									if (size > 1)
										jsonData[currentTypeVariable.GetName()][i] = valueString.AsCStr();
									else
										jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
								{
									Containers::String64 valueString;
									Convert(currentTypeVariable.GetArithmeticValue<long long>(instance, i), valueString);
									if (size > 1)
										jsonData[currentTypeVariable.GetName()][i] = valueString.AsCStr();
									else
										jsonData[currentTypeVariable.GetName()] = valueString.AsCStr();
								}
								break;
							case TypeVariableDataArithmetic::kIsArithmeticFloat:
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue<float>(instance, i);
								else
									jsonData[currentTypeVariable.GetName()] = currentTypeVariable.GetArithmeticValue<float>(instance, i);
								break;
							case TypeVariableDataArithmetic::kIsArithmeticDouble:
								if (size > 1)
									jsonData[currentTypeVariable.GetName()][i] = currentTypeVariable.GetArithmeticValue < double > (instance, i);
								else
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
						
						newClassJsonData[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)] = className;
						newClassJsonData[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::CRC)] = hashID;

						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i));

						WriteVariables(newInstance, newClassJsonData);
						if (currentTypeVariable.GetNumberOfElements() > 1)
							jsonData[name][i] = newClassJsonData;
						else
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
					DIA_ASSERT_SUPPORT(Dia::Core::Containers::String64 name(parsedFromString[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)].asString().c_str()));
					DIA_ASSERT_SUPPORT(unsigned int hashID = parsedFromString[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::CRC)].asUInt());
					DIA_ASSERT(name == instance.GetTypeDescriptor()->GetName(), "An object of [%s] is trying to deserialise into object of [%s]", parsedFromString[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)].asString().c_str(), instance.GetTypeDescriptor()->GetName());
					DIA_ASSERT(hashID == instance.GetTypeDescriptor()->GetUniqueCRC(), "%s crc is not correct", parsedFromString[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)].asString().c_str());
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
						bool isMetaData = memberName[0] == TypeJsonSerializer::MetaData::sMetaDataPrefix;
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
						bool isCustomDerserializer = currentTypeVariable.HasAttribute<TypeVariableAttributesCustomJsonDeserializer>();
						
						if (isCustomDerserializer)
						{
							currentTypeVariable.GetAttributeConst<TypeVariableAttributesCustomJsonDeserializer>()->Deserialize(instance, currentTypeVariable, currentJsonValue);
						}
						else if (isArithmeticType || isPointerActingAsArthmethicObject)
						{
							ReadArithmeticType(instance, currentTypeVariable, currentJsonValue);
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
					unsigned int size = currentTypeVariable.GetNumberOfElements();
					for (unsigned int i = 0; i < size; i++)
					{
						TypeInstance newInstance(currentTypeVariable.GetClassDefinition(), currentTypeVariable.GetClassPointee(instance, i) );

						unsigned int hashID = 0;
						std::string strTemp;
						if (size > 1)
						{
							strTemp = jsonData[i][TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)].asString();
							hashID = jsonData[i][TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::CRC)].asUInt();
						}
						else
						{
							strTemp = jsonData[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::ClassName)].asString();
							hashID = jsonData[TypeJsonSerializer::MetaData::GetMetaData(TypeJsonSerializer::MetaData::EFlagName::CRC)].asUInt();
						}
						const char* className = strTemp.c_str();

						DIA_ASSERT_SUPPORT(Dia::Core::Containers::String64 name(className));
						DIA_ASSERT(name == newInstance.GetTypeDescriptor()->GetName(), "An object of [%s] is trying to deserialise into object of [%s]", className, newInstance.GetTypeDescriptor()->GetName());
						DIA_ASSERT(hashID == newInstance.GetTypeDescriptor()->GetUniqueCRC(), "%s crc is not correct", className);
						
						if (size > 1)
						{
							ReadVariable(newInstance, jsonData[i]);
						}
						else
						{
							ReadVariable(newInstance, jsonData);
						}
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
				void ReadArithmeticType( TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& val)
				{
					bool isArthmeticPointer = currentTypeVariable.IsPointerArthmeticType() && currentTypeVariable.HasAttribute<TypeVariableAttributesPointerAsObject>();

					unsigned int size = currentTypeVariable.GetNumberOfElements();
					for (unsigned int i = 0; i < size; i++)
					{						
						bool isBool = currentTypeVariable.IsArithmeticBool();
						bool isChar = currentTypeVariable.IsArithmeticChar();
						
						if (isBool)
						{
							if (size > 1)
								currentTypeVariable.SetArithmeticValue(val[i].asBool(), instance, i, isArthmeticPointer);
							else
								currentTypeVariable.SetArithmeticValue(val.asBool(), instance, i, isArthmeticPointer);
						}
						else if (isChar)
						{
							if (size > 1)
								currentTypeVariable.SetArithmeticValue(static_cast<char>(val[i].asInt()), instance, i, isArthmeticPointer);
							else
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
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(static_cast<unsigned short>(val[i].asInt()), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(static_cast<unsigned short>(val.asInt()), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(val[i].asUInt(), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(val.asUInt(), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(StringConvertToULong(val[i].asString().c_str()), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(StringConvertToULong( val.asString().c_str() ), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(StringConvertToUInt64(val[i].asString().c_str()), instance, i, isArthmeticPointer);
										else
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
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(static_cast<short>(val[i].asInt()), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(static_cast<short>(val.asInt()), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticInt: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(val[i].asInt(), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(val.asInt(), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLong: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(StringConvertToLong(val[i].asString().c_str()), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(StringConvertToLong(val.asString().c_str()), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticLongLong: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(StringConvertToInt64(val[i].asString().c_str()), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(StringConvertToInt64(val.asString().c_str()), instance, i, isArthmeticPointer);
										break;							
									case TypeVariableDataArithmetic::kIsArithmeticFloat: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(val[i].asFloat(), instance, i, isArthmeticPointer);
										else
											currentTypeVariable.SetArithmeticValue(val.asFloat(), instance, i, isArthmeticPointer);
										break;
									case TypeVariableDataArithmetic::kIsArithmeticDouble: 
										if (size > 1)
											currentTypeVariable.SetArithmeticValue(val[i].asDouble(), instance, i, isArthmeticPointer);
										else
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