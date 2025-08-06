#pragma once

#include <string>
#include <optional>
#include <unordered_map>

namespace GdtfDictionary {
    // Loads the dictionary file into a map of type -> gdtf absolute path
    std::unordered_map<std::string, std::string> Load();
    // Saves the dictionary map back to disk
    void Save(const std::unordered_map<std::string, std::string>& dict);
    // Returns the stored gdtf path for a given type if it exists and file exists.
    // If the file is missing, the entry is removed and std::nullopt returned.
    std::optional<std::string> Get(const std::string& type);
    // Copies the gdtf file into the fixtures library and updates the dictionary
    void Update(const std::string& type, const std::string& gdtfPath);
}
