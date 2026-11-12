// S-1 World Generation: world files. See WorldFiles.h.
#include <Shared/WorldGeneration/WorldFiles.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace CoW {
namespace WorldGeneration {

namespace
{
    namespace fs = std::filesystem;

    WorldFileResult Fail(WorldFileStatus status, std::string message, std::string path = {})
    {
        WorldFileResult r;
        r.status = status;
        r.message = std::move(message);
        r.path = std::move(path);
        return r;
    }

    bool IsDeviceName(const std::string& name)
    {
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL") { return true; }
        return upper.size() == 4 && (upper.compare(0, 3, "COM") == 0 || upper.compare(0, 3, "LPT") == 0) && upper[3] >= '1' && upper[3] <= '9';
    }

    std::atomic<uint32_t> sTempCounter{ 0 };
}

const char* WorldFileStatusName(WorldFileStatus status)
{
    switch (status)
    {
    case WorldFileStatus::Ok:            return "Ok";
    case WorldFileStatus::InvalidName:   return "InvalidName";
    case WorldFileStatus::NoDirectory:   return "NoDirectory";
    case WorldFileStatus::NotFound:      return "NotFound";
    case WorldFileStatus::AlreadyExists: return "AlreadyExists";
    case WorldFileStatus::TooLarge:      return "TooLarge";
    case WorldFileStatus::IoError:       return "IoError";
    }
    return "Unknown";
}

bool IsValidWorldName(const std::string& name)
{
    if (name.empty() || name.size() > kMaxWorldNameLength) { return false; }
    for (unsigned char c : name)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) { return false; }
    }
    return !IsDeviceName(name);
}

std::string WorldFilePath(const std::string& directory, const std::string& name)
{
    return (fs::path(directory) / (name + kWorldFileExtension)).string();
}

std::string DefaultWorldName(uint64_t seed)
{
    return std::to_string(seed);
}

WorldFileResult WriteWorldFile(const std::string& directory, const std::string& name, const std::string& text, bool overwrite)
{
    if (!IsValidWorldName(name))
    {
        return Fail(WorldFileStatus::InvalidName, "invalid world name \"" + name + "\" (expected [A-Za-z0-9_-]{1,64}, not a device name)");
    }
    if (directory.empty()) { return Fail(WorldFileStatus::NoDirectory, "no worlds directory is configured"); }
    if (text.size() > kMaxWorldJsonBytes)
    {
        return Fail(WorldFileStatus::TooLarge, "world text is larger than " + std::to_string(kMaxWorldJsonBytes) + " bytes");
    }

    const std::string path = WorldFilePath(directory, name);
    std::error_code ec;
    if (!overwrite && fs::exists(path, ec))
    {
        return Fail(WorldFileStatus::AlreadyExists, "world already exists (overwrite not set): " + path, path);
    }
    fs::create_directories(directory, ec);
    if (ec) { return Fail(WorldFileStatus::IoError, "cannot create directory " + directory + ": " + ec.message(), path); }

    // Temporary file next to the target (same volume, so the rename is atomic), unique per call.
    const std::string temp = path + ".tmp" + std::to_string(sTempCounter.fetch_add(1));
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) { return Fail(WorldFileStatus::IoError, "cannot open for writing: " + temp, path); }
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
        if (!file)
        {
            fs::remove(temp, ec);
            return Fail(WorldFileStatus::IoError, "write failed: " + temp, path);
        }
    }
    fs::rename(temp, path, ec);   // replaces an existing file
    if (ec)
    {
        const std::string why = ec.message();
        fs::remove(temp, ec);
        return Fail(WorldFileStatus::IoError, "cannot replace " + path + ": " + why, path);
    }

    WorldFileResult r;
    r.path = path;
    r.bytes = text.size();
    return r;
}

WorldFileResult ReadWorldFile(const std::string& outDirectory, const std::string& deployedDirectory, const std::string& name,
                              std::string& text)
{
    text.clear();
    if (!IsValidWorldName(name))
    {
        return Fail(WorldFileStatus::InvalidName, "invalid world name \"" + name + "\" (expected [A-Za-z0-9_-]{1,64}, not a device name)");
    }
    if (outDirectory.empty() && deployedDirectory.empty()) { return Fail(WorldFileStatus::NoDirectory, "no worlds directory is configured"); }

    std::error_code ec;
    std::string found;
    for (const std::string* dir : { &outDirectory, &deployedDirectory })
    {
        if (dir->empty()) { continue; }
        const std::string candidate = WorldFilePath(*dir, name);
        if (fs::is_regular_file(candidate, ec)) { found = candidate; break; }
    }
    if (found.empty())
    {
        std::string where = outDirectory.empty() ? deployedDirectory : outDirectory;
        if (!outDirectory.empty() && !deployedDirectory.empty()) { where += " or " + deployedDirectory; }
        return Fail(WorldFileStatus::NotFound, "world \"" + name + "\" not found in " + where);
    }

    const uintmax_t size = fs::file_size(found, ec);
    if (ec) { return Fail(WorldFileStatus::IoError, "cannot read size of " + found + ": " + ec.message(), found); }
    if (size > kMaxWorldJsonBytes)
    {
        return Fail(WorldFileStatus::TooLarge, "world file is larger than " + std::to_string(kMaxWorldJsonBytes) + " bytes: " + found, found);
    }

    std::ifstream file(found, std::ios::binary);
    if (!file) { return Fail(WorldFileStatus::IoError, "cannot open for reading: " + found, found); }
    text.resize(static_cast<size_t>(size));
    file.read(text.data(), static_cast<std::streamsize>(size));
    if (!file && size != 0)
    {
        text.clear();
        return Fail(WorldFileStatus::IoError, "read failed: " + found, found);
    }

    WorldFileResult r;
    r.path = found;
    r.bytes = text.size();
    return r;
}

} // namespace WorldGeneration
} // namespace CoW
