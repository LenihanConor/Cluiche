#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String64.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// LoadErrorKind
		//---------------------------------------------------------------------------------------------------------
		enum class LoadErrorKind
		{
			FileNotFound,
			JsonParseError,
			TypeNotRegistered,
			MissingRequiredField,
			TypeMismatch,
			DeserializationError
		};

		//---------------------------------------------------------------------------------------------------------
		// LoadError
		//---------------------------------------------------------------------------------------------------------
		struct LoadError
		{
			LoadErrorKind mKind;
			Dia::Core::StringCRC mFieldPath;
			Dia::Core::Containers::String64 mMessage;
		};

		//---------------------------------------------------------------------------------------------------------
		// LoadResult<T>
		//---------------------------------------------------------------------------------------------------------
		template<typename T>
		struct LoadResult
		{
			bool mSuccess;
			T mValue;
			Dia::Core::Containers::DynamicArrayC<LoadError, 16> mErrors;

			LoadResult()
				: mSuccess(false)
				, mValue()
			{}

			bool HasErrors() const
			{
				return mErrors.Size() > 0;
			}

			const LoadError& GetFirstError() const
			{
				DIA_ASSERT(mErrors.Size() > 0, "No errors to retrieve");
				return mErrors[0];
			}
		};

		//---------------------------------------------------------------------------------------------------------
		// LoadResult<void> specialization — for operations that return success/errors but no value.
		//---------------------------------------------------------------------------------------------------------
		template<>
		struct LoadResult<void>
		{
			bool mSuccess;
			Dia::Core::Containers::DynamicArrayC<LoadError, 16> mErrors;

			LoadResult()
				: mSuccess(false)
			{}

			bool HasErrors() const
			{
				return mErrors.Size() > 0;
			}

			const LoadError& GetFirstError() const
			{
				DIA_ASSERT(mErrors.Size() > 0, "No errors to retrieve");
				return mErrors[0];
			}
		};

	} // namespace AssetCatalogue
} // namespace Dia
