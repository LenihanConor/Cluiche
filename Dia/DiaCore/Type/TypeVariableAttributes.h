#ifndef DIA_TYPE_VARIABLE_ATTRIBUTES_H
#define DIA_TYPE_VARIABLE_ATTRIBUTES_H

#include "DiaCore/CRC/StripStringCRC.h"

#include <functional>

namespace Json { class Value; }

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			class TypeVariable;
			class TypeInstance;

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributes
			//---------------------------------------------------------------------------------------------------------
			class TypeVariableAttributes
			{
			public:

				TypeVariableAttributes(){};
				virtual ~TypeVariableAttributes(){};
				
				virtual const StripStringCRC& GetUniqueID()const = 0;
				virtual void AssignedTo(const TypeVariable& typeVariable) = 0;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesPointerAsObject
			//---------------------------------------------------------------------------------------------------------
			// Changes the serialization behavior of a pointer to act like an object
			class TypeVariableAttributesPointerAsObject : public TypeVariableAttributes
			{
			public:
				TypeVariableAttributesPointerAsObject();	 
				
				static const StripStringCRC mAttributeID;
				static const StripStringCRC&	GetStaticUniqueID(){return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const{ return mAttributeID;};
				virtual void AssignedTo(const TypeVariable& typeVariable);
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesCustomJsonSerializer
			//---------------------------------------------------------------------------------------------------------
			// Allow the function to handle serialization differently
			class TypeVariableAttributesCustomJsonSerializer : public TypeVariableAttributes
			{
			public:
				typedef std::function<void(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData)> CustomSerializer;

				TypeVariableAttributesCustomJsonSerializer(CustomSerializer func);

				static const StripStringCRC mAttributeID;
				static const StripStringCRC&	GetStaticUniqueID() { return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const override { return mAttributeID; };
				virtual void AssignedTo(const TypeVariable& typeVariable)override {};

				void Serialize(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData)const;

			private:
				CustomSerializer mFuncHandler;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesCustomJsonSerializer
			//---------------------------------------------------------------------------------------------------------
			// Allow the function to handle serialization differently
			class TypeVariableAttributesCustomJsonDeserializer : public TypeVariableAttributes
			{
			public:
				typedef std::function<void(TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData)> CustomDeserializer;

				TypeVariableAttributesCustomJsonDeserializer(CustomDeserializer func);

				static const StripStringCRC mAttributeID;
				static const StripStringCRC&	GetStaticUniqueID() { return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const override { return mAttributeID; };
				virtual void AssignedTo(const TypeVariable& typeVariable)override {};

				void Deserialize(TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData)const;

			private:
				CustomDeserializer mFuncHandler;
			};
		}
	}
}

#endif // DIA_ASSERT