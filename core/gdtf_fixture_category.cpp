#include "gdtf_fixture_category.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {

std::string ToLowerCopy(const std::string &value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lowered;
}

std::string Trim(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

bool ContainsKeyword(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

std::vector<std::string> TokenizeWords(const std::string &text) {
  std::vector<std::string> tokens;
  std::string current;
  for (unsigned char c : text) {
    if (std::isalnum(c)) {
      current.push_back(static_cast<char>(std::tolower(c)));
    } else if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  }
  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

bool ContainsWord(const std::string &haystack, const char *needle) {
  const std::string target = ToLowerCopy(needle);
  const auto tokens = TokenizeWords(haystack);
  return std::find(tokens.begin(), tokens.end(), target) != tokens.end();
}

bool ReadDescriptionXmlFromGdtf(const std::string &gdtfPath, std::string &xmlOut) {
  wxFileInputStream input(gdtfPath);
  if (!input.IsOk())
    return false;

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    const std::string name = ToLowerCopy(entry->GetName().ToStdString());
    if (name != "description.xml")
      continue;

    std::ostringstream buffer;
    char chunk[4096];
    while (true) {
      zip.Read(chunk, sizeof(chunk));
      const size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      buffer.write(chunk, static_cast<std::streamsize>(bytes));
    }
    xmlOut = buffer.str();
    return !xmlOut.empty();
  }

  return false;
}

const tinyxml2::XMLElement *ResolveFixtureType(const tinyxml2::XMLDocument &doc) {
  const tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  return fixtureType;
}

struct Signals {
  bool hasPan = false;
  bool hasTilt = false;
  bool hasAnyAttributeDefinition = false;
  bool hasZoom = false;
  bool hasFocus = false;
  bool hasIris = false;
  bool hasFrost = false;
  bool hasGobo = false;
  bool hasAnimationWheel = false;
  bool hasFraming = false;
  bool hasStrobe = false;
  bool hasShutter = false;
  bool hasFog = false;
  bool hasHaze = false;
  bool hasBlower = false;
  bool hasLaser = false;
  bool hasVideo = false;
  bool hasDisplayOrMediaGeometry = false;
  bool hasPixelControl = false;
  bool hasSupport = false;
  bool supportLooksLikeHoist = false;
  bool hasBeamGeometry = false;
  bool lampTypeIsLed = false;
  float minBeamAngle = std::numeric_limits<float>::max();
  float maxBeamAngle = -1.0f;
  std::set<std::string> beamTypes;
  std::string normalizedName;
};

void ParseGeometrySignals(const tinyxml2::XMLElement *node, Signals &signals) {
  for (const tinyxml2::XMLElement *child = node->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    const std::string nodeName = ToLowerCopy(child->Name());

    if (nodeName.find("beam") != std::string::npos)
      signals.hasBeamGeometry = true;
    if (nodeName.find("laser") != std::string::npos)
      signals.hasLaser = true;
    if (nodeName.find("display") != std::string::npos ||
        nodeName.find("media") != std::string::npos) {
      signals.hasVideo = true;
      signals.hasDisplayOrMediaGeometry = true;
    }
    if (nodeName.find("support") != std::string::npos)
      signals.hasSupport = true;

    const char *beamTypeAttr = child->Attribute("BeamType");
    if (beamTypeAttr)
      signals.beamTypes.insert(ToLowerCopy(Trim(beamTypeAttr)));

    float beamAngle = 0.0f;
    if (child->QueryFloatAttribute("BeamAngle", &beamAngle) == tinyxml2::XML_SUCCESS &&
        beamAngle > 0.0f) {
      signals.minBeamAngle = std::min(signals.minBeamAngle, beamAngle);
      signals.maxBeamAngle = std::max(signals.maxBeamAngle, beamAngle);
    }

    float fieldAngle = 0.0f;
    if (child->QueryFloatAttribute("FieldAngle", &fieldAngle) == tinyxml2::XML_SUCCESS &&
        fieldAngle > 0.0f) {
      signals.minBeamAngle = std::min(signals.minBeamAngle, fieldAngle);
      signals.maxBeamAngle = std::max(signals.maxBeamAngle, fieldAngle);
    }

    if (const char *typeAttr = child->Attribute("Type")) {
      const std::string type = ToLowerCopy(Trim(typeAttr));
      if (type == "rope")
        signals.supportLooksLikeHoist = true;
    }

    ParseGeometrySignals(child, signals);
  }
}

void ApplyAttributeNameSignals(const char *attributeName, Signals &signals);

void ParseAttributeSignals(const tinyxml2::XMLElement *node, Signals &signals) {
  for (const tinyxml2::XMLElement *child = node->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    const std::string attributeName =
        std::string(child->Attribute("Name") ? child->Attribute("Name") : "") +
        " " +
        std::string(child->Attribute("Pretty") ? child->Attribute("Pretty") : "");
    ApplyAttributeNameSignals(attributeName.c_str(), signals);

    ParseAttributeSignals(child, signals);
  }
}

void ApplyAttributeNameSignals(const char *attributeName, Signals &signals) {
  if (!attributeName)
    return;

  signals.hasAnyAttributeDefinition = true;
  const std::string combined = ToLowerCopy(attributeName);

  if (ContainsKeyword(combined, "pan"))
    signals.hasPan = true;
  if (ContainsKeyword(combined, "tilt"))
    signals.hasTilt = true;
  if (ContainsKeyword(combined, "zoom"))
    signals.hasZoom = true;
  if (ContainsKeyword(combined, "focus"))
    signals.hasFocus = true;
  if (ContainsKeyword(combined, "iris"))
    signals.hasIris = true;
  if (ContainsKeyword(combined, "frost"))
    signals.hasFrost = true;
  if (ContainsKeyword(combined, "gobo"))
    signals.hasGobo = true;
  if (ContainsKeyword(combined, "animation"))
    signals.hasAnimationWheel = true;
  if (ContainsKeyword(combined, "blade") || ContainsKeyword(combined, "shaper") ||
      ContainsKeyword(combined, "keystone"))
    signals.hasFraming = true;
  if (ContainsKeyword(combined, "strobe"))
    signals.hasStrobe = true;
  if (ContainsKeyword(combined, "shutter"))
    signals.hasShutter = true;
  if (ContainsWord(combined, "fog"))
    signals.hasFog = true;
  if (ContainsWord(combined, "haze"))
    signals.hasHaze = true;
  if (ContainsWord(combined, "blower") || ContainsWord(combined, "fan"))
    signals.hasBlower = true;
  if (ContainsKeyword(combined, "video") || ContainsKeyword(combined, "inputsource") ||
      ContainsKeyword(combined, "fov"))
    signals.hasVideo = true;
  if (ContainsKeyword(combined, "pixel") || ContainsKeyword(combined, "ledzonemode"))
    signals.hasPixelControl = true;
}

void ParseDmxModeSignals(const tinyxml2::XMLElement *fixtureType, Signals &signals) {
  const tinyxml2::XMLElement *dmxModes = fixtureType->FirstChildElement("DMXModes");
  if (!dmxModes)
    return;

  for (const tinyxml2::XMLElement *dmxMode = dmxModes->FirstChildElement("DMXMode");
       dmxMode; dmxMode = dmxMode->NextSiblingElement("DMXMode")) {
    const tinyxml2::XMLElement *dmxChannels = dmxMode->FirstChildElement("DMXChannels");
    if (!dmxChannels)
      continue;

    for (const tinyxml2::XMLElement *dmxChannel = dmxChannels->FirstChildElement("DMXChannel");
         dmxChannel; dmxChannel = dmxChannel->NextSiblingElement("DMXChannel")) {
      for (const tinyxml2::XMLElement *logical =
               dmxChannel->FirstChildElement("LogicalChannel");
           logical; logical = logical->NextSiblingElement("LogicalChannel")) {
        ApplyAttributeNameSignals(logical->Attribute("Attribute"), signals);
        for (const tinyxml2::XMLElement *channelFunction =
                 logical->FirstChildElement("ChannelFunction");
             channelFunction;
             channelFunction = channelFunction->NextSiblingElement("ChannelFunction")) {
          ApplyAttributeNameSignals(channelFunction->Attribute("Attribute"), signals);
        }
      }
    }
  }
}

Signals ExtractSignals(const std::string &xml) {
  Signals signals;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
    return signals;

  const tinyxml2::XMLElement *fixtureType = ResolveFixtureType(doc);
  if (!fixtureType)
    return signals;

  const char *name = fixtureType->Attribute("Name");
  const char *shortName = fixtureType->Attribute("ShortName");
  const char *longName = fixtureType->Attribute("LongName");
  signals.normalizedName = ToLowerCopy(Trim(std::string(name ? name : "") + " " +
                                            std::string(shortName ? shortName : "") + " " +
                                            std::string(longName ? longName : "")));

  if (const tinyxml2::XMLElement *physical = fixtureType->FirstChildElement("PhysicalDescriptions")) {
    if (const tinyxml2::XMLElement *emitters = physical->FirstChildElement("Emitters")) {
      for (const tinyxml2::XMLElement *emitter = emitters->FirstChildElement("Emitter"); emitter;
           emitter = emitter->NextSiblingElement("Emitter")) {
        if (const char *lampType = emitter->Attribute("LampType")) {
          if (ToLowerCopy(Trim(lampType)) == "led")
            signals.lampTypeIsLed = true;
        }
      }
    }
  }

  if (const tinyxml2::XMLElement *geometries = fixtureType->FirstChildElement("Geometries"))
    ParseGeometrySignals(geometries, signals);

  if (const tinyxml2::XMLElement *defs = fixtureType->FirstChildElement("AttributeDefinitions"))
    ParseAttributeSignals(defs, signals);
  ParseDmxModeSignals(fixtureType, signals);

  return signals;
}

bool HasNameKeyword(const std::string &name, std::initializer_list<const char *> keys) {
  for (const char *key : keys) {
    const std::string pattern = ToLowerCopy(key);
    if (pattern.find(' ') != std::string::npos) {
      if (ContainsKeyword(name, pattern.c_str()))
        return true;
    } else if (ContainsWord(name, pattern.c_str())) {
      return true;
    }
  }
  return false;
}

GdtfFixtureCategory::InferenceResult
InferFromNameHints(const std::string &normalizedName) {
  if (normalizedName.empty())
    return {std::string(), "no name hints"};

  if (HasNameKeyword(
          normalizedName,
          {"hazer", "haze", "smoke", "humo", "fan", "turbina", "ventilador",
           "turbine", "fog"})) {
    return {GdtfFixtureCategory::kSmoke, "name hint smoke"};
  }

  if (HasNameKeyword(normalizedName, {"strobe", "estrobo", "flash"}))
    return {GdtfFixtureCategory::kStrobe, "name hint strobe"};

  if (HasNameKeyword(normalizedName, {"hybrid", "hibrido", "híbrido"}))
    return {GdtfFixtureCategory::kHybrid, "name hint hybrid keyword"};

  const bool beam = HasNameKeyword(normalizedName, {"beam"});
  const bool spot = HasNameKeyword(normalizedName, {"spot", "profile"});
  const bool wash = HasNameKeyword(normalizedName, {"wash"});
  if (wash)
    return {GdtfFixtureCategory::kWash, "name hint wash"};
  if (spot)
    return {GdtfFixtureCategory::kSpot, "name hint spot/profile"};
  if (beam)
    return {GdtfFixtureCategory::kBeam, "name hint beam"};

  if (HasNameKeyword(
          normalizedName,
          {"fresnel", "fresnell", "pc", "par", "par64", "convencional"})) {
    return {GdtfFixtureCategory::kConventional, "name hint conventional"};
  }

  if (HasNameKeyword(normalizedName, {"blinder", "cegadora", "molefay"}))
    return {GdtfFixtureCategory::kBlinder, "name hint blinder"};

  return {std::string(), "no name hints"};
}

} // namespace

namespace GdtfFixtureCategory {

bool IsValidCategory(const std::string &category) {
  const std::string normalized = NormalizeCategory(category);
  return !normalized.empty();
}

std::string NormalizeCategory(const std::string &category) {
  const std::string lowered = ToLowerCopy(Trim(category));
  if (lowered.empty())
    return {};
  if (lowered == "unknown")
    return kUnknown;
  if (lowered == "hoist")
    return kHoist;
  if (lowered == "smoke" || lowered == "haze" || lowered == "fog")
    return kSmoke;
  if (lowered == "laser")
    return kLaser;
  if (lowered == "video" || lowered == "display" || lowered == "media")
    return kVideo;
  if (lowered == "fx")
    return kFx;
  if (lowered == "strobe")
    return kStrobe;
  if (lowered == "led")
    return kLed;
  if (lowered == "blinder" || lowered == "cegadora" || lowered == "molefay")
    return kBlinder;
  if (lowered == "conventional")
    return kConventional;
  if (lowered == "wash")
    return kWash;
  if (lowered == "spot")
    return kSpot;
  if (lowered == "beam")
    return kBeam;
  if (lowered == "hybrid")
    return kHybrid;
  return {};
}

InferenceResult InferFromName(const std::string &fixtureName) {
  const std::string normalizedName = ToLowerCopy(Trim(fixtureName));
  const auto inferred = InferFromNameHints(normalizedName);
  if (!inferred.category.empty())
    return inferred;
  return {kUnknown, "name hints unavailable"};
}

InferenceResult InferFromGdtf(const std::string &gdtfPath) {
  std::string xml;
  if (!ReadDescriptionXmlFromGdtf(gdtfPath, xml))
    return {kUnknown, "missing description.xml"};

  Signals s = ExtractSignals(xml);
  if (const auto fromName = InferFromNameHints(s.normalizedName);
      !fromName.category.empty()) {
    return fromName;
  }

  if (!s.hasAnyAttributeDefinition)
    return {kUnknown, "empty attribute definitions"};

  const bool hasMovement = s.hasPan && s.hasTilt;
  const bool narrowBeam = s.minBeamAngle > 0.0f && s.minBeamAngle <= 8.0f;
  const bool wideBeam = s.maxBeamAngle >= 20.0f;
  const bool wideZoomRange = s.minBeamAngle > 0.0f && s.maxBeamAngle > 0.0f &&
                             (s.maxBeamAngle - s.minBeamAngle) >= 15.0f;
  const bool hasSpotOptics = s.hasGobo || s.hasAnimationWheel || s.hasIris ||
                             s.hasFocus || s.hasFraming;
  const bool washBeamType = s.beamTypes.contains("wash") || s.beamTypes.contains("glow");
  const bool conventionalBeamType =
      s.beamTypes.contains("pc") || s.beamTypes.contains("fresnel");
  const bool spotBeamType = s.beamTypes.contains("spot") || s.beamTypes.contains("rectangle");
  const bool canDoWash = s.hasFrost || washBeamType || wideBeam;

  if (s.hasSupport && s.supportLooksLikeHoist && !s.hasBeamGeometry && !s.hasLaser &&
      !s.hasVideo && !s.hasFog && !s.hasHaze) {
    return {kHoist, "support geometry"};
  }

  if (!hasMovement &&
      (s.hasFog || s.hasHaze || s.hasBlower ||
       HasNameKeyword(s.normalizedName, {"haze", "hazer", "fog", "smoke"}))) {
    return {kSmoke, "fog/haze signals"};
  }

  if (s.hasLaser || HasNameKeyword(s.normalizedName, {"laser"}))
    return {kLaser, "laser signals"};

  if ((s.hasDisplayOrMediaGeometry ||
       (!hasMovement && s.hasVideo && !s.hasBeamGeometry)) &&
      !HasNameKeyword(s.normalizedName, {"wash", "spot", "beam", "profile"})) {
    return {kVideo, "video signals"};
  }

  if (!s.hasBeamGeometry && !hasMovement &&
      HasNameKeyword(s.normalizedName, {"pyro", "confetti", "snow", "bubble", "flame"})) {
    return {kFx, "fx keyword"};
  }

  if (!hasMovement) {
    if (HasNameKeyword(s.normalizedName, {"blinder", "cegadora", "molefay"}))
      return {kBlinder, "blinder keyword fallback"};

    const bool looksLikeLedUnit =
        HasNameKeyword(s.normalizedName, {"pixel", "bar", "strip", "panel", "par", "blinder"});
    const bool looksLikeStrobeName =
        HasNameKeyword(s.normalizedName, {"strobe", "flash", "q-7", "q7"});

    if ((s.hasStrobe || (s.hasShutter && looksLikeStrobeName) || looksLikeStrobeName) &&
        !s.hasGobo && !s.hasIris && !s.hasFraming && !s.hasFocus && !looksLikeLedUnit) {
      return {kStrobe, "static strobe profile"};
    }
    if (s.hasPixelControl || (s.lampTypeIsLed && looksLikeLedUnit)) {
      return {kLed, "pixel/led profile"};
    }
    if (conventionalBeamType ||
        (s.hasBeamGeometry && !s.hasVideo && !s.hasLaser && !s.hasFog &&
         !s.hasHaze && !s.hasGobo && !s.hasAnimationWheel && !s.hasFraming &&
         !s.hasIris && !s.hasFocus && !s.hasZoom)) {
      return {kConventional, "static conventional profile"};
    }
    return {kUnknown, "static ambiguous"};
  }

  const bool strongWashCapability = s.hasFrost || washBeamType || wideZoomRange ||
                                  (narrowBeam && wideBeam);

  if (!s.hasGobo)
    return {kWash, "moving without gobo"};

  if (HasNameKeyword(s.normalizedName, {"profile", "followspot"}) && hasSpotOptics &&
      !strongWashCapability)
    return {kSpot, "profile optics"};

  if (hasSpotOptics && strongWashCapability)
    return {kHybrid, "hybrid profile"};

  if (narrowBeam && !canDoWash)
    return {kBeam, "narrow beam"};

  if (hasSpotOptics || spotBeamType)
    return {kSpot, "spot optics"};

  if (canDoWash)
    return {kWash, "wash optics"};

  if (s.hasPixelControl || (s.lampTypeIsLed && HasNameKeyword(s.normalizedName, {"bar", "panel", "pixel"})))
    return {kLed, "moving led fallback"};

  if (hasSpotOptics)
    return {kSpot, "moving fallback spot"};

  if (hasMovement)
    return {kWash, "moving fallback wash"};

  return {kUnknown, "ambiguous"};
}

} // namespace GdtfFixtureCategory
