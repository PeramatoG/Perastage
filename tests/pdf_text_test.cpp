/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
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

