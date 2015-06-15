
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
		}
	}
}