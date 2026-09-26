/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "cli_runner.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
// Adapts native UTF-16 Windows arguments to the runner's UTF-8 contract.
int wmain(int argc, wchar_t *argv[]) {
  std::vector<std::string> ownedArgs;
  ownedArgs.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index) {
    const std::u8string utf8 = std::filesystem::path(argv[index]).u8string();
    ownedArgs.emplace_back(reinterpret_cast<const char *>(utf8.data()),
                           utf8.size());
  }
  std::vector<std::string_view> args;
  args.reserve(ownedArgs.size());
  for (const std::string &argument : ownedArgs)
    args.emplace_back(argument);
  return perastage::cli::Run(args, std::cout, std::cerr);
}
#else
// Adapts process arguments to the independently testable CLI runner.
int main(int argc, char *argv[]) {
  std::vector<std::string_view> args;
  args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index)
    args.emplace_back(argv[index]);
  return perastage::cli::Run(args, std::cout, std::cerr);
}
#endif
