#include "DiaSDL/DisplayInfo.h"
#include <SDL3/SDL.h>

namespace Dia { namespace SDL {

void GetPrimaryDisplaySize(unsigned int& outWidth, unsigned int& outHeight)
{
    outWidth  = 1920;
    outHeight = 1080;

    // SDL3 display queries require the video subsystem to be initialized.
    // Safe to call if already initialized — SDL_Init is reference-counted.
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_Init(SDL_INIT_VIDEO);

    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0) return;

    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(display, &bounds))
    {
        outWidth  = static_cast<unsigned int>(bounds.w);
        outHeight = static_cast<unsigned int>(bounds.h);
    }
}

}} // namespace Dia::SDL
