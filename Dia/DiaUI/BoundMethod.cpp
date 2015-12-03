#include "DiaUI/BoundMethod.h"

Dia::UI::BoundMethodValue::BoundMethodValue()
{}

Dia::UI::BoundMethodValue::BoundMethodValue(bool value)
{
	mBoolean = value;
	mType = EType::kBoolean;
}

Dia::UI::BoundMethodValue::BoundMethodValue(int value)
{
	mInt = value;
	mType = EType::kInteger;
}

Dia::UI::BoundMethodValue::BoundMethodValue(double value)
{
	mDouble = value;
	mType = EType::kDouble;
}

Dia::UI::BoundMethodValue::BoundMethodValue(const Dia::Core::Containers::String64 & value)
{
	mString = value; 
	mType = EType::kString;
}

Dia::UI::BoundMethodValue::BoundMethodValue(const BoundMethodValue & original)
{
	mString = original.mString;
	mType = original.mType;
}

Dia::UI::BoundMethodValue & Dia::UI::BoundMethodValue::operator=(const BoundMethodValue & rhs)
{
	mString = rhs.mString;
	mType = rhs.mType;
	return *this;
}

bool Dia::UI::BoundMethodValue::IsBoolean() const
{
	return mType == EType::kBoolean;
}

bool Dia::UI::BoundMethodValue::IsInteger() const
{
	return mType == EType::kInteger;
}

bool Dia::UI::BoundMethodValue::IsDouble() const
{
	return mType == EType::kDouble;
}

bool Dia::UI::BoundMethodValue::IsString() const
{
	return mType == EType::kString;
}

bool Dia::UI::BoundMethodValue::GetBoolean() const
{
	return mBoolean;
}

int Dia::UI::BoundMethodValue::GetInteger() const
{
	return mInt;
}

double Dia::UI::BoundMethodValue::GetDouble() const
{
	return mDouble;
}

const Dia::Core::Containers::String64& Dia::UI::BoundMethodValue::GetString() const
{
	return mString;
}

unsigned int Dia::UI::BoundMethodArgs::Size() const
{
	return mArgs.Size();
}

const Dia::UI::BoundMethodValue& Dia::UI::BoundMethodArgs::At(unsigned int idx) const
{
	return mArgs[idx];
}

void Dia::UI::BoundMethodArgs::Add(const BoundMethodValue & value)
{
	mArgs.Add(value);
}

void Dia::UI::BoundMethodArgs::Clear()
{
	mArgs.RemoveAll();
}
