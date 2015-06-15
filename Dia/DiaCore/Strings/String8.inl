namespace Dia
{
	namespace Core
	{
		namespace Containers
		{
			//------------------------------------------------------------------------------------
			//	Implementation
			//------------------------------------------------------------------------------------
					inline
			String8::String8()
				: String<8>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String8::String8 (ConstIterator& iter)
				: String<8>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String8::String8 ( ConstReverseIterator& iter )
				: String<8>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String8::String8 (const String<_size>& rhs)
				: String<8>( rhs )
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String8::String8 ( ConstIterator& iter, const Evaluator& filter )
				: String<8>( iter, filter)
			{}
			
			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String8::String8 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements )
				: String<8>(rhs, startIndex, numberElements)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String8::String8 (const char* pRawString, ...)
				: String<8>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			inline
			String8::String8(const char* pRawString, va_list argList)
				: String<8>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}