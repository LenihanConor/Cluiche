
namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			inline
			TypeTextSerializer&	TypeFacade::TextSerializer()
			{
				return mTextSerializer;
			}

			inline
			const TypeTextSerializer& TypeFacade::TextSerializer()const
			{
				return mTextSerializer;
			}

			inline
			TypeJsonSerializer&	TypeFacade::JsonSerializer()
			{
				return mJsonSerializer;
			}

			inline
			const TypeJsonSerializer& TypeFacade::JsonSerializer()const
			{
				return mJsonSerializer;
			}
		}
	}
}