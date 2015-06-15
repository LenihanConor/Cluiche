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
			String32::String32()
				: String<32>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String32::String32 (ConstIterator& iter)
				: String<32>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String32::String32 ( ConstReverseIterator& iter )
				: String<32>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String32::String32 (const String<_size>& rhs)
				: String<32>( rhs )
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String32::String32 ( ConstIterator& iter, const Evaluator& filter )
				: String<32>( iter, filter)
			{}

			//-----------------------------------------------------------------------------
			template<unsigned int _size> inline
			String32::String32 (const String<_size>& rhs, unsigned int startIndex, unsigned int numberElements )
				: String<32>(rhs, startIndex, numberElements)
			{}

			//-----------------------------------------------------------------------------
			inline
			String32::String32 (const char* pRawString, ...)
				: String<32>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
			//-----------------------------------------------------------------------------
			inline
			String32::String32(const char* pRawString, va_list argList)
				: String<32>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}