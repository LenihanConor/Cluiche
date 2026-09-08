#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Editor
	{
		class PluginServiceLocator
		{
		public:
			template<typename T>
			void RegisterService(T* service)
			{
				DIA_ASSERT(!HasService<T>(), "Service already registered");
				ServiceEntry entry;
				entry.typeId = T::kUniqueId;
				entry.service = service;
				mServices.Add(entry);
			}

			template<typename T>
			void UnregisterService()
			{
				for (unsigned int i = 0; i < mServices.Size(); ++i)
				{
					if (mServices[i].typeId == T::kUniqueId)
					{
						mServices.RemoveAt(i);
						return;
					}
				}
			}

			template<typename T>
			T* GetService() const
			{
				for (unsigned int i = 0; i < mServices.Size(); ++i)
				{
					if (mServices[i].typeId == T::kUniqueId)
					{
						return static_cast<T*>(mServices[i].service);
					}
				}
				return nullptr;
			}

			template<typename T>
			bool HasService() const
			{
				for (unsigned int i = 0; i < mServices.Size(); ++i)
				{
					if (mServices[i].typeId == T::kUniqueId)
						return true;
				}
				return false;
			}

		private:
			struct ServiceEntry
			{
				Dia::Core::StringCRC typeId;
				void* service;
			};

			static const unsigned int kMaxServices = 16;
			Dia::Core::Containers::DynamicArrayC<ServiceEntry, kMaxServices> mServices;
		};
	}
}
