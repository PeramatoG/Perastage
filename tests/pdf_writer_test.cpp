#include "pdf_draw_commands.h"
#include "pdf_objects.h"
#include "pdf_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
  const std::filesystem::path outPath =
      std::filesystem::temp_directory_path() / "perastage_pdf_writer_test.pdf";

  std::vector<PdfObject> objects;
  objects.push_back({"<< /Type /Catalog /Pages 2 0 R >>"});
  objects.push_back({"<< /Type /Pages /Kids [3 0 R] /Count 1 >>"});
  objects.push_back({"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] /Contents 4 0 R >>"});
  objects.push_back({"<< /Length 0 >>\nstream\n\nendstream"});

  std::string error;
  if (!WritePdfDocument(outPath, objects, 1, error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  std::ifstream in(outPath, std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.find("xref") == std::string::npos || data.find("%%EOF") == std::string::npos) {
    std::cerr << "Missing xref or EOF markers" << std::endl;
    return 1;
  }

  std::filesystem::remove(outPath);

  // Non-regression check: draw command serialization remains stable after
  // splitting layout_pdf_exporter internals into dedicated modules.
  {
    layout_pdf_internal::Mapping mapping;
    mapping.scale = 1.0;
    mapping.flipY = false;

    layout_pdf_internal::Transform transform;
    layout_pdf_internal::RenderOptions options;
    layout_pdf_internal::GraphicsStateCache cache;
    layout_pdf_internal::FloatFormatter fmt(3);

    LineCommand line;
    line.x0 = 0.0f;
    line.y0 = 0.0f;
    line.x1 = 10.0f;
    line.y1 = 10.0f;
    line.stroke.width = 1.0f;
    line.stroke.color = {0.0f, 0.0f, 0.0f};

    std::ostringstream content;
    layout_pdf_internal::EmitCommandStroke(content, cache, fmt, mapping,
                                           transform, CanvasCommand{line},
                                           options);
    const std::string expected =
        "1 j\n1 J\n0.000 0.000 0.000 RG\n1.000 w\n"
        "0.000 0.000 m\n10.000 10.000 l\nS\n";
    if (content.str() != expected) {
      std::cerr << "Unexpected serialized draw command output" << std::endl;
      return 1;
    }
  }
  return 0;
}
