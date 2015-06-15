#pragma once

namespace Dia
{
	namespace Core
	{
		template <class T>
		class Functor
		{
		public:
			virtual void Call(T& object)=0;  
			virtual void Call(const T& object)const=0; 
		};

		template <class T>
		class ComparisonFunctor
		{
		public:
			virtual bool Equal(const T& object1, const T& object2)const = 0;
		};

		template <class T>
		class EqualityFunctor
		{
		public:
			virtual bool GreaterThen(const T& object1, const T& object2)const = 0;
			virtual bool LessThen(const T& object1, const T& object2)const = 0;
			virtual bool Equal(const T& object1, const T& object2)const = 0;
		};

		template <class T, class X>
		class EvaluateFunctor
		{
		public:
			virtual X Evaluate(const T& object1)const=0;	
		};
	}
} 

