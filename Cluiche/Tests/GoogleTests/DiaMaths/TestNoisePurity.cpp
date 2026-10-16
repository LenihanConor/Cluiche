#include <gtest/gtest.h>
#include <DiaMaths/Noise/Noise.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <string>
#include <thread>
#include <vector>

using namespace Dia::Maths;

//-----------------------------------------------------------------------------
// AC17: purity source scan
//-----------------------------------------------------------------------------

namespace
{
	std::string ReadFile(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	bool FileExists(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		return in.good();
	}

	// Walks up from this source file's directory (then from the working directory) until
	// Dia/DiaMaths/Noise/Noise.h is found.
	std::string FindNoiseDir()
	{
		const std::string marker = "Dia/DiaMaths/Noise/Noise.h";
		std::vector<std::string> starts;
		starts.push_back(__FILE__);
		starts.push_back("./x");
		for (const std::string& start : starts)
		{
			std::string dir = start;
			std::replace(dir.begin(), dir.end(), '\\', '/');
			for (int depth = 0; depth < 12; ++depth)
			{
				const size_t slash = dir.find_last_of('/');
				if (slash == std::string::npos)
				{
					break;
				}
				dir = dir.substr(0, slash);
				if (FileExists(dir + "/" + marker))
				{
					return dir + "/Dia/DiaMaths/Noise/";
				}
			}
		}
		return std::string();
	}

	// Removes // and /* */ comments and the contents of string/char literals.
	std::string StripComments(const std::string& src)
	{
		std::string out;
		out.reserve(src.size());
		const size_t n = src.size();
		size_t i = 0;
		while (i < n)
		{
			const char c = src[i];
			if (c == '/' && i + 1 < n && src[i + 1] == '/')
			{
				while (i < n && src[i] != '\n') { ++i; }
			}
			else if (c == '/' && i + 1 < n && src[i + 1] == '*')
			{
				i += 2;
				while (i + 1 < n && !(src[i] == '*' && src[i + 1] == '/'))
				{
					if (src[i] == '\n') { out.push_back('\n'); }
					++i;
				}
				i += 2;
			}
			else if (c == '"' || c == '\'')
			{
				// Digit separators (1'000) are not used in the noise sources; treat ' after a digit as a separator.
				if (c == '\'' && i > 0 && std::isxdigit(static_cast<unsigned char>(src[i - 1])))
				{
					out.push_back(c);
					++i;
					continue;
				}
				out.push_back(c);
				++i;
				while (i < n && src[i] != c)
				{
					if (src[i] == '\\') { ++i; }
					++i;
				}
				out.push_back(c);
				++i;
			}
			else
			{
				out.push_back(c);
				++i;
			}
		}
		return out;
	}

	bool IsWordChar(char c)
	{
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
	}

	// Returns descriptions of mutable-static violations.
	std::vector<std::string> FindMutableStatics(const std::string& code)
	{
		std::vector<std::string> bad;
		size_t pos = 0;
		while ((pos = code.find("static", pos)) != std::string::npos)
		{
			const size_t end = pos + 6;
			const bool wordStart = (pos == 0) || !IsWordChar(code[pos - 1]);
			const bool wordEnd = (end >= code.size()) || !IsWordChar(code[end]);
			if (wordStart && wordEnd)
			{
				// Declaration text up to the first of ; = { (
				size_t stop = code.find_first_of(";={(", end);
				if (stop == std::string::npos) { stop = code.size(); }
				const std::string decl = code.substr(end, stop - end);
				const bool isFunction = (stop < code.size() && code[stop] == '(');
				const bool isConst = std::regex_search(decl, std::regex("\\b(const|constexpr|inline)\\b"));
				if (!isFunction && !isConst)
				{
					bad.push_back("static" + decl);
				}
			}
			pos = end;
		}
		return bad;
	}

	std::vector<std::string> NoiseFiles()
	{
		const char* names[] = {
			"FractalNoise.cpp", "FractalNoise.h", "FractalNoise.inl", "GradientNoise.cpp", "GradientNoise.h",
			"Noise.h", "Noise3D.cpp", "Noise3D.h", "NoiseHash.h", "SimplexNoise.cpp", "SimplexNoise.h",
			"ValueNoise.cpp", "ValueNoise.h", "WorleyNoise.cpp", "WorleyNoise.h" };
		return std::vector<std::string>(std::begin(names), std::end(names));
	}
}

TEST(NoisePurity, SourcesFound)
{
	const std::string dir = FindNoiseDir();
	ASSERT_FALSE(dir.empty()) << "could not locate Dia/DiaMaths/Noise from " << __FILE__;
	for (const std::string& f : NoiseFiles())
	{
		EXPECT_TRUE(FileExists(dir + f)) << f;
	}
}

TEST(NoisePurity, StripCommentsHelperWorks)
{
	const std::string code = StripComments("int a; // static int x;\n/* std::sin(1) */ int b; const char* s = \"thread_local\";");
	EXPECT_EQ(code.find("static"), std::string::npos);
	EXPECT_EQ(code.find("std::sin"), std::string::npos);
	EXPECT_EQ(code.find("thread_local"), std::string::npos);
	EXPECT_NE(code.find("int b;"), std::string::npos);
	EXPECT_EQ(FindMutableStatics("static int counter = 0;").size(), 1u);
	EXPECT_EQ(FindMutableStatics("static const int k = 0; static constexpr float f = 1.0f; static float Fn(int a);").size(), 0u);
}

TEST(NoisePurity, NoMutableStatics)
{
	const std::string dir = FindNoiseDir();
	ASSERT_FALSE(dir.empty());
	for (const std::string& f : NoiseFiles())
	{
		const std::string code = StripComments(ReadFile(dir + f));
		ASSERT_FALSE(code.empty()) << f;
		for (const std::string& v : FindMutableStatics(code))
		{
			ADD_FAILURE() << f << ": mutable static state: " << v;
		}
	}
}

TEST(NoisePurity, NoRandomHeaderOrThreadLocal)
{
	const std::string dir = FindNoiseDir();
	ASSERT_FALSE(dir.empty());
	const std::regex randomInclude("#\\s*include\\s*<random>");
	const std::regex threadLocal("\\bthread_local\\b");
	for (const std::string& f : NoiseFiles())
	{
		const std::string code = StripComments(ReadFile(dir + f));
		EXPECT_FALSE(std::regex_search(code, randomInclude)) << f << " includes <random>";
		EXPECT_FALSE(std::regex_search(code, threadLocal)) << f << " uses thread_local";
	}
}

TEST(NoisePurity, NoForbiddenMathCalls)
{
	const std::string dir = FindNoiseDir();
	ASSERT_FALSE(dir.empty());
	// std::-qualified and global C-style (sinf, ::floor, ...) forms.
	const std::regex qualified("\\bstd\\s*::\\s*(sin|cos|pow|fmod|floor|fma|exp|log|tan|abs|fabs)\\b");
	const std::regex cStyle("(^|[^\\w.:>])(::)?(sin|cos|pow|fmod|floor|fma|exp|log|tan|abs|fabs)f?\\s*\\(");
	for (const std::string& f : NoiseFiles())
	{
		const std::string code = StripComments(ReadFile(dir + f));
		std::smatch m;
		EXPECT_FALSE(std::regex_search(code, m, qualified)) << f << ": " << m.str();
		EXPECT_FALSE(std::regex_search(code, m, cStyle)) << f << ": " << m.str();
	}
}

TEST(NoisePurity, ImplementationFilesDisableFpContract)
{
	const std::string dir = FindNoiseDir();
	ASSERT_FALSE(dir.empty());
	const char* impls[] = { "ValueNoise.cpp", "GradientNoise.cpp", "SimplexNoise.cpp", "WorleyNoise.cpp", "Noise3D.cpp", "FractalNoise.cpp" };
	const std::regex pragma("#\\s*pragma\\s+fp_contract\\s*\\(\\s*off\\s*\\)");
	for (const char* f : impls)
	{
		const std::string code = StripComments(ReadFile(dir + f));
		EXPECT_TRUE(std::regex_search(code, pragma)) << f << " is missing #pragma fp_contract(off)";
	}
}

//-----------------------------------------------------------------------------
// Shared sampling of every type
//-----------------------------------------------------------------------------

namespace
{
	uint32_t Bits(float f)
	{
		uint32_t u;
		std::memcpy(&u, &f, sizeof(u));
		return u;
	}

	struct Pt { uint32_t seed; float x; float y; float z; };

	std::vector<Pt> MakePoints(int count)
	{
		std::vector<Pt> pts;
		pts.reserve(count);
		uint32_t s = 12345u;
		for (int i = 0; i < count; ++i)
		{
			s = s * 1664525u + 1013904223u;
			const float x = static_cast<float>(static_cast<int>(s >> 8) % 4001 - 2000) * 0.0173f;
			s = s * 1664525u + 1013904223u;
			const float y = static_cast<float>(static_cast<int>(s >> 8) % 4001 - 2000) * 0.0191f;
			s = s * 1664525u + 1013904223u;
			const float z = static_cast<float>(static_cast<int>(s >> 8) % 401 - 200) * 0.05f;
			pts.push_back(Pt{ static_cast<uint32_t>(i % 7), x, y, z });
		}
		return pts;
	}

	FractalParams Params(int octaves, WorleyOutput w = WorleyOutput::F2MinusF1)
	{
		FractalParams p;
		p.octaves = octaves;
		p.worley = w;
		return p;
	}

	// Evaluates every type at every point, appending result bit patterns.
	std::vector<uint32_t> EvaluateAll(const std::vector<Pt>& pts)
	{
		std::vector<uint32_t> out;
		const FractalParams fp = Params(5);
		RidgedParams rp;
		rp.fractal = Params(4);
		rp.sharpness = 2;
		for (const Pt& p : pts)
		{
			out.push_back(Bits(ValueNoise2D(p.seed, p.x, p.y)));
			out.push_back(Bits(GradientNoise2D(p.seed, p.x, p.y)));
			out.push_back(Bits(SimplexNoise2D(p.seed, p.x, p.y)));
			const WorleyResult w = WorleyNoise2D(p.seed, p.x, p.y, 0.8f);
			out.push_back(Bits(w.f1));
			out.push_back(Bits(w.f2));
			out.push_back(Bits(w.f2MinusF1));
			out.push_back(w.cellId);
			out.push_back(Bits(w.cellValue));
			out.push_back(Bits(Noise3D(p.seed, p.x, p.y, p.z)));
			out.push_back(Bits(FractalNoise3D(p.seed, p.x, p.y, p.z, fp)));
			out.push_back(Bits(FractalNoise2D<NoiseType::Value>(p.seed, p.x, p.y, fp)));
			out.push_back(Bits(FractalNoise2D<NoiseType::Gradient>(p.seed, p.x, p.y, fp)));
			out.push_back(Bits(FractalNoise2D<NoiseType::Simplex>(p.seed, p.x, p.y, fp)));
			out.push_back(Bits(FractalNoise2D<NoiseType::Worley>(p.seed, p.x, p.y, Params(4, WorleyOutput::F1))));
			out.push_back(Bits(FractalNoise2D<NoiseType::Worley>(p.seed, p.x, p.y, Params(4, WorleyOutput::CellValue))));
			out.push_back(Bits(RidgedNoise2D<NoiseType::Value>(p.seed, p.x, p.y, rp)));
			out.push_back(Bits(RidgedNoise2D<NoiseType::Gradient>(p.seed, p.x, p.y, rp)));
			out.push_back(Bits(RidgedNoise2D<NoiseType::Simplex>(p.seed, p.x, p.y, rp)));
			const Warp2DResult wr = Warp2D<NoiseType::Gradient>(p.seed, p.x, p.y, 3.0f, Params(3));
			out.push_back(Bits(wr.x));
			out.push_back(Bits(wr.y));
			out.push_back(Bits(WarpedNoise2D<NoiseType::Simplex, NoiseType::Gradient>(p.seed, p.x, p.y, 3.0f, Params(3), fp)));
		}
		return out;
	}
}

//-----------------------------------------------------------------------------
// AC18: thread safety
//-----------------------------------------------------------------------------

TEST(NoisePurity, ThreadSafetyFourThreadsMatchSingleThread)
{
	const std::vector<Pt> pts = MakePoints(600);
	const std::vector<uint32_t> reference = EvaluateAll(pts);
	ASSERT_FALSE(reference.empty());

	const int kThreads = 4;
	std::vector<std::vector<uint32_t>> results(kThreads);
	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t)
	{
		threads.emplace_back([&pts, &results, t]() { results[t] = EvaluateAll(pts); });
	}
	for (std::thread& th : threads)
	{
		th.join();
	}
	for (int t = 0; t < kThreads; ++t)
	{
		ASSERT_EQ(reference.size(), results[t].size()) << "thread " << t;
		EXPECT_TRUE(reference == results[t]) << "thread " << t << " differs from single-threaded results";
	}
}

//-----------------------------------------------------------------------------
// AC19: performance (66 x 42 grid, 6 octaves)
//-----------------------------------------------------------------------------

namespace
{
	const int kGridW = 66;
	const int kGridH = 42;

#ifdef NDEBUG
	const bool kReleaseLimits = true;
#else
	const bool kReleaseLimits = false;
#endif
	const double kDebugLimitMs = 100.0;

	// Runs the grid sampler a few times and returns the fastest wall time in milliseconds.
	template <typename Fn>
	double TimeGridMs(Fn&& sample, float& sink)
	{
		double best = 1.0e30;
		for (int run = 0; run < 5; ++run)
		{
			const auto t0 = std::chrono::steady_clock::now();
			float acc = 0.0f;
			for (int j = 0; j < kGridH; ++j)
			{
				for (int i = 0; i < kGridW; ++i)
				{
					acc += sample(static_cast<float>(i) * 0.37f + 0.11f, static_cast<float>(j) * 0.41f + 0.23f);
				}
			}
			const auto t1 = std::chrono::steady_clock::now();
			sink += acc;
			best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
		}
		return best;
	}

	void Report(const char* name, double ms, double releaseLimitMs)
	{
		const double limit = kReleaseLimits ? releaseLimitMs : kDebugLimitMs;
		std::cout << "[NoisePerf] " << name << ": " << ms << " ms for " << (kGridW * kGridH)
				  << " samples (limit " << limit << " ms, " << (kReleaseLimits ? "Release" : "Debug") << ")" << std::endl;
		EXPECT_LT(ms, limit) << name;
	}

	volatile float gPerfSink = 0.0f;
}

TEST(NoisePerf, GridTimings)
{
	float sink = 0.0f;
	const FractalParams fp = Params(6);

	Report("FractalNoise2D<Value> 6 oct", TimeGridMs([&](float x, float y) { return FractalNoise2D<NoiseType::Value>(7u, x, y, fp); }, sink), 1.0);
	Report("FractalNoise2D<Gradient> 6 oct", TimeGridMs([&](float x, float y) { return FractalNoise2D<NoiseType::Gradient>(7u, x, y, fp); }, sink), 1.0);
	Report("FractalNoise2D<Simplex> 6 oct", TimeGridMs([&](float x, float y) { return FractalNoise2D<NoiseType::Simplex>(7u, x, y, fp); }, sink), 1.0);
	Report("FractalNoise3D 6 oct", TimeGridMs([&](float x, float y) { return FractalNoise3D(7u, x, y, 0.5f, fp); }, sink), 2.0);
	Report("FractalNoise2D<Worley> 6 oct", TimeGridMs([&](float x, float y) { return FractalNoise2D<NoiseType::Worley>(7u, x, y, fp); }, sink), 4.0);
	Report("WorleyNoise2D 1 oct", TimeGridMs([&](float x, float y) { const WorleyResult w = WorleyNoise2D(7u, x, y); return w.f1 + w.f2MinusF1; }, sink), 1.0);

	RidgedParams rp;
	rp.fractal = fp;
	Report("RidgedNoise2D<Gradient> 6 oct", TimeGridMs([&](float x, float y) { return RidgedNoise2D<NoiseType::Gradient>(7u, x, y, rp); }, sink), 2.0);
	Report("WarpedNoise2D<Gradient,Gradient> 6 oct", TimeGridMs([&](float x, float y) { return WarpedNoise2D<NoiseType::Gradient, NoiseType::Gradient>(7u, x, y, 3.0f, fp, fp); }, sink), 2.0);

	gPerfSink = sink;   // keeps the accumulated results observable so the calls cannot be elided
}
