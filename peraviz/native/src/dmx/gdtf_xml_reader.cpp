#include "dmx/gdtf_xml_reader.h"

#include <algorithm>
#include <cctype>
#include <memory>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace peraviz::dmx {

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string trim_ascii(std::string text) {
    const auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [&](unsigned char c) { return !is_space(c); }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [&](unsigned char c) { return !is_space(c); }).base(), text.end());
    return text;
}

std::string read_gdtf_description_xml(const std::string &gdtf_path) {
    wxFileInputStream input(wxString::FromUTF8(gdtf_path.c_str()));
    if (!input.IsOk()) {
        return {};
    }

    wxZipInputStream zip(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip.GetNextEntry())), entry) {
        const std::string file_name = lower_ascii(entry->GetName().ToUTF8().data());
        if (file_name != "description.xml" && file_name.find("/description.xml") == std::string::npos) {
            continue;
        }

        std::string xml;
        char buffer[4096];
        while (!zip.Eof()) {
            zip.Read(buffer, sizeof(buffer));
            const size_t n = zip.LastRead();
            if (n == 0) {
                break;
            }
            xml.append(buffer, n);
        }
        return xml;
    }

    return {};
}

std::vector<tinyxml2::XMLElement *> collect_elements_by_name(tinyxml2::XMLElement *root,
                                                             const std::string &name_lower) {
    std::vector<tinyxml2::XMLElement *> result;
    std::vector<tinyxml2::XMLElement *> stack;
    if (root) {
        stack.push_back(root);
    }

    while (!stack.empty()) {
        tinyxml2::XMLElement *node = stack.back();
        stack.pop_back();

        if (lower_ascii(node->Name()) == name_lower) {
            result.push_back(node);
        }

        for (tinyxml2::XMLElement *child = node->FirstChildElement(); child;
             child = child->NextSiblingElement()) {
            stack.push_back(child);
        }
    }
    return result;
}

std::vector<tinyxml2::XMLElement *> collect_direct_children_by_name(tinyxml2::XMLElement *root,
                                                                    const std::string &name_lower) {
    std::vector<tinyxml2::XMLElement *> result;
    if (!root) {
        return result;
    }
    for (tinyxml2::XMLElement *child = root->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (lower_ascii(child->Name()) == name_lower) {
            result.push_back(child);
        }
    }
    return result;
}

std::string read_attr_ci(tinyxml2::XMLElement *node,
                         const char *name_a,
                         const char *name_b) {
    if (!node) {
        return {};
    }
    const char *value = node->Attribute(name_a);
    if (!value) {
        value = node->Attribute(name_b);
    }
    return value ? trim_ascii(value) : std::string();
}

} // namespace peraviz::dmx
