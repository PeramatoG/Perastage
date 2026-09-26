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

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct ExpectedResult {
  int exitCode;
  std::string output;
  std::string error;
};

// Executes one runner case and verifies its complete observable result twice.
bool CheckCase(const std::vector<std::string_view> &args,
               const ExpectedResult &expected) {
  for (int repetition = 0; repetition < 2; ++repetition) {
    std::ostringstream out;
    std::ostringstream err;
    const int exitCode = perastage::cli::Run(args, out, err);
    if (exitCode != expected.exitCode || out.str() != expected.output ||
        err.str() != expected.error) {
      std::cerr << "CLI runner case failed for repetition " << repetition
                << '\n';
      return false;
    }
  }
  return true;
}

} // namespace

// Verifies top-level and inspect grammar with deterministic output routing.
int main() {
  const std::string help =
      "Usage: perastage-cli [--help | --version]\n"
      "       perastage-cli inspect <file> [--view <view> | --json]\n"
      "\n"
      "Options:\n"
      "  -h, --help  Show this help and exit.\n"
      "  --version   Show the CLI version and exit.\n"
      "\n"
      "Commands:\n"
      "  inspect     Inspect a GDTF or MVR package.\n";
  const std::string inspectHelp =
      "Usage: perastage-cli inspect <file> [--view <view> | --json]\n\n"
      "Views:\n"
      "  summary      Show the format-specific summary (default).\n"
      "  inventory    Show ordered package entries.\n"
      "  resources    Show ordered resource descriptors.\n"
      "  diagnostics  Show inspection and validation findings.\n"
      "  xml          Emit the retained root XML exactly.\n\n"
      "Options:\n"
      "  --view <view>  Select one human-readable view.\n"
      "  --json         Emit one complete structured report.\n"
      "  -h, --help     Show this help and exit.\n";
  const auto usageError = [](std::string_view argument) {
    return "perastage-cli: unexpected argument: " + std::string(argument) +
           "\nTry 'perastage-cli --help' for usage.\n";
  };

  bool passed = true;
  passed &= CheckCase({}, {0, help, ""});
  passed &= CheckCase({"-h"}, {0, help, ""});
  passed &= CheckCase({"--help"}, {0, help, ""});
  passed &= CheckCase(
      {"--version"}, {0, "Perastage CLI " PERASTAGE_CLI_TEST_VERSION "\n", ""});
  passed &= CheckCase({"--unknown"}, {2, "", usageError("--unknown")});
  passed &= CheckCase({"file.mvr"}, {2, "", usageError("file.mvr")});
  passed &= CheckCase({"--help", "extra"}, {2, "", usageError("extra")});
  passed &= CheckCase({"--version", "extra"}, {2, "", usageError("extra")});
  passed &= CheckCase({"inspect", "--help"}, {0, inspectHelp, ""});
  passed &=
      CheckCase({"inspect"},
                {2, "",
                 "perastage-cli inspect: exactly one input file is "
                 "required.\nTry 'perastage-cli inspect --help' for usage.\n"});
  passed &= CheckCase({"inspect", "file.mvr", "--bad"},
                      {2, "",
                       "perastage-cli inspect: unknown option: --bad\nTry "
                       "'perastage-cli inspect --help' for usage.\n"});
  passed &= CheckCase({"inspect", "file.mvr", "--view"},
                      {2, "",
                       "perastage-cli inspect: missing value for --view.\nTry "
                       "'perastage-cli inspect --help' for usage.\n"});
  passed &= CheckCase({"inspect", "file.mvr", "--view", "bad"},
                      {2, "",
                       "perastage-cli inspect: unknown view: bad\nTry "
                       "'perastage-cli inspect --help' for usage.\n"});
  passed &=
      CheckCase({"inspect", "file.mvr", "--view", "summary", "--view", "xml"},
                {2, "",
                 "perastage-cli inspect: duplicate --view option.\nTry "
                 "'perastage-cli inspect --help' for usage.\n"});
  passed &= CheckCase({"inspect", "file.mvr", "--json", "--json"},
                      {2, "",
                       "perastage-cli inspect: duplicate --json option.\nTry "
                       "'perastage-cli inspect --help' for usage.\n"});
  passed &= CheckCase(
      {"inspect", "file.mvr", "--json", "--view", "summary"},
      {2, "",
       "perastage-cli inspect: --json and --view are mutually exclusive.\nTry "
       "'perastage-cli inspect --help' for usage.\n"});
  passed &=
      CheckCase({"inspect", "file.mvr", "extra.mvr"},
                {2, "",
                 "perastage-cli inspect: extra positional argument: "
                 "extra.mvr\nTry 'perastage-cli inspect --help' for usage.\n"});
  return passed ? 0 : 1;
}
