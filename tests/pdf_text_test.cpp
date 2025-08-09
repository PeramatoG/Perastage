#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include "../core/pdftext.h"

static std::string ReadFile(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string out = buffer.str();
  while (!out.empty() && (out.back() == '\n' || out.back() == '\f'))
    out.pop_back();
  return out;
}

int main(int argc, char **argv) {
  if (argc < 2)
    return 1;
  for (int i = 1; i < argc; ++i) {
    std::string pdf = argv[i];
    std::string expectedPath = pdf;
    size_t pos = expectedPath.find_last_of('.');
    if (pos != std::string::npos)
      expectedPath.replace(pos, std::string::npos, ".txt");
    else
      expectedPath += ".txt";
    std::string expected = ReadFile(expectedPath);
    std::string actual = ExtractPdfText(pdf);
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == '\f'))
      actual.pop_back();
    if (actual != expected) {
      std::cerr << "Mismatch for " << pdf << "\nExpected:\n" << expected
                << "Actual:\n" << actual << std::endl;
      return 1;
    }
  }
  return 0;
}

