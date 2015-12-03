////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Awesomium { class JSArray; }
namespace Dia { namespace UI { class BoundMethodArgs;  } }
namespace Dia { namespace UI { class BoundMethodValue; } }

namespace Dia
{
	namespace UI
	{
		namespace Awesomium
		{
			void Convert(BoundMethodArgs& lhs, const ::Awesomium::JSArray& rhs);
			void Convert(BoundMethodValue& lhs, const ::Awesomium::JSValue& rhs);
		}
	}
}