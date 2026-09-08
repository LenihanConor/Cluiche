#pragma once

#include <stdint.h>

namespace Dia::SaveGame {

enum class SaveResultCode : uint8_t {
    Ok,
    SlotAtCapacity,
    SerializeError,      // Flush/Write failed (buffer overflow)
    FileWriteError,
};

enum class LoadResultCode : uint8_t {
    Ok,
    SlotNotFound,
    FileReadError,
    ParseError,
    EngineMismatch,
    MigrationError,
    DeserializeError,
};

struct [[nodiscard]] SaveResult {
    SaveResultCode code;
    bool Ok() const { return code == SaveResultCode::Ok; }
    static SaveResult Success()          { return {SaveResultCode::Ok}; }
    static SaveResult Fail(SaveResultCode c) { return {c}; }
};

struct [[nodiscard]] LoadResult {
    LoadResultCode code;
    bool Ok() const { return code == LoadResultCode::Ok; }
    static LoadResult Success()          { return {LoadResultCode::Ok}; }
    static LoadResult Fail(LoadResultCode c) { return {c}; }
};

} // namespace Dia::SaveGame
