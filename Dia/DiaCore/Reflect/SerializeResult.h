#pragma once
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Strings/String128.h"

namespace Dia::Reflect {

enum class SerializeErrorKind : uint8_t {
    RequiredFieldMissing,
    TypeMismatch,
    CapacityExceeded,
    UnknownPolymorphicType,
    RangeViolation,
    BufferOverflow,
};

struct SerializeError {
    SerializeErrorKind kind;
    Dia::Core::StringCRC fieldName;
    Dia::Core::Containers::String128 message;
};

class SerializeResult {
public:
    static constexpr unsigned int kMaxErrors = 16;

    SerializeResult() = default;

    bool IsOk() const { return mErrors.Size() == 0; }
    bool HasErrors() const { return mErrors.Size() > 0; }
    unsigned int ErrorCount() const { return mErrors.Size(); }
    const SerializeError& GetError(unsigned int index) const { return mErrors.At(index); }

    void AddError(SerializeErrorKind kind, Dia::Core::StringCRC fieldName, const char* message) {
        if (!mErrors.IsFull()) {
            SerializeError err;
            err.kind = kind;
            err.fieldName = fieldName;
            err.message = Dia::Core::Containers::String128(message);
            mErrors.Add(err);
        }
    }

    void Clear() { mErrors.RemoveAll(); }

private:
    Dia::Core::Containers::DynamicArrayC<SerializeError, kMaxErrors> mErrors;
};

} // namespace Dia::Reflect
