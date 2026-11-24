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
			String512::String512()
				: String<512>()
			{}

			//-----------------------------------------------------------------------------
			inline
			String512::String512 (ConstIterator& iter)
				: String<512>(iter)
			{}
			
			//-----------------------------------------------------------------------------
			inline
			String512::String512 ( ConstReverseIterator& iter )
				: String<512>(iter)
			{}

			//-----------------------------------------------------------------------------
			template<class Evaluator> inline
			String512::String512 ( ConstIterator& iter, const Evaluator& filter )
				: String<512>( iter, filter)
			{}


			
			//-----------------------------------------------------------------------------
			inline
			String512::String512(const char* pRawString, va_list argList)
				: String<512>()
			{
				Format(pRawString, argList);			
			}
		}
	}
}