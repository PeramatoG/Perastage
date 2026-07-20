/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "../viewer3d/gdtfloader.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {

// Creates a minimal GDTF with a top-level beam definition referenced beneath Head.
std::string MakeReferencedGeometryGdtf()
{
    wxFileName tempName(wxFileName::CreateTempFileName("gdtf_geometry_roots_"));
    const std::string outPath = tempName.GetFullPath().ToStdString() + ".gdtf";
    wxRemoveFile(tempName.GetFullPath());

    wxFFileOutputStream fileOut(outPath);
    assert(fileOut.IsOk());
    wxZipOutputStream zipOut(fileOut);
    zipOut.PutNextEntry("description.xml");
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<GDTF DataVersion=\"1.2\">"
        "<FixtureType Name=\"ReferencedGeometryFixture\">"
        "<Models>"
        "<Model Name=\"BaseModel\" PrimitiveType=\"Cube\" Length=\"1\" Width=\"1\" Height=\"1\"/>"
        "<Model Name=\"BeamModel\" PrimitiveType=\"Cylinder\" Length=\"0.2\" Width=\"0.2\" Height=\"0.2\"/>"
        "</Models>"
        "<Geometries>"
        "<Geometry Name=\"Base\" Model=\"BaseModel\">"
        "<Geometry Name=\"Head\" Position=\"{1,0,0,1}{0,1,0,0}{0,0,1,0}{0,0,0,1}\">"
        "<GeometryReference Name=\"MountedBeam\" Geometry=\"BeamLED\" "
        "Position=\"{1,0,0,2}{0,1,0,0}{0,0,1,0}{0,0,0,1}\"/>"
        "</Geometry>"
        "</Geometry>"
        "<Beam Name=\"BeamLED\" Model=\"BeamModel\"/>"
        "</Geometries>"
        "<DMXModes>"
        "<DMXMode Name=\"Standard\" Geometry=\"Base\"/>"
        "</DMXModes>"
        "</FixtureType>"
        "</GDTF>";
    zipOut.Write(xml.data(), xml.size());
    zipOut.Close();
    return outPath;
}

// Compares floating-point transform components with a small tolerance.
bool NearlyEqual(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) < 0.0001f;
}

} // namespace

// Verifies that referenced top-level geometry is rendered only through its reference.
int main()
{
    const std::string gdtfPath = MakeReferencedGeometryGdtf();

    std::vector<GdtfObject> objects;
    std::string loadError;
    assert(LoadGdtf(gdtfPath, objects, "Standard", &loadError));
    assert(objects.size() == 2);
    assert(NearlyEqual(objects[0].transform.o[0], 0.0f));
    assert(NearlyEqual(objects[1].transform.o[0], 3.0f));

    GdtfGeometryTree tree;
    assert(LoadGdtfGeometryTree(gdtfPath, tree, "Standard", &loadError));
    assert(tree.emitterNodeIndices.size() == 1);
    const GdtfNode3D& emitter = tree.nodes[tree.emitterNodeIndices.front()];
    assert(emitter.parentIndex >= 0);
    assert(NearlyEqual(emitter.worldTransform.o[0], 3.0f));

    std::vector<GdtfObject> defaultModeObjects;
    assert(LoadGdtf(gdtfPath, defaultModeObjects, &loadError));
    assert(defaultModeObjects.size() == 2);
    assert(NearlyEqual(defaultModeObjects[1].transform.o[0], 3.0f));

    std::error_code ec;
    std::filesystem::remove(gdtfPath, ec);
    return 0;
}
