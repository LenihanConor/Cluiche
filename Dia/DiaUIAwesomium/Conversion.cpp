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
					DIA_ASSERT(0, "Cannot convert JSValue to Dia BoundMethodArgs");
				}
			}

			void Convert(::Awesomium::JSArray& lhs, const BoundMethodArgs& rhs)
			{
				for (unsigned int i = 0; i < rhs.Size(); i++)
				{
					const BoundMethodValue& diaValue = rhs.At(i);
					::Awesomium::JSValue value;
					Convert(value, diaValue);
					lhs.Push(value);
				}
			}

			void Convert(::Awesomium::JSValue& lhs, const BoundMethodValue& rhs)
			{
				if (rhs.IsBoolean())
				{
					lhs = ::Awesomium::JSValue(rhs.GetBoolean());
				}
				else if (rhs.IsInteger())
				{
					lhs = ::Awesomium::JSValue(rhs.GetInteger());
				}
				else if (rhs.IsDouble())
				{
					lhs = ::Awesomium::JSValue(rhs.GetDouble());
				}
				else if (rhs.IsString())
				{
					lhs = ::Awesomium::JSValue(::Awesomium::WebString::CreateFromUTF8(rhs.GetString().AsCStr(), rhs.GetString().Size()));
				}
				else
				{
					DIA_ASSERT(0, "Cannot convert JSValue to Dia BoundMethodArgs");
				}
			}
		}
	}
}