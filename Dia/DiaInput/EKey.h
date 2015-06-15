////////////////////////////////////////////////////////////////////////////////
// Filename: Event.h: Single input event
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Core/EnumClass.h>

namespace Dia
{
	namespace Input
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: EKey
		////////////////////////////////////////////////////////////////////////////////
		CLASSEDENUM(EKey, \
			CE_ITEMVAL(Unknown, 1)\
			CE_ITEMVAL(A, 0)\
			CE_ITEM(B)\
			CE_ITEM(C)\
			CE_ITEM(D)\
			CE_ITEM(E)\
			CE_ITEM(F)\
			CE_ITEM(G)\
			CE_ITEM(H)\
			CE_ITEM(I)\
			CE_ITEM(J)\
			CE_ITEM(K)\
			CE_ITEM(L)\
			CE_ITEM(M)\
			CE_ITEM(N)\
			CE_ITEM(O)\
			CE_ITEM(P)\
			CE_ITEM(Q)\
			CE_ITEM(R)\
			CE_ITEM(S)\
			CE_ITEM(T)\
			CE_ITEM(U)\
			CE_ITEM(V)\
			CE_ITEM(W)\
			CE_ITEM(X)\
			CE_ITEM(Y)\
			CE_ITEM(Z)\
			CE_ITEM(Num0)\
			CE_ITEM(Num1)\
			CE_ITEM(Num2)\
			CE_ITEM(Num3)\
			CE_ITEM(Num4)\
			CE_ITEM(Num5)\
			CE_ITEM(Num6)\
			CE_ITEM(Num7)\
			CE_ITEM(Num8)\
			CE_ITEM(Num9)\
			CE_ITEM(Escape)\
			CE_ITEM(LControl)\
			CE_ITEM(LShift)\
			CE_ITEM(LAlt)\
			CE_ITEM(LSystem)\
			CE_ITEM(RControl)\
			CE_ITEM(RShift)\
			CE_ITEM(RAlt)\
			CE_ITEM(RSystem)\
			CE_ITEM(Menu)\
			CE_ITEM(LBracket)\
			CE_ITEM(RBracket)\
			CE_ITEM(SemiColon)\
			CE_ITEM(Comma)\
			CE_ITEM(Period)\
			CE_ITEM(Quote)\
			CE_ITEM(Slash)\
			CE_ITEM(BackSlash)\
			CE_ITEM(Tilde)\
			CE_ITEM(Equal)\
			CE_ITEM(Dash)\
			CE_ITEM(Space)\
			CE_ITEM(Return)\
			CE_ITEM(BackSpace)\
			CE_ITEM(Tab)\
			CE_ITEM(PageUp)\
			CE_ITEM(PageDown)\
			CE_ITEM(End)\
			CE_ITEM(Home)\
			CE_ITEM(Insert)\
			CE_ITEM(Delete)\
			CE_ITEM(Add)\
			CE_ITEM(Subtract)\
			CE_ITEM(Multiply)\
			CE_ITEM(Divide)\
			CE_ITEM(Left)\
			CE_ITEM(Right)\
			CE_ITEM(Up)\
			CE_ITEM(Down)\
			CE_ITEM(Numpad0)\
			CE_ITEM(Numpad1)\
			CE_ITEM(Numpad2)\
			CE_ITEM(Numpad3)\
			CE_ITEM(Numpad4)\
			CE_ITEM(Numpad5)\
			CE_ITEM(Numpad6)\
			CE_ITEM(Numpad7)\
			CE_ITEM(Numpad8)\
			CE_ITEM(Numpad9)\
			CE_ITEM(F1)\
			CE_ITEM(F2)\
			CE_ITEM(F3)\
			CE_ITEM(F4)\
			CE_ITEM(F5)\
			CE_ITEM(F6)\
			CE_ITEM(F7)\
			CE_ITEM(F8)\
			CE_ITEM(F9)\
			CE_ITEM(F10)\
			CE_ITEM(F11)\
			CE_ITEM(F12)\
			CE_ITEM(F13)\
			CE_ITEM(F14)\
			CE_ITEM(F15)\
			CE_ITEM(Pause)\
			, Unknown \
			);
	}
}