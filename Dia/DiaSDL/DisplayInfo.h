#pragma once

namespace Dia { namespace SDL {

// Returns the pixel dimensions of the primary display.
// Falls back to (1920, 1080) if SDL is not yet initialized or the query fails.
void GetPrimaryDisplaySize(unsigned int& outWidth, unsigned int& outHeight);

}} // namespace Dia::SDL
