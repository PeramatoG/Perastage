/* This file is part of Perastage. */
#pragma once
#include "mvrscene.h"
namespace tinyxml2 { class XMLElement; }
namespace mvr {
// Canonical MVR matrices contain exactly four comma-separated triples.
std::optional<MvrMatrixSource> ParseCanonicalMatrix(const std::string &text);
std::optional<MvrMatrixSource> ReadCanonicalMatrixElement(const tinyxml2::XMLElement &element);
std::string FormatCanonicalMatrix(const Matrix &edited,
                                 const std::optional<MvrMatrixSource> &source);
bool CanonicalMatrixIsIdentity(const Matrix &matrix,
                               const std::optional<MvrMatrixSource> &source);
// MVR 1.6 Table 17 permits elevation only, including source precision.
bool CanonicalLayerMatrixIsElevation(const Matrix &matrix,
                                     const std::optional<MvrMatrixSource> &source);
std::optional<MvrMatrixSource> ReadMatrixSource(tinyxml2::XMLElement &node, MvrScene &scene);
// Final numeric serialization uses existing node ownership and current local transforms.
void WriteCanonicalMatrices(tinyxml2::XMLElement &sceneElement, const MvrScene &scene);
} // namespace mvr
