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

#include "app_version.h"
#include "inspect_command.h"

#include <exception>
#include <ostream>
#include <string_view>

namespace perastage::cli {
namespace {

constexpr std::string_view kHelp =
    "Usage: perastage-cli [--help | --version]\n"
    "       perastage-cli inspect <file> [--view <view> | --json]\n"
    "\n"
    "Options:\n"
    "  -h, --help  Show this help and exit.\n"
    "  --version   Show the CLI version and exit.\n"
    "\n"
    "Commands:\n"
    "  inspect     Inspect a GDTF or MVR package.\n";

// Writes a stable usage diagnostic and returns the usage-error status.
int UsageError(std::string_view argument, std::ostream &err) {
  err << "perastage-cli: unexpected argument: " << argument << '\n';
  err << "Try 'perastage-cli --help' for usage.\n";
  return 2;
}

} // namespace

// Runs the CLI grammar without initializing application or GUI state.
int Run(std::span<const std::string_view> args, std::ostream &out,
        std::ostream &err) {
  if (args.empty()) {
    out << kHelp;
    return 0;
  }
  if (args.front() == "inspect") {
    try {
      return RunInspect(args.subspan(1), out, err);
    } catch (const std::exception &) {
      err << "perastage-cli: unexpected internal failure.\n";
      return 5;
    } catch (...) {
      err << "perastage-cli: unexpected internal failure.\n";
      return 5;
    }
  }
  if (args.size() != 1) {
    return UsageError(args[1], err);
  }

  const std::string_view argument = args.front();
  if (argument == "-h" || argument == "--help") {
    out << kHelp;
    return 0;
  }
  if (argument == "--version") {
    out << app::kName << " CLI " << PERASTAGE_CLI_VERSION << '\n';
    return 0;
  }
  return UsageError(argument, err);
}

} // namespace perastage::cli
