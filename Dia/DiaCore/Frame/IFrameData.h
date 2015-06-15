#ifndef DIA_CRC_H
#define DIA_CRC_H

namespace Dia
{
	namespace Core
	{
		class CRC
		{
		public:

			static const unsigned int MaxCRC();
			static const unsigned int InvalidCRC();

			CRC ();
			CRC (unsigned int val);
			CRC (const CRC& crc);
			explicit CRC (const char* string); 
			explicit CRC (const void* data, const size_t size);

			CRC&				operator = (const CRC& crc);
			CRC&				operator = (const unsigned int val);
			
			bool				operator == (const CRC& crc)const;
			bool				operator != (const CRC& crc)const;
			
			const unsigned int	Value () const;							/// accessor		
			
			const float AsFractionOfMaxCRC() const;

			operator const	unsigned int() const;
			
		protected:
			/// protected member function that calculates the crc
			static const unsigned int	Calc (const char* data, const size_t size);

			/// member variables
			static const unsigned int	msTable [256];
			unsigned int				mCRC;

		}; // CRC

	}
}

#endif // DIA_ASSERT