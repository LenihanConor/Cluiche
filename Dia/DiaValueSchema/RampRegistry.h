#pragma once

// RampRegistry: colour ramps keyed by id (StringCRC of the ramp name).
//
// Rules (Feature 5):
//  - Dia ramps (isDia = true) are registered first by RampSetLoader. A game ramp whose id equals a Dia ramp is
//    rejected (ClashesWithDia, Error naming both files): Dia wins regardless of load order. A Dia ramp registered
//    AFTER a game ramp with the same name replaces the game ramp's data in place (Ok, Warning naming both files;
//    pointers returned earlier by Find stay valid and now show the Dia ramp).
//  - Two different ramp names whose ids (CRC) are equal are a HashCollision (Error): the first registration stays.
//    DuplicateId / ClashesWithDia are only reported when the names are identical.
//  - Between game files the first registration wins (files load in sorted-path order); the later one is
//    rejected (DuplicateId, Error naming both files).
//  - Immutable after Publish(): Register afterwards returns AlreadyPublished (Error). Reads take no lock.
//  - Returned RampData pointers stay valid for the registry's life.

#include <DiaCore/CRC/StringCRC.h>
#include <DiaValueSchema/RampData.h>

namespace Dia::ValueSchema
{
	enum class RampRegisterResult { Ok, DuplicateId, ClashesWithDia, AlreadyPublished, HashCollision };

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
		const RampData*		At(unsigned int index) const;	// registration order; null when out of range

	private:
		struct Impl;
		Impl* mImpl;
	};
}
