////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Awesomium { class JSArray; }
namespace Awesomium { class JSValue; }
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

			void Convert(::Awesomium::JSArray& lhs, const BoundMethodArgs& rhs);
			void Convert(::Awesomium::JSValue& lhs, const BoundMethodValue& rhs);
		}
	}
}