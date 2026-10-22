#pragma once

// RampRegistry: colour ramps keyed by id (StringCRC of the ramp name).
//
// Rules (Feature 5):
//  - Dia ramps (isDia = true) are registered first by RampSetLoader. A game ramp whose id equals a Dia ramp is
//    rejected (ClashesWithDia, Error naming both files): Dia wins regardless of load order.
//  - Between game files the first registration wins (files load in sorted-path order); the later one is
//    rejected (DuplicateId, Error naming both files).
//  - Immutable after Publish(): Register afterwards returns AlreadyPublished (Error). Reads take no lock.
//  - Returned RampData pointers stay valid for the registry's life.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/RampData.h>

namespace Dia::ValueSchema
{
	enum class RampRegisterResult { Ok, DuplicateId, ClashesWithDia, AlreadyPublished };

	class RampRegistry
	{
	public:
		RampRegistry();
		~RampRegistry();
		RampRegistry(const RampRegistry&) = delete;
		RampRegistry& operator=(const RampRegistry&) = delete;

		// sourceFile is used for diagnostics only (copied).
		RampRegisterResult	Register(const RampData& ramp, bool isDia, const char* sourceFile);

		void				Publish();
		bool				IsPublished() const;

		const RampData*		Find(Dia::Core::StringCRC rampId) const;	// null when unknown
		unsigned int		Count() const;

	private:
		struct Impl;
		Impl* mImpl;
	};
}
