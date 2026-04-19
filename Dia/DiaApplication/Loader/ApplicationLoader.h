#pragma once

#include <DiaApplication/Manifest/ManifestValidator.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;  // Forward declaration - defined in ApplicationProcessingUnit.h

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
			// @param manifestPath - Path to .diaapp manifest file
			// @param outResult - Output validation result (errors, warnings)
			// @return Created ProcessingUnit (caller owns it), or nullptr on failure
			static ProcessingUnit* LoadApplication(const char* manifestPath,
												   ManifestValidationResult& outResult);

			// AC3: Convenience overload with default error handling
			// Logs errors automatically and returns nullptr on failure
			// @param manifestPath - Path to .diaapp manifest file
			// @return Created ProcessingUnit (caller owns it), or nullptr on failure
			static ProcessingUnit* LoadApplication(const char* manifestPath);

			// AC9: Fallback to code-defined structure if manifest fails
			// @param manifestPath - Path to .diaapp manifest file
			// @param fallbackFactory - Factory function to create fallback ProcessingUnit
			// @return Created ProcessingUnit (caller owns it), never nullptr unless fallbackFactory returns nullptr
			//
			// This method first attempts to load from manifest. If that fails, it logs a warning
			// and calls fallbackFactory() to create a code-defined ProcessingUnit instead.
			// This allows graceful degradation when manifests are missing, corrupted, or invalid.
			static ProcessingUnit* LoadApplicationWithFallback(
				const char* manifestPath,
				ProcessingUnit* (*fallbackFactory)());

		private:
			ApplicationLoader() = delete;  // Static API only
		};
	}
}
