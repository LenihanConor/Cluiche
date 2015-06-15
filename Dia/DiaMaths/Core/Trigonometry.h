#pragma once

//---------------------------------------------------------------------------------------------------------------------------------
// Trigonometry functions
//---------------------------------------------------------------------------------------------------------------------------------
		
namespace Dia
{
	namespace Maths
	{
		class Angle;

		float DegToRadians(float angle);	//convert degrees to radians
		float RadiansToDeg(float angle);	//converts radians to degree

		float Sin(float radians);
		float Cos(float radians);
		float Tan(float radians);
		float ATan(float radians);
		float ACos(float radians);
		float ASin(float radians);

		float Sin(const Angle& angle);
		float Cos(const Angle& angle);
		float Tan(const Angle& angle);
		float ATan(const Angle& angle);
		float ACos(const Angle& angle);
		float ASin(const Angle& angle);
	}
};

#include "DiaMaths/Core/Trigonometry.inl"
