#ifndef DIA_LOG_H
#define DIA_LOG_H


namespace Dia
{
	namespace Core
	{
		class Log
		{
		public:
			static void Output(const char* str);
			static void OutputLine(const char* str);
			static void OutputVaradicLine(const char* str, ...);
		};
	}
}

#endif // DIA_ASSERT