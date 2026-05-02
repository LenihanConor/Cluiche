#pragma once

#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia
{
	namespace StateMachine
	{
		namespace Testing
		{
			enum class CallbackType
			{
				kEnter,
				kExit,
				kUpdate,
				kPause,
				kResume
			};

			struct CallbackRecord
			{
				CallbackType type;
				Dia::Core::StringCRC stateId;
			};

			class TransitionRecorder
			{
			public:
				static const unsigned int kMaxRecords = 128;

				void RecordEnter(Dia::Core::StringCRC stateId);
				void RecordExit(Dia::Core::StringCRC stateId);
				void RecordUpdate(Dia::Core::StringCRC stateId);
				void RecordPause(Dia::Core::StringCRC stateId);
				void RecordResume(Dia::Core::StringCRC stateId);

				const Dia::Core::Containers::DynamicArrayC<CallbackRecord, kMaxRecords>&
					GetSequence() const;

				void Clear();
				unsigned int Count() const;

			private:
				Dia::Core::Containers::DynamicArrayC<CallbackRecord, kMaxRecords> mRecords;
			};

			inline void TransitionRecorder::RecordEnter(Dia::Core::StringCRC stateId)
			{
				CallbackRecord r;
				r.type = CallbackType::kEnter;
				r.stateId = stateId;
				mRecords.Add(r);
			}

			inline void TransitionRecorder::RecordExit(Dia::Core::StringCRC stateId)
			{
				CallbackRecord r;
				r.type = CallbackType::kExit;
				r.stateId = stateId;
				mRecords.Add(r);
			}

			inline void TransitionRecorder::RecordUpdate(Dia::Core::StringCRC stateId)
			{
				CallbackRecord r;
				r.type = CallbackType::kUpdate;
				r.stateId = stateId;
				mRecords.Add(r);
			}

			inline void TransitionRecorder::RecordPause(Dia::Core::StringCRC stateId)
			{
				CallbackRecord r;
				r.type = CallbackType::kPause;
				r.stateId = stateId;
				mRecords.Add(r);
			}

			inline void TransitionRecorder::RecordResume(Dia::Core::StringCRC stateId)
			{
				CallbackRecord r;
				r.type = CallbackType::kResume;
				r.stateId = stateId;
				mRecords.Add(r);
			}

			inline const Dia::Core::Containers::DynamicArrayC<CallbackRecord,
				TransitionRecorder::kMaxRecords>&
				TransitionRecorder::GetSequence() const
			{
				return mRecords;
			}

			inline void TransitionRecorder::Clear()
			{
				mRecords.RemoveAll();
			}

			inline unsigned int TransitionRecorder::Count() const
			{
				return mRecords.Size();
			}
		}
	}
}
