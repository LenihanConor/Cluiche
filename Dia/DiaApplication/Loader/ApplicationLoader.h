#pragma once

#include <DiaApplication/Manifest/ManifestValidator.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class ApplicationTypeRegistry;

		// ApplicationLoader
		// High-level API for loading applications from manifests
		//
		// AC3: Provides simple interface for loading entire applications from .diaapp files
		// AC9: Supports fallback to code-defined structure if manifest loading fails
		//
		// Usage:
		//   // Simple load with default error handling
		//   ProcessingUnit* app = ApplicationLoader::LoadApplication("app.diaapp");
		//
		//   // Load with fallback to code-defined structure
		//   ProcessingUnit* app = ApplicationLoader::LoadApplicationWithFallback(
		//       "app.diaapp",
		//       []() { return new MyDefaultProcessingUnit(); }
		//   );
		//
		//   // Load with custom error handling
		//   ManifestValidationResult result;
		//   ProcessingUnit* app = ApplicationLoader::LoadApplication("app.diaapp", result);
		//   if (!app) {
		//       // Handle error based on result
		//   }
		class ApplicationLoader
		{
		public:
			// AC3: Load entire application from manifest
			static ProcessingUnit* LoadApplication(ApplicationTypeRegistry& registry,
												   const char* manifestPath,
												   ManifestValidationResult& outResult);

			// AC3: Convenience overload with default error handling
			static ProcessingUnit* LoadApplication(ApplicationTypeRegistry& registry,
												   const char* manifestPath);

			// AC9: Fallback to code-defined structure if manifest fails
			static ProcessingUnit* LoadApplicationWithFallback(
				ApplicationTypeRegistry& registry,
				const char* manifestPath,
				ProcessingUnit* (*fallbackFactory)());

			// Builds full PU tree from merged manifest.
			// Returns the root PU which owns all children.
			// Exactly one PU entry must have root == true.
			static ProcessingUnit* LoadApplicationTree(ApplicationTypeRegistry& registry,
													   const char* manifestPath,
													   ManifestValidationResult& outResult);


		private:
			ApplicationLoader() = delete;
		};
	}
}
