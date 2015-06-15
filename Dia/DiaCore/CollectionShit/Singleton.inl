namespace Dia
{
	namespace Collections
	{
		//------------------------------------------------
		template <class T> 
		Singleton<T>::Singleton(void)
		{}

		//------------------------------------------------
		template <class T> 
		void Singleton<T>::Create ()
		{
			DIA_ASSERT(msInstance == NULL, "Singleton Already created");
			msInstance = new T();//DIA_NEW();
		}

		//------------------------------------------------
		template <class T> 
		void Singleton<T>::Destroy ()
		{
			DIA_ASSERT(msInstance, "Singleton not created");
			delete msInstance; //AP_DELETE();
			msInstance = NULL;
		}

		//------------------------------------------------
		template <class T> 
		const T& Singleton<T>::GetInstanceConst()
		{
			DIA_ASSERT(msInstance, "Singleton not created");
			return *msInstance;
		}

		//------------------------------------------------
		template <class T> 
		T& Singleton<T>::GetInstance()
		{
			DIA_ASSERT(msInstance, "Singleton not created");
			return *msInstance;
		}

		//------------------------------------------------
		template <class T> 
		const T* Singleton<T>::GetInstancePtrConst()
		{
			DIA_ASSERT(msInstance, "Singleton not created");
			return msInstance;
		}
		
		//------------------------------------------------
		template <class T> 
		T* Singleton<T>::GetInstancePtr()
		{
			DIA_ASSERT(msInstance, "Singleton not created");
			return msInstance;
		}

		//--------------------------------------------------------------------------
		template <class T>
		bool Singleton <T>::IsCreated()
		{
			return (msInstance != NULL);
		}

		//--------------------------------------------------------------------------
		template <class T>
		T* Singleton <T>::msInstance = NULL;
	}
}