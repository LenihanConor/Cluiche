#include "JsonSpatialGridSerializer.h"
#include "ISpatialGridSerializer.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaLogger/DiaLog.h>
#include <cstring>

namespace Dia
{
	namespace Geometry2D
	{
		static const char* kSchemaVersion = "1.0";

		const char* JsonSpatialGridSerializer::GetVersion() const
		{
			return kSchemaVersion;
		}

		// ---------------------------------------------------------------------------
		// Shared helpers
		// ---------------------------------------------------------------------------

		static bool ParseAARect(const Json::Value& root, const char* key, AARect& out, const char* logCtx)
		{
			if (!root.isMember(key) || !root[key].isObject())
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: missing '%s' in %s", key, logCtx);
				return false;
			}

			const Json::Value& r = root[key];
			if (!r.isMember("bottom_left") || !r["bottom_left"].isArray() || r["bottom_left"].size() != 2 ||
				!r.isMember("top_right")   || !r["top_right"].isArray()   || r["top_right"].size()   != 2)
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: malformed world_bounds in %s", logCtx);
				return false;
			}

			Dia::Maths::Vector2D bl(
				r["bottom_left"][0u].asFloat(),
				r["bottom_left"][1u].asFloat());
			Dia::Maths::Vector2D tr(
				r["top_right"][0u].asFloat(),
				r["top_right"][1u].asFloat());
			out = AARect(bl, tr);
			return true;
		}

		static Json::Value SerialiseAARect(const AARect& rect)
		{
			Json::Value r;
			Json::Value bl(Json::arrayValue);
			bl.append(rect.GetBottomLeft().X());
			bl.append(rect.GetBottomLeft().Y());
			Json::Value tr(Json::arrayValue);
			tr.append(rect.GetTopRight().X());
			tr.append(rect.GetTopRight().Y());
			r["bottom_left"] = bl;
			r["top_right"]   = tr;
			return r;
		}

		static Dia::Serializer::SerializeResult WriteToBuffer(const Json::Value& root, char* outBuffer, unsigned int bufferSize, const char* logCtx)
		{
			Json::StyledWriter writer;
			std::string output = writer.write(root);

			if (output.size() + 1 > bufferSize)
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: buffer too small in %s", logCtx);
				return Dia::Serializer::SerializeResult::Failure("output buffer too small");
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			return Dia::Serializer::SerializeResult::Success();
		}

		// ---------------------------------------------------------------------------
		// SpatialGrid
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult JsonSpatialGridSerializer::LoadSpatialGrid(const char* data, SpatialGridConfig& outCfg) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: failed to parse JSON (spatial_grid)");
				return Dia::Serializer::SerializeResult::Failure("json parse error");
			}

			if (!ParseAARect(root, "world_bounds", outCfg.worldBounds, "spatial_grid"))
				return Dia::Serializer::SerializeResult::Failure("missing world_bounds");

			if (!root.isMember("cell_size"))
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: missing cell_size");
				return Dia::Serializer::SerializeResult::Failure("missing cell_size");
			}

			outCfg.cellSize = root["cell_size"].asFloat();

			if (outCfg.cellSize <= 0.0f)
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: cell_size must be > 0");
				return Dia::Serializer::SerializeResult::Failure("cell_size must be > 0");
			}

			if (outCfg.worldBounds.GetBottomLeft().X() >= outCfg.worldBounds.GetTopRight().X() ||
				outCfg.worldBounds.GetBottomLeft().Y() >= outCfg.worldBounds.GetTopRight().Y())
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: world_bounds are inverted or zero-area");
				return Dia::Serializer::SerializeResult::Failure("world_bounds inverted or zero-area");
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonSpatialGridSerializer::SaveSpatialGrid(const SpatialGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["world_bounds"] = SerialiseAARect(cfg.worldBounds);
			root["cell_size"]    = cfg.cellSize;
			return WriteToBuffer(root, outBuffer, bufferSize, "spatial_grid");
		}

		// ---------------------------------------------------------------------------
		// HexGrid
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult JsonSpatialGridSerializer::LoadHexGrid(const char* data, HexGridConfig& outCfg) const
		{
			Json::Value root;
			Json::Reader reader;

			if (!reader.parse(data, root))
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: failed to parse JSON (hex_grid)");
				return Dia::Serializer::SerializeResult::Failure("json parse error");
			}

			if (!ParseAARect(root, "world_bounds", outCfg.worldBounds, "hex_grid"))
				return Dia::Serializer::SerializeResult::Failure("missing world_bounds");

			if (!root.isMember("hex_radius"))
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: missing hex_radius");
				return Dia::Serializer::SerializeResult::Failure("missing hex_radius");
			}

			outCfg.hexRadius = root["hex_radius"].asFloat();

			if (outCfg.hexRadius <= 0.0f)
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: hex_radius must be > 0");
				return Dia::Serializer::SerializeResult::Failure("hex_radius must be > 0");
			}

			if (outCfg.worldBounds.GetBottomLeft().X() >= outCfg.worldBounds.GetTopRight().X() ||
				outCfg.worldBounds.GetBottomLeft().Y() >= outCfg.worldBounds.GetTopRight().Y())
			{
				DIA_LOG_WARNING("Geometry2D", "JsonSpatialGridSerializer: world_bounds are inverted or zero-area");
				return Dia::Serializer::SerializeResult::Failure("world_bounds inverted or zero-area");
			}

			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult JsonSpatialGridSerializer::SaveHexGrid(const HexGridConfig& cfg, char* outBuffer, unsigned int bufferSize) const
		{
			Json::Value root;
			root["world_bounds"] = SerialiseAARect(cfg.worldBounds);
			root["hex_radius"]   = cfg.hexRadius;
			return WriteToBuffer(root, outBuffer, bufferSize, "hex_grid");
		}

		// ---------------------------------------------------------------------------
		// ISpatialGridSerializer file helpers
		// ---------------------------------------------------------------------------

		Dia::Serializer::SerializeResult ISpatialGridSerializer::LoadSpatialGridFromFile(const char* path, SpatialGridConfig& outCfg) const
		{
			char buffer[4096];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return LoadSpatialGrid(buffer, outCfg);
		}

		Dia::Serializer::SerializeResult ISpatialGridSerializer::SaveSpatialGridToFile(const char* path, const SpatialGridConfig& cfg) const
		{
			char buffer[4096];
			auto result = SaveSpatialGrid(cfg, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}

		Dia::Serializer::SerializeResult ISpatialGridSerializer::LoadHexGridFromFile(const char* path, HexGridConfig& outCfg) const
		{
			char buffer[4096];
			if (!ReadFileToBuffer(path, buffer, sizeof(buffer)))
				return Dia::Serializer::SerializeResult::Failure("file read error");
			return LoadHexGrid(buffer, outCfg);
		}

		Dia::Serializer::SerializeResult ISpatialGridSerializer::SaveHexGridToFile(const char* path, const HexGridConfig& cfg) const
		{
			char buffer[4096];
			auto result = SaveHexGrid(cfg, buffer, sizeof(buffer));
			if (!result)
				return result;
			if (!WriteBufferToFile(path, buffer, static_cast<unsigned int>(strlen(buffer))))
				return Dia::Serializer::SerializeResult::Failure("file write error");
			return Dia::Serializer::SerializeResult::Success();
		}
	}
}
