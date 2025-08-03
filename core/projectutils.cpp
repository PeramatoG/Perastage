#include "projectutils.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace ProjectUtils {

std::string GetLastProjectPathFile()
{
    wxString dir = wxStandardPaths::Get().GetUserDataDir();
    fs::path p = fs::path(dir.ToStdString());
    fs::create_directories(p);
    p /= "last_project.txt";
    return p.string();
}

bool SaveLastProjectPath(const std::string& path)
{
    std::ofstream out(GetLastProjectPathFile());
    if (!out.is_open())
        return false;
    out << path;
    return true;
}

std::optional<std::string> LoadLastProjectPath()
{
    std::ifstream in(GetLastProjectPathFile());
    if (!in.is_open())
        return std::nullopt;
    std::string path;
    std::getline(in, path);
    if (path.empty())
        return std::nullopt;
    return path;
}

std::string GetDefaultLibraryPath(const std::string& subdir)
{
    // Resolve the library directory relative to the executable location to
    // avoid relying on the current working directory at runtime.
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    fs::path base = fs::path(exe.GetPath().ToStdString());
    fs::path p = base / "library" / subdir;
    if (fs::exists(p))
        return p.string();
    return {};
}

} // namespace ProjectUtils

