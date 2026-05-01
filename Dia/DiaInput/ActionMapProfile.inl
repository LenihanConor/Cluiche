////////////////////////////////////////////////////////////////////////////////
// Filename: ActionMapProfile.inl - ActionMap profile save/load implementations
////////////////////////////////////////////////////////////////////////////////
#pragma once

// This file provides implementations for ActionMap::SaveProfile and LoadProfile
// Include this file AFTER both ActionMap.h and InputProfile.h to resolve
// the circular dependency between these headers.
//
// Usage:
//   #include "DiaInput/ActionMap.h"
//   #include "DiaInput/InputProfile.h"
//   #include "DiaInput/ActionMapProfile.inl"

#include "DiaInput/ActionMap.h"
#include "DiaInput/InputProfile.h"

namespace Dia
{
	namespace Input
	{
		inline bool ActionMap::SaveProfile(const char* filePath, const char* profileName) const
		{
			return InputProfile::SaveProfile(*this, filePath, profileName);
		}

		inline bool ActionMap::LoadProfile(const char* filePath)
		{
			return InputProfile::LoadProfile(*this, filePath);
		}
	}
}
