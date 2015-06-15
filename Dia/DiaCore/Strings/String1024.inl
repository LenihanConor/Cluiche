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
			String1024::String1024()
				: String<1024>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String1024::String1024 (ConstIterator& iter)
				: String<1024>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String1024::String1024 ( ConstReverseIterator& iter )
				: String<1024>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String1024::String1024 (const String<_size>& rhs)
				: String<1024>( rhs )
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String1024::String1024 ( ConstIterator& iter, const Evaluator& filter )
				: String<1024>( iter, filter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String1024::String1024 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements )
				: String<1024>(rhs, startIndex, numberElements)
			{}

			//-----------------------------------------------------------------------------
			inline
			String1024::String1024 (const char* pRawString, ...)
				: String<1024>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			inline
			String1024::String1024(const char* pRawString, va_list argList)
				: String<1024>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}