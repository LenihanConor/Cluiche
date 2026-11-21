// S-1 World Generation map view: builds the DiaCellLayers viewer's PointerFrame from an input source that exposes the
// InputStreamModule accessors. Copy of CluicheTest's BuildPointerFrame (CoW cannot include CluicheTest).
#pragma once

#include <DiaCellLayers/SelectionInput.h>
#include <DiaInput/EKey.h>

namespace CoW {

// Mouse button indices as produced by the SDL input source (Dia/DiaSDL/InputSource.cpp) and read by InputStreamModule.
constexpr int kWorldGenButtonLeft   = 0;
constexpr int kWorldGenButtonRight  = 1;
constexpr int kWorldGenButtonMiddle = 2;

// Templated so it is testable without the application.
template <typename TInput>
Dia::CellLayers::SelectionInput::PointerFrame BuildWorldGenPointerFrame(const TInput& input)
{
    using Dia::Input::EKey;
    Dia::CellLayers::SelectionInput::PointerFrame f;
    f.x = static_cast<float>(input.GetMouseX());
    f.y = static_cast<float>(input.GetMouseY());
    f.left   = input.IsMouseButtonDown(kWorldGenButtonLeft);
    f.middle = input.IsMouseButtonDown(kWorldGenButtonMiddle);
    f.right  = input.IsMouseButtonDown(kWorldGenButtonRight);
    f.shift  = input.IsKeyDown(EKey::LShift)   || input.IsKeyDown(EKey::RShift);
    f.ctrl   = input.IsKeyDown(EKey::LControl) || input.IsKeyDown(EKey::RControl);
    f.alt    = input.IsKeyDown(EKey::LAlt)     || input.IsKeyDown(EKey::RAlt);
    f.escPressed = input.WasKeyPressed(EKey::Escape);
    f.hasKeyboard = true;
    f.valid = true;
    return f;
}

} // namespace CoW
