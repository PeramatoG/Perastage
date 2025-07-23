#include "mvrexporter.h"
#include "configmanager.h"

#include <wx/wx.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/filename.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool MvrExporter::ExportToFile(const std::string& filePath)
{
    const auto& scene = ConfigManager::Get().GetScene();
    if (scene.basePath.empty() || !fs::exists(scene.basePath))
        return false;

    wxFileOutputStream output(filePath);
    if (!output.IsOk())
        return false;

    wxZipOutputStream zip(output);

    fs::path baseDir = scene.basePath;
    for (auto& p : fs::recursive_directory_iterator(baseDir))
    {
        if (!p.is_regular_file())
            continue;

        fs::path rel = fs::relative(p.path(), baseDir);
        wxZipEntry entry(rel.string());
        entry.SetMethod(wxZIP_METHOD_DEFLATE);
        zip.PutNextEntry(entry);

        std::ifstream in(p.path(), std::ios::binary);
        char buf[4096];
        while (in.good())
        {
            in.read(buf, sizeof(buf));
            std::streamsize s = in.gcount();
            if (s > 0)
                zip.Write(buf, s);
        }
        zip.CloseEntry();
    }

    zip.Close();
    return true;
}
