/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "gdtfloader.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {
std::string MakeGdtf(const std::string& primitiveType)
{
    wxFileName tempName(wxFileName::CreateTempFileName("gdtf_primitive_"));
    const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
    wxRemoveFile(tempName.GetFullPath());

    wxFFileOutputStream fileOut(outPath);
    assert(fileOut.IsOk());
    wxZipOutputStream zipOut(fileOut);

    zipOut.PutNextEntry("description.xml");
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GDTF DataVersion=\"1.2\">"
        "<FixtureType Name=\"Test\">"
        "<Models>"
        "<Model Name=\"Body\" File=\"\" PrimitiveType=\"" + primitiveType + "\" "
        "Length=\"1.0\" Width=\"1.0\" Height=\"1.0\"/>"
        "</Models>"
        "<Geometries>"
        "<Geometry Name=\"Root\" Model=\"Body\"/>"
        "</Geometries>"
        "</FixtureType>"
        "</GDTF>";
    zipOut.Write(xml.data(), xml.size());
    zipOut.Close();

    return outPath;
}
}

int main()
{
    {
        const std::string gdtfPath = MakeGdtf("Cube");
        std::vector<GdtfObject> objects;
        std::string error;
        const bool ok = LoadGdtf(gdtfPath, objects, &error);
        assert(ok);
        assert(error.empty());
        assert(!objects.empty());
        std::error_code ec;
        std::filesystem::remove(gdtfPath, ec);
    }

    {
        const std::string gdtfPath = MakeGdtf("Undefined");
        std::vector<GdtfObject> objects;
        std::string error;
        const bool ok = LoadGdtf(gdtfPath, objects, &error);
        assert(!ok);
        assert(objects.empty());
        assert(!error.empty());
        std::error_code ec;
        std::filesystem::remove(gdtfPath, ec);
    }

    return 0;
}
