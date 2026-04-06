#pragma once

#include <string>

namespace mvr {

bool ResolvePrimitiveTokenFromModelRef(const std::string &modelRef,
                                       std::string &outPrimitiveToken);
std::string PrimitiveArchivePathForToken(const std::string &primitiveToken);
std::string PrimitiveArchivePathForToken(const std::string &primitiveToken,
                                         const std::string &objectUuid);
bool WritePrimitiveModelForToken(const std::string &primitiveToken,
                                 const std::string &outputPath);

} // namespace mvr
