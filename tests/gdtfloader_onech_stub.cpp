#include <string>
#include <vector>
#include "../models/types.h"
#include "../viewer3d/mesh.h"

struct GdtfObject { Mesh mesh; Matrix transform; };

bool LoadGdtf(const std::string&, std::vector<GdtfObject>&) { return false; }
int GetGdtfModeChannelCount(const std::string&, const std::string&) { return 1; }
std::string GetGdtfFixtureName(const std::string& gdtfPath) { return gdtfPath; }
