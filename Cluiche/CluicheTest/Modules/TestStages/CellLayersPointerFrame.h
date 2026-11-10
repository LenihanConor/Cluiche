#pragma once
#include <DiaCellLayers/SelectionInput.h>
#include <DiaInput/EKey.h>

namespace CluicheTest {

// Mouse button indices as produced by the SDL input source (Dia/DiaSDL/InputSource.cpp) and read by InputStreamModule.
constexpr int kPointerButtonLeft   = 0;
constexpr int kPointerButtonRight  = 1;
constexpr int kPointerButtonMiddle = 2;

// Builds the viewer's PointerFrame from an input source exposing the InputStreamModule accessors
// (GetMouseX/Y, IsMouseButtonDown(int), IsKeyDown(EKey), WasKeyPressed(EKey)). Templated so it is testable without the app.
template <typename TInput>
Dia::CellLayers::SelectionInput::PointerFrame BuildPointerFrame(const TInput& input)
{
	using Dia::Input::EKey;
	Dia::CellLayers::SelectionInput::PointerFrame f;
	f.x = static_cast<float>(input.GetMouseX());
	f.y = static_cast<float>(input.GetMouseY());
	f.left   = input.IsMouseButtonDown(kPointerButtonLeft);
	f.middle = input.IsMouseButtonDown(kPointerButtonMiddle);
	f.right  = input.IsMouseButtonDown(kPointerButtonRight);
	f.shift  = input.IsKeyDown(EKey::LShift)   || input.IsKeyDown(EKey::RShift);
	f.ctrl   = input.IsKeyDown(EKey::LControl) || input.IsKeyDown(EKey::RControl);
	f.alt    = input.IsKeyDown(EKey::LAlt)     || input.IsKeyDown(EKey::RAlt);
	f.escPressed = input.WasKeyPressed(EKey::Escape);
	f.hasKeyboard = true;
	f.valid = true;
	return f;
}

} // namespace CluicheTest
