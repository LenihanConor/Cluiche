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
			String512::String512 (const char* pRawString, ...)
				: String<512>()
			{
				DIA_ASSERT(pRawString != NULL, "Invalid Data");

				va_list args;
				va_start(args, pRawString);
				Format(pRawString, args);
				va_end(args);
			}
			
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