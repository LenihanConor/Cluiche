#ifndef DIA_TYPE_VARIABLE_ATTRIBUTES_H
#define DIA_TYPE_VARIABLE_ATTRIBUTES_H

#include "DiaCore/CRC/StripStringCRC.h"
#include "DiaCore/CRC/StringCRC.h"

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
			class TypeJsonSerializerExternalSerializeInterface;
			class TypeJsonSerializerExternalDeserializeInterface;

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
				typedef std::function<void(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData, TypeJsonSerializerExternalSerializeInterface& parent)> CustomSerializer;

				TypeVariableAttributesCustomJsonSerializer(CustomSerializer func);

				static const StripStringCRC mAttributeID;
				static const StripStringCRC& GetStaticUniqueID() { return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const override { return mAttributeID; };
				virtual void AssignedTo(const TypeVariable& typeVariable)override {};

				void Serialize(const TypeInstance& instance, const TypeVariable& currentTypeVariable, Json::Value& jsonData, TypeJsonSerializerExternalSerializeInterface& parent)const;

			private:
				CustomSerializer mFuncHandler;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributeRequired
			//---------------------------------------------------------------------------------------------------------
			// Marks a field as required — deserializers that support validation will fail if this field is missing
			class TypeVariableAttributeRequired : public TypeVariableAttributes
			{
			public:
				TypeVariableAttributeRequired();

				static const StripStringCRC mAttributeID;
				static const StripStringCRC& GetStaticUniqueID() { return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const override { return mAttributeID; };
				virtual void AssignedTo(const TypeVariable& typeVariable) override {};
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributeAssetReference
			//---------------------------------------------------------------------------------------------------------
			// Marks a field as referencing another asset by its StringCRC type+name ID.
			// The target type ID identifies which asset type the field points at.
			// Any field type may carry this attribute — no assertion is raised in AssignedTo.
			class TypeVariableAttributeAssetReference : public TypeVariableAttributes
			{
			public:
				explicit TypeVariableAttributeAssetReference(const Dia::Core::StringCRC& targetTypeId);

				const Dia::Core::StringCRC& GetTargetTypeId() const;

				static const StripStringCRC mAttributeID;
				static const StripStringCRC& GetStaticUniqueID() { return mAttributeID; }
				virtual const StripStringCRC& GetUniqueID() const override { return mAttributeID; }
				virtual void AssignedTo(const TypeVariable& typeVariable) override {} // any field can reference an asset

			private:
				Dia::Core::StringCRC mTargetTypeId;
			};

			//---------------------------------------------------------------------------------------------------------
			// TypeVariableAttributesCustomJsonSerializer
			//---------------------------------------------------------------------------------------------------------
			// Allow the function to handle serialization differently
			class TypeVariableAttributesCustomJsonDeserializer : public TypeVariableAttributes
			{
			public:
				typedef std::function<void(TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData, TypeJsonSerializerExternalDeserializeInterface& parent)> CustomDeserializer;

				TypeVariableAttributesCustomJsonDeserializer(CustomDeserializer func);

				static const StripStringCRC mAttributeID;
				static const StripStringCRC&	GetStaticUniqueID() { return mAttributeID; };
				virtual const StripStringCRC& GetUniqueID()const override { return mAttributeID; };
				virtual void AssignedTo(const TypeVariable& typeVariable)override {};

				void Deserialize(TypeInstance& instance, const TypeVariable& currentTypeVariable, const Json::Value& jsonData, TypeJsonSerializerExternalDeserializeInterface& parent)const;

			private:
				CustomDeserializer mFuncHandler;
			};
		}
	}
}

#endif // DIA_ASSERT