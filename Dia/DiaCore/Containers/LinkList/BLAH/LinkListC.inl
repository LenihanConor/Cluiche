namespace Dia
{
	namespace Core
	{
		//------------------------------------------------------------------------------------
		//	LinkListC
		//------------------------------------------------------------------------------------
		template <class Payload, unsigned int size>
		LinkListC<Payload, size>::LinkListC()
			: mCurrentNumberNodes(0)
			, mRootNode(NULL)
		{}

		//------------------------------------------------------------------------------------
		template <class Payload, unsigned int size>
		LinkListC<Payload, size>::LinkListC(const LinkListC<Payload, size>& rhs)
			: mCurrentNumberNodes(0)
			, mRootNode(NULL)
		{
			this->AddNode(rhs.Front())
		}
	}
}