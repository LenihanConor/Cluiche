#ifndef SINGLETON
#define SINGLETON

#include "DiaCore/Core/Assert.h"
#include "DiaCore/Memory/Memory.h"
namespace Dia
{
	namespace Core
	{
		template <class T>
		class Singleton
		{
		public:
			static void			Create ();
			static void			Destroy ();		

			static const T&		GetInstanceConst();
			static T&			GetInstance();

			static const T*		GetInstancePtrConst();
			static T*			GetInstancePtr();
			
			static bool			IsCreated();

		protected:
			Singleton(void);

		private:
			static T* msInstance;
		};
	}
}

#include "DiaCore/Architecture/Singleton/Singleton.inl"

#endif