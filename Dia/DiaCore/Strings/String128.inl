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
			String128::String128()
				: String<128>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String128::String128 (ConstIterator& iter)
				: String<128>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String128::String128 ( ConstReverseIterator& iter )
				: String<128>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String128::String128 (const String<_size>& rhs)
				: String<128>( rhs )
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String128::String128 ( ConstIterator& iter, const Evaluator& filter )
				: String<128>( iter, filter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String128::String128 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements )
				: String<128>(rhs, startIndex, numberElements)
			{}

			//-----------------------------------------------------------------------------
			inline
			String128::String128 (const char* pRawString, ...)
				: String<128>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			inline
			String128::String128(const char* pRawString, va_list argList)
				: String<128>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}