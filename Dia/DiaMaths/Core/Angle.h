namespace Dia
{
    namespace Maths
    {
        ////////////////////////////////////// class Angle //////////////////////////////////////

        // The Angle class represents an angle measurement in radians.
        class Angle
        {
        public:
            // Default constructor
            Angle();

            // Copy constructor
            Angle(const Angle& angle);

            // Constructor that takes radians as input
            explicit Angle(const float radian);

            // Assignment operator
            Angle& operator=(const Angle& angle);

            // Unary negation operator
            Angle operator-() const;

            // Addition operator
            Angle operator+(const Angle& angle) const;

            // Subtraction operator
            Angle operator-(const Angle& angle) const;

            // Multiplication operator with a scaler
            Angle operator*(const float scaler) const;

            // Division operator with a scaler
            Angle operator/(const float scaler) const;

            // Addition-assignment operator
            inline Angle& operator+=(const Angle& angle);

            // Subtraction-assignment operator
            inline Angle& operator-=(const Angle& angle);

            // Multiplication-assignment operator with a scaler
            inline Angle& operator*=(const float scaler);

            // Division-assignment operator with a scaler
            inline Angle& operator/=(const float scaler);

            // Equality comparison operator
            bool operator==(const Angle& angle) const;

            // Inequality comparison operator
            bool operator!=(const Angle& angle) const;

            // Less than or equal to comparison operator
            bool operator<=(const Angle& angle) const;

            // Greater than or equal to comparison operator
            bool operator>=(const Angle& angle) const;

            // Less than comparison operator
            bool operator<(const Angle& angle) const;

            // Greater than comparison operator
            bool operator>(const Angle& angle) const;

            // Returns the angle value in radians
            float AsRadians() const;

            // Returns the angle value in degrees
            float AsDegrees() const;

            // Returns the positive angle value in radians
            float AsPositiveRadians() const;

            // Returns the positive angle value in degrees
            float AsPositiveDegrees() const;

            // Normalizes the angle within the range [0, 2π)
            inline void Normalize();

            // Creates an Angle object from degrees
            static Angle FromDegrees(const float degrees);

            // Creates an Angle object from radians
            static Angle FromRadians(const float radians);

        private:
            // Sets the angle value from degrees
            void SetFromDegrees(const float degrees);

            float mRadian; // The angle value in radians

        public:
            // Pre-defined constant angles
            static const Angle Deg0;
            static const Angle DegHalf;
            static const Angle Deg1;
            static const Angle Deg5;
            static const Angle Deg10;
            static const Angle Deg15;
            static const Angle Deg30;
            static const Angle Deg45;
            static const Angle Deg60;
            static const Angle Deg90;
            static const Angle Deg120;
            static const Angle Deg135;
            static const Angle Deg180;
            static const Angle Deg270;
            static const Angle Deg360;
        };
    }
}
