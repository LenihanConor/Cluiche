#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "Bone.h"

namespace Dia
{
	namespace Rig2D
	{
		static const unsigned int kMaxBones = 128;

		struct SkeletonDef
		{
			Dia::Core::StringCRC								id;
			Dia::Core::Containers::DynamicArrayC<Bone, kMaxBones>	bones;
		};

		class Skeleton
		{
		public:
			explicit Skeleton(const SkeletonDef& def);

			int					GetBoneCount() const;
			const Bone&			GetBone(int index) const;
			int					FindBoneIndex(const Dia::Core::StringCRC& name) const;
			int					GetRequiredBoneIndex(const Dia::Core::StringCRC& name) const;
			const Dia::Core::StringCRC& GetId() const;
			bool				IsValid() const;

		private:
			void				Validate(const SkeletonDef& def) const;
			void				ComputeBoneLengths();

			Dia::Core::StringCRC								mId;
			Dia::Core::Containers::DynamicArrayC<Bone, kMaxBones>	mBones;
		};
	}
}
