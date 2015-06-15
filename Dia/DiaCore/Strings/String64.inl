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
			String64::String64()
				: String<64>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String64::String64 (ConstIterator& iter)
				: String<64>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String64::String64 ( ConstReverseIterator& iter )
				: String<64>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String64::String64 (const String<_size>& rhs)
				: String<64>( rhs )
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String64::String64 ( ConstIterator& iter, const Evaluator& filter )
				: String<64>( iter, filter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String64::String64 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements )
				: String<64>(rhs, startIndex, numberElements)
			{}

			//-----------------------------------------------------------------------------
			inline
			String64::String64 (const char* pRawString, ...)
				: String<64>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			inline
			String64::String64(const char* pRawString, va_list argList)
				: String<64>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}