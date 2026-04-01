---
schema: dia.module.v1
module_id: dia.core.type
name: Type
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaCore/Type
language: cpp
parent_module_id: dia.core

summary: >
  Defines Type APIs (BasicTypeDefines, TypeDeclarationMacros, TypeDefinition, TypeDefinitionMacros, TypeFacade, TypeInstance, TypeJsonSerializer, TypeMember, TypeParameterInput, TypeRegistry, TypeTextSerializer, TypeTraits, TypeVariable, TypeVariableAttributes, TypeVariableData) including: IntegralConstant, MetaData, null_t, StringReader, StringWriter, TypeConstant, TypeDefinition, TypeFacade, TypeInstance, TypeJsonSerializer, TypeJsonSerializerExternalDeserializeInterface, TypeJsonSerializerExternalSerializeInterface.

intent: >
  Provide reusable Type building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: IntegralConstant, MetaData, null_t, StringReader, StringWriter, TypeConstant, TypeDefinition, TypeFacade, TypeInstance, TypeJsonSerializer, TypeJsonSerializerExternalDeserializeInterface, TypeJsonSerializerExternalSerializeInterface
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

public_api:
  headers:
    - Dia/DiaCore/Type/BasicTypeDefines.h
    - Dia/DiaCore/Type/TypeDeclarationMacros.h
    - Dia/DiaCore/Type/TypeDefinition.h
    - Dia/DiaCore/Type/TypeDefinitionMacros.h
    - Dia/DiaCore/Type/TypeFacade.h
    - Dia/DiaCore/Type/TypeInstance.h
    - Dia/DiaCore/Type/TypeJsonSerializer.h
    - Dia/DiaCore/Type/TypeMember.h
    - Dia/DiaCore/Type/TypeParameterInput.h
    - Dia/DiaCore/Type/TypeRegistry.h
    - Dia/DiaCore/Type/TypeTextSerializer.h
    - Dia/DiaCore/Type/TypeTraits.h
    - Dia/DiaCore/Type/TypeVariable.h
    - Dia/DiaCore/Type/TypeVariableAttributes.h
    - Dia/DiaCore/Type/TypeVariableData.h
  namespaces: []
  entry_points:
    - IntegralConstant
    - MetaData
    - null_t
    - StringReader
    - StringWriter
    - TypeConstant
    - TypeDefinition
    - TypeFacade
    - TypeInstance
    - TypeJsonSerializer
    - TypeJsonSerializerExternalDeserializeInterface
    - TypeJsonSerializerExternalSerializeInterface

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.bitflag
    - dia.core.containers.hashtables
    - dia.core.containers.linklist
    - dia.core.containers.strings
    - dia.core.core
    - dia.core.crc
    - dia.core.json.external.json
    - dia.core.memory
    - dia.core.strings
  forbidden: []
---
