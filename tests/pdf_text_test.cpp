#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "../core/pdftext.h"

static std::string RunPdftotext(const std::string &path) {
  std::string cmd = std::string("/usr/bin/pdftotext -layout \"") + path + "\" -";
  FILE *pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return {};
  char buffer[256];
  std::string out;
  while (fgets(buffer, sizeof(buffer), pipe))
    out += buffer;
  pclose(pipe);
  return out;
}

int main(int argc, char **argv) {
  if (argc < 2)
    return 1;
  for (int i = 1; i < argc; ++i) {
    std::string pdf = argv[i];
    std::string expected = RunPdftotext(pdf);
    while (!expected.empty() && (expected.back() == '\n' || expected.back() == '\f'))
      expected.pop_back();
    const char *oldPath = getenv("PATH");
    setenv("PATH", "", 1); // force ExtractPdfText to use PoDoFo
    std::string actual = ExtractPdfText(pdf);
    while (!actual.empty() && (actual.back() == '\n' || actual.back() == '\f'))
      actual.pop_back();
    if (oldPath)
      setenv("PATH", oldPath, 1);
    else
      unsetenv("PATH");
    if (actual != expected) {
      std::cerr << "Mismatch for " << pdf << "\nExpected:\n" << expected
                << "Actual:\n" << actual << std::endl;
      return 1;
    }
  }
  return 0;
}
