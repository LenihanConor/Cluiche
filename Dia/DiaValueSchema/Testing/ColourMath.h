#pragma once

// ColourMath: header-only colour test utilities (sRGB -> linear -> XYZ D65 -> CIELAB, CIE76 Delta-E, and
// colour-vision-deficiency simulation). Used by the shipped-ramp property tests; no engine dependencies.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Dia::ValueSchema::Testing
{
	struct Rgb { double r, g, b; };		// components in [0,1]
	struct Lab { double L, a, b; };

	inline Rgb UnpackRgba8(uint32_t rgba8)
	{
		return { double((rgba8 >> 24) & 0xFF) / 255.0, double((rgba8 >> 16) & 0xFF) / 255.0, double((rgba8 >> 8) & 0xFF) / 255.0 };
	}

	inline double SrgbToLinear(double c)
	{
		return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
	}

	// Linear-light RGB (components in [0,1]) -> CIELAB, D65 white.
	inline Lab LinearRgbToLab(const Rgb& lin)
	{
		const double X = 0.4124564 * lin.r + 0.3575761 * lin.g + 0.1804375 * lin.b;
		const double Y = 0.2126729 * lin.r + 0.7151522 * lin.g + 0.0721750 * lin.b;
		const double Z = 0.0193339 * lin.r + 0.1191920 * lin.g + 0.9503041 * lin.b;
		const double xn = 0.95047, yn = 1.0, zn = 1.08883;
		auto f = [](double t) { return t > 216.0 / 24389.0 ? std::cbrt(t) : (24389.0 / 27.0 * t + 16.0) / 116.0; };
		const double fx = f(X / xn), fy = f(Y / yn), fz = f(Z / zn);
		return { 116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz) };
	}

	inline Lab SrgbToLab(const Rgb& srgb)
	{
		return LinearRgbToLab({ SrgbToLinear(srgb.r), SrgbToLinear(srgb.g), SrgbToLinear(srgb.b) });
	}

	inline Lab Rgba8ToLab(uint32_t rgba8) { return SrgbToLab(UnpackRgba8(rgba8)); }

	inline double DeltaE76(const Lab& x, const Lab& y)
	{
		const double dL = x.L - y.L, da = x.a - y.a, db = x.b - y.b;
		return std::sqrt(dL * dL + da * da + db * db);
	}

	enum class CvdType { Deuteranopia, Protanopia };

	// Machado, Oliveira, Fernandes 2009, severity 1.0, applied in linear RGB (result clamped to [0,1]).
	inline Rgb SimulateCvdLinear(const Rgb& lin, CvdType type)
	{
		static const double kDeut[3][3] = {
			{  0.367322, 0.860646, -0.227968 },
			{  0.280085, 0.672501,  0.047413 },
			{ -0.011820, 0.042940,  0.968881 } };
		static const double kProt[3][3] = {
			{  0.152286, 1.052583, -0.204868 },
			{  0.114503, 0.786281,  0.099216 },
			{ -0.003882, -0.048116, 1.051998 } };
		const double (*m)[3] = (type == CvdType::Deuteranopia) ? kDeut : kProt;
		auto row = [&](int i) { return std::clamp(m[i][0] * lin.r + m[i][1] * lin.g + m[i][2] * lin.b, 0.0, 1.0); };
		return { row(0), row(1), row(2) };
	}

	// Lab of an 0xRRGGBBAA colour as seen under the given colour-vision deficiency.
	inline Lab Rgba8ToLabCvd(uint32_t rgba8, CvdType type)
	{
		const Rgb s = UnpackRgba8(rgba8);
		return LinearRgbToLab(SimulateCvdLinear({ SrgbToLinear(s.r), SrgbToLinear(s.g), SrgbToLinear(s.b) }, type));
	}
}
