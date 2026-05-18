#include "ApplicationManifestLoader.h"
#include "JsonApplicationManifestSerializer.h"
#include "ManifestComposer.h"

#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>

#include <DiaLogger/DiaLog.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String512.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstring>

namespace Dia
{
	namespace Application
	{
		//-----------------------------------------------------------------------------
		// ApplicationManifestLoader
		//-----------------------------------------------------------------------------

		ApplicationManifestLoader::ApplicationManifestLoader(ApplicationTypeRegistry& registry)
			: mRegistry(registry)
			, mErrors()
			, mValidator(registry)
		{
		}

		ApplicationManifestLoader::~ApplicationManifestLoader()
		{
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromFile(const char* filePath, ApplicationManifest& outManifest)
		{
			ClearErrors();
			ManifestComposer composer;
			ManifestValidationResult result = composer.ComposeSingleManifest(filePath, outManifest);
			if (result != ManifestValidationResult::kSuccess)
			{
				const auto& composerErrors = composer.GetErrors();
				for (unsigned int i = 0; i < composerErrors.Size(); ++i)
					mErrors.Add(composerErrors[i]);
				return result;
			}
			return Validate(outManifest);
		}

		ManifestValidationResult ApplicationManifestLoader::LoadFromString(const char* jsonString, ApplicationManifest& outManifest)
		{
			ClearErrors();
			JsonApplicationManifestSerializer serializer;
			auto result = serializer.Load(jsonString, outManifest);
			if (!result)
			{
				const char* err = result.error ? result.error : "parse error";
				ManifestValidationResult code = (strcmp(err, "json parse error") == 0)
					? ManifestValidationResult::kInvalidJSON
					: ManifestValidationResult::kMissingRequiredField;
				AddError(code, err, "json");
				return code;
			}
			return Validate(outManifest);
		}

		ManifestValidationResult ApplicationManifestLoader::Validate(const ApplicationManifest& manifest)
		{
			ClearErrors();

			// Delegate to ManifestValidator
			ManifestValidationResult result = mValidator.Validate(manifest);

			// Copy errors from validator
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& validatorErrors = mValidator.GetErrors();
			for (unsigned int i = 0; i < validatorErrors.Size(); ++i)
			{
				mErrors.Add(validatorErrors[i]);
			}

			return result;
		}

		const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& ApplicationManifestLoader::GetErrors() const
		{
			return mErrors;
		}

		void ApplicationManifestLoader::ClearErrors()
		{
			mErrors.RemoveAll();
			mValidator.ClearErrors();
		}


		void ApplicationManifestLoader::AddError(ManifestValidationResult code, const char* message, const char* context)
		{
			mErrors.Add(ManifestValidationError(code, message, context));
		}
	}
}
