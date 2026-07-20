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
#include "../viewer3d/gdtfloader.h"

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
// Creates a temporary single-model primitive GDTF archive.
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

// Creates a temporary primitive-only fixture with a nested geometry hierarchy.
std::string MakePrimitiveFixtureGdtf()
{
    wxFileName tempName(wxFileName::CreateTempFileName("gdtf_primitive_multi_"));
    const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
    wxRemoveFile(tempName.GetFullPath());

    wxFFileOutputStream fileOut(outPath);
    assert(fileOut.IsOk());
    wxZipOutputStream zipOut(fileOut);

    zipOut.PutNextEntry("description.xml");
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GDTF DataVersion=\"1.2\">"
        "<FixtureType Name=\"PrimitiveOnly\">"
        "<Models>"
        "<Model Name=\"Base\" File=\"\" PrimitiveType=\"Base\" Length=\"0.4\" Width=\"0.4\" Height=\"0.1\"/>"
        "<Model Name=\"Yoke\" File=\"\" PrimitiveType=\"Yoke\" Length=\"0.5\" Width=\"0.2\" Height=\"0.5\"/>"
        "<Model Name=\"Head\" File=\"\" PrimitiveType=\"Head\" Length=\"0.3\" Width=\"0.3\" Height=\"0.3\"/>"
        "<Model Name=\"Beam\" File=\"\" PrimitiveType=\"Cylinder\" Length=\"0.1\" Width=\"0.1\" Height=\"0.8\"/>"
        "</Models>"
        "<Geometries>"
        "<Geometry Name=\"Root\" Model=\"Base\">"
        "<Axis Name=\"YokeAxis\" Model=\"Yoke\">"
        "<Axis Name=\"HeadAxis\" Model=\"Head\">"
        "<Beam Name=\"BeamNode\" Model=\"Beam\"/>"
        "</Axis>"
        "</Axis>"
        "</Geometry>"
        "</Geometries>"
        "<DMXModes><DMXMode Name=\"Default\" Geometry=\"Root\"/></DMXModes>"
        "</FixtureType>"
        "</GDTF>";
    zipOut.Write(xml.data(), xml.size());
    zipOut.Close();
    return outPath;
}
}

// Runs primitive GDTF loader regression checks.
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
        const std::string gdtfPath = MakePrimitiveFixtureGdtf();
        std::vector<GdtfObject> objects;
        GdtfGeometryTree tree;
        std::string error;
        assert(LoadGdtf(gdtfPath, objects, "Default", &error));
        assert(error.empty());
        assert(objects.size() == 4);
        assert(LoadGdtfGeometryTree(gdtfPath, tree, "Default", &error));
        assert(tree.nodes.size() == 4);
        assert(tree.axisNodeIndices.size() == 2);
        assert(tree.emitterNodeIndices.size() == 1);
        assert(tree.nodes[0].stableName == "Geometry_Root");
        assert(tree.nodes[1].stableName == "Axis_YokeAxis");
        assert(tree.nodes[2].stableName == "Axis_HeadAxis");
        assert(tree.nodes[3].stableName == "Emitter_BeamNode");

        std::vector<GdtfObject> objectsAgain;
        GdtfGeometryTree treeAgain;
        assert(LoadGdtf(gdtfPath, objectsAgain, "Default", &error));
        assert(LoadGdtfGeometryTree(gdtfPath, treeAgain, "Default", &error));
        assert(objectsAgain.size() == objects.size());
        assert(treeAgain.nodes.size() == tree.nodes.size());
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
