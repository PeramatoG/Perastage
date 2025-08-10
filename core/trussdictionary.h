#pragma once

#include <string>
#include <optional>
#include <unordered_map>

namespace TrussDictionary {
    // Loads the dictionary file into a map of model -> absolute path
    std::optional<std::unordered_map<std::string, std::string>> Load();
    // Saves the dictionary map back to disk
    void Save(const std::unordered_map<std::string, std::string>& dict);
    // Returns stored path for a model if exists and file exists; removes missing entries
    std::optional<std::string> Get(const std::string& model);
    // Copies model file into trusses library and updates dictionary
    void Update(const std::string& model, const std::string& modelPath);
}
