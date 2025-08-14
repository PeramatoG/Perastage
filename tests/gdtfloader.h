#pragma once
#include <string>
#include <vector>
#include "../viewer3d/mesh.h"
#include "../models/types.h"
struct GdtfObject { Mesh mesh; Matrix transform; };
bool LoadGdtf(const std::string&, std::vector<GdtfObject>&);
int GetGdtfModeChannelCount(const std::string&, const std::string&);
std::string GetGdtfFixtureName(const std::string& gdtfPath);
std::string GetGdtfModelColor(const std::string& gdtfPath);
