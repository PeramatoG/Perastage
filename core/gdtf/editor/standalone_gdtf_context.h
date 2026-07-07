#pragma once

#include "gdtf_edit_session.h"

namespace gdtf {

GdtfEditorContext BuildStandaloneGdtfEditorContext(
    const std::filesystem::path &sourcePath,
    GdtfWritePolicy writePolicy = GdtfWritePolicy::ReadOnly);
GdtfEditSession BuildStandaloneGdtfEditSession(
    const std::filesystem::path &sourcePath,
    GdtfWritePolicy writePolicy = GdtfWritePolicy::ReadOnly);

} // namespace gdtf
