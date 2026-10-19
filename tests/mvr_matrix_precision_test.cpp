/* This file is part of Perastage. */
#include "mvr_matrix_precision.h"
#include "mvr_symdef_preservation.h"
#include "matrixutils.h"
#include <cassert>
#include <limits>
#include <locale>
#include <random>
#include <bit>
#include <tinyxml2.h>

namespace {
struct DecimalComma : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};
}
int main() {
  const auto original = std::locale();
  std::locale::global(std::locale(original, new DecimalComma));
  const std::string text = "{0.81234567890123456,+0.12345678901234567,0}{-0.12345678901234567,0.81234567890123456,0}{0,0,1.2345678901234567}{123456.78901234567,-8.53590478e-08,3000.123456789}";
  const auto source = mvr::ParseCanonicalMatrix(text);
  assert(source);
  Matrix projected;
  assert(MatrixUtils::ParseMatrix(text, projected));
  assert(!mvr::SymdefMatrixPreservesSource(text, projected));
  assert(mvr::SymdefMatrixPreservesSource(text, projected, source));
  const auto formatted = mvr::FormatCanonicalMatrix(projected, source);
  assert(mvr::ParseCanonicalMatrix(formatted)->values == source->values);
  assert(mvr::FormatCanonicalMatrix(projected, source) == formatted);
  Matrix edited = projected;
  edited.o[0] = std::nextafter(edited.o[0], std::numeric_limits<float>::infinity());
  assert(!mvr::SymdefMatrixPreservesSource(text, edited, source));
  const auto editedText = mvr::FormatCanonicalMatrix(edited, source);
  const auto editedSource = mvr::ParseCanonicalMatrix(editedText);
  assert(editedSource && editedSource->values[9] == static_cast<double>(edited.o[0]));
  for (size_t index = 0; index < source->values.size(); ++index)
    if (index != 9) assert(editedSource->values[index] == source->values[index]);
  assert(mvr::FormatCanonicalMatrix(editedSource->projection, editedSource) == editedText);
  assert(mvr::ParseCanonicalMatrix(mvr::FormatCanonicalMatrix(edited, source))->values != source->values);
  const auto hiddenNonidentity = mvr::ParseCanonicalMatrix("{1.0000000000000002,0,0}{0,1,0}{0,0,1}{0,0,0}");
  assert(hiddenNonidentity && !mvr::CanonicalMatrixIsIdentity(hiddenNonidentity->projection, hiddenNonidentity));
  assert(mvr::CanonicalMatrixIsIdentity(Matrix{}, std::nullopt));
  for (const auto &bad : {"", "1,0,0,0,1,0,0,0,1,0,0,0", "{1 0 0}{0 1 0}{0 0 1}{0 0 0}",
      "{1,0,0}{0,1,0}{0,0,1}{0,0,0}junk", "{1,0,0}{0,1,0}{0,0,1}{0,0,0}{1,2,3}",
      "{1,0,0}{0,1,0}{0,0,1}{nan,0,0}", "{1,0,0}{0,1,0}{0,0,1}{inf,0,0}",
      "{1,0,0}{0,1,0}{0,0,1}{1e999,0,0}", "{1,0,0}{0,1,0}{0,0,1}{1e39,0,0}",
      "{1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,1}", "{1,0,0}{0,1,0}{0,0,1}{0,,0,0}"})
    assert(!mvr::ParseCanonicalMatrix(bad));
  assert(mvr::ParseCanonicalMatrix(" \n {1, 0, 0} {0, 1, 0} {0,0,1} {+0,-0,0e0}\t"));
  tinyxml2::XMLDocument document;
  assert(document.Parse("<Truss><Matrix><![CDATA[{1,0,0}]]><!-- split numeric text -->{0,1,0}{0,0,1}{1.2345678901234567,0,0}</Matrix></Truss>") == tinyxml2::XML_SUCCESS);
  MvrScene scene;
  tinyxml2::XMLDocument absent;
  assert(absent.Parse("<Truss/>") == tinyxml2::XML_SUCCESS);
  const auto absentSource = mvr::ReadMatrixSource(*absent.RootElement(), scene);
  assert(absentSource && mvr::CanonicalMatrixIsIdentity(absentSource->projection, absentSource));
  assert(mvr::ReadMatrixSource(*document.RootElement(), scene));
  assert(scene.preservationIssues.empty());
  for (const auto *invalid : {
      "<Truss><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix></Truss>",
      "<Truss><Matrix future='x'>{1,0,0}{0,1,0}{0,0,1}{0,0,0}</Matrix></Truss>",
      "<Truss><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}<Extra/></Matrix></Truss>",
      "<Truss><Matrix>{1,0,0}{0,1,0}{0,0,1}{0,0,0}<!-- split -->garbage</Matrix></Truss>"}) {
    assert(document.Parse(invalid) == tinyxml2::XML_SUCCESS);
    MvrScene invalidScene;
    assert(!mvr::ReadMatrixSource(*document.RootElement(), invalidScene));
    assert(invalidScene.preservationIssues.size() == 1);
    assert(!invalidScene.preservationIssues.front().reason.empty());
  }
  const auto legacy = "<Truss><Matrix>1,0,0,0,1,0,0,0,1,0.12345678901234567,0,0</Matrix>"
      "<UserData><Data provider='Perastage' ver='1.0'><TrussInfo/></Data></UserData></Truss>";
  assert(document.Parse(legacy) == tinyxml2::XML_SUCCESS);
  MvrScene legacyScene;
  legacyScene.provider = "PERASTAGE";
  const auto legacySource = mvr::ReadMatrixSource(*document.RootElement(), legacyScene);
  assert(legacySource && legacySource->values[9] == 0.12345678901234567);
  assert(legacyScene.preservationIssues.empty());
  MvrScene foreignScene;
  foreignScene.provider = "External";
  assert(!mvr::ReadMatrixSource(*document.RootElement(), foreignScene));
  assert(!foreignScene.preservationIssues.empty());
  assert(document.Parse("<Truss><Matrix>1,0,0,0,1,0,0,0,1,0,0,0</Matrix></Truss>") == tinyxml2::XML_SUCCESS);
  assert(!mvr::ReadMatrixSource(*document.RootElement(), legacyScene));
  std::mt19937 random(42);
  for (int i = 0; i < 10000; ++i) {
    const auto value = std::bit_cast<float>(static_cast<uint32_t>(random()));
    if (!std::isfinite(value)) continue;
    Matrix matrix;
    matrix.o[0] = value;
    Matrix roundTrip;
    assert(MatrixUtils::ParseMatrix(MatrixUtils::FormatMatrix(matrix), roundTrip));
    assert(roundTrip.o == matrix.o);
  }
  std::locale::global(original);
}
