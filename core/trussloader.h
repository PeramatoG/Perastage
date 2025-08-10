#pragma once
#include <string>
#include "truss.h"

// Loads a .gtruss archive extracting metadata and model path
bool LoadTrussArchive(const std::string &archivePath, Truss &outTruss);
