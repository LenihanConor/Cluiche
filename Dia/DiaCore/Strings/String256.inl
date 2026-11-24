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
			String256::String256()
				: String<256>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String256::String256 (ConstIterator& iter)
				: String<256>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String256::String256 ( ConstReverseIterator& iter )
				: String<256>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String256::String256 ( ConstIterator& iter, const Evaluator& filter )
				: String<256>( iter, filter)
			{}

			
			//-----------------------------------------------------------------------------
			inline
			String256::String256(const char* pRawString, va_list argList)
				: String<256>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}