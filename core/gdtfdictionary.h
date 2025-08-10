#pragma once

#include <string>
#include <optional>
#include <unordered_map>

namespace GdtfDictionary {
    struct Entry {
        std::string path;
        std::string mode;
    };

    // Loads the dictionary file into a map of type -> {gdtf absolute path, default mode}
    std::optional<std::unordered_map<std::string, Entry>> Load();
    // Saves the dictionary map back to disk
    void Save(const std::unordered_map<std::string, Entry>& dict);
    // Returns the stored entry for a given type if it exists and file exists.
    // If the file is missing, the entry is removed and std::nullopt returned.
    std::optional<Entry> Get(const std::string& type);
    // Copies the gdtf file into the fixtures library and updates the dictionary
    void Update(const std::string& type, const std::string& gdtfPath, const std::string& mode = {});
}
