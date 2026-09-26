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
bool CheckCase(const std::vector<std::string_view>& args,
               const ExpectedResult& expected) {
    for (int repetition = 0; repetition < 2; ++repetition) {
        std::ostringstream out;
        std::ostringstream err;
        const int exitCode = perastage::cli::Run(args, out, err);
        if (exitCode != expected.exitCode || out.str() != expected.output ||
            err.str() != expected.error) {
            std::cerr << "CLI runner case failed for repetition " << repetition << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

// Verifies all CLI-200 grammar and deterministic-output cases.
int main() {
    const std::string help =
        "Usage: perastage-cli [--help | --version]\n\n"
        "Options:\n"
        "  -h, --help  Show this help and exit.\n"
        "  --version   Show the CLI version and exit.\n";
    const auto usageError = [](std::string_view argument) {
        return "perastage-cli: unexpected argument: " + std::string(argument) +
               "\nTry 'perastage-cli --help' for usage.\n";
    };

    bool passed = true;
    passed &= CheckCase({}, {0, help, ""});
    passed &= CheckCase({"-h"}, {0, help, ""});
    passed &= CheckCase({"--help"}, {0, help, ""});
    passed &= CheckCase({"--version"},
                        {0, "Perastage CLI " PERASTAGE_CLI_TEST_VERSION "\n", ""});
    passed &= CheckCase({"--unknown"}, {2, "", usageError("--unknown")});
    passed &= CheckCase({"file.mvr"}, {2, "", usageError("file.mvr")});
    passed &= CheckCase({"--help", "extra"}, {2, "", usageError("extra")});
    passed &= CheckCase({"--version", "extra"}, {2, "", usageError("extra")});
    return passed ? 0 : 1;
}
