////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion
////////////////////////////////////////////////////////////////////////////////
#include "DiaUIAwesomium/Conversion.h"

#include <DiaUI/BoundMethod.h>
#include <Awesomium/JSArray.h>
#include <Awesomium/JSValue.h>

namespace Dia
{
	namespace UI
	{
		namespace Awesomium
		{
			void Convert(BoundMethodArgs& lhs, const ::Awesomium::JSArray& rhs)
			{
				for (unsigned int i = 0; i < rhs.size(); i++)
				{
					const ::Awesomium::JSValue& value = rhs[i];
					BoundMethodValue diaValue;
					Convert(diaValue, value);
					lhs.Add(diaValue);
				}
			}

			void Convert(BoundMethodValue& lhs, const ::Awesomium::JSValue& rhs)
			{
				if (rhs.IsBoolean())
				{
					lhs = BoundMethodValue(rhs.ToBoolean());
				}
				else if (rhs.IsInteger())
				{
					lhs = BoundMethodValue(rhs.ToInteger());
				}
				else if (rhs.IsDouble())
				{
					lhs = BoundMethodValue(rhs.IsDouble());
				}
				else if (rhs.IsString())
				{
					Dia::Core::Containers::String64 str(reinterpret_cast<const char*>(rhs.ToString().data()));

					lhs = BoundMethodValue(str);
				}
				else
				{
					DIA_ASSERT(0, "Cannot convert JSValue to Dia BoundMethodArgs")
				}
			}
		}
	}
}