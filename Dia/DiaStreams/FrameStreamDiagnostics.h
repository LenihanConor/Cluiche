#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace ApplicationFlow {

// Called from FrameStreamStore<T>::FetchLatest() when no data has been written.
// Defined in a .cpp so DIA_LOG_WARN is not expanded inside a template header.
void FrameStream_WarnNoData(const Dia::Core::StringCRC& id);

}} // namespace Dia::ApplicationFlow
