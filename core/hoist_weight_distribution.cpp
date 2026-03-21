#include "hoist_weight_distribution.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace HoistWeightDistribution {
namespace {
constexpr float kFiveKgStep = 5.0f;
constexpr float kSafetyMarginFactor = 0.05f;
constexpr float kCollinearTolerance = 0.001f;

using Point3 = std::array<float, 3>;

const std::vector<float> *GetLoadPercentagesForCount(size_t hoistCount) {
  static const std::unordered_map<size_t, std::vector<float>> kPercentages = {
      {2, {50.0f, 50.0f}},
      {3, {19.0f, 62.0f, 19.0f}},
      {4, {13.0f, 37.0f, 37.0f, 13.0f}},
      {5, {10.0f, 28.0f, 24.0f, 28.0f, 10.0f}},
      {6, {8.0f, 23.0f, 19.0f, 19.0f, 23.0f, 8.0f}},
      {7, {7.0f, 19.0f, 15.0f, 18.0f, 15.0f, 19.0f, 7.0f}},
      {8, {6.0f, 16.0f, 14.0f, 14.0f, 14.0f, 14.0f, 16.0f, 6.0f}},
  };
  auto it = kPercentages.find(hoistCount);
  return it == kPercentages.end() ? nullptr : &it->second;
}

std::string NormalizePosition(const std::string &positionName) {
  return positionName.empty() ? std::string("Unassigned") : positionName;
}

Point3 ToPoint(const Support &support) {
  return {support.transform.o[0], support.transform.o[1], support.transform.o[2]};
}

Point3 Subtract(const Point3 &a, const Point3 &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

float Norm(const Point3 &p) {
  return std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
}

Point3 Cross(const Point3 &a, const Point3 &b) {
  return {
      a[1] * b[2] - a[2] * b[1],
      a[2] * b[0] - a[0] * b[2],
      a[0] * b[1] - a[1] * b[0],
  };
}

float Dot(const Point3 &a, const Point3 &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool AreSupportsCollinear(const std::vector<Support *> &supports) {
  if (supports.size() < 3)
    return true;

  const Point3 origin = ToPoint(*supports.front());
  Point3 direction = {0.0f, 0.0f, 0.0f};
  bool foundDirection = false;
  for (size_t i = 1; i < supports.size(); ++i) {
    direction = Subtract(ToPoint(*supports[i]), origin);
    if (Norm(direction) > kCollinearTolerance) {
      foundDirection = true;
      break;
    }
  }

  if (!foundDirection)
    return true;

  for (size_t i = 1; i < supports.size(); ++i) {
    const Point3 offset = Subtract(ToPoint(*supports[i]), origin);
    const Point3 cross = Cross(offset, direction);
    if (Norm(cross) > kCollinearTolerance)
      return false;
  }
  return true;
}

std::vector<Support *> GetSupportsByPosition(MvrScene &scene,
                                             const std::string &positionName) {
  std::vector<Support *> supports;
  for (auto &[uuid, support] : scene.supports) {
    (void)uuid;
    if (NormalizePosition(support.positionName) == positionName)
      supports.push_back(&support);
  }
  return supports;
}

} // namespace

float RoundUpToNextFiveKg(float valueKg) {
  if (valueKg <= 0.0f)
    return 0.0f;
  return std::ceil(valueKg / kFiveKgStep) * kFiveKgStep;
}

float RoundRiggingTotalForHangPosition(float totalWeightKg) {
  if (totalWeightKg <= 0.0f)
    return 0.0f;
  const float withMargin = totalWeightKg + totalWeightKg * kSafetyMarginFactor;
  return RoundUpToNextFiveKg(withMargin);
}

std::unordered_map<std::string, bool>
BuildMissingWeightMapByHangPosition(const MvrScene &scene) {
  std::unordered_map<std::string, bool> hasMissingWeightByPosition;

  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    const std::string pos = NormalizePosition(fixture.positionName);
    if (fixture.weightKg <= 0.0f)
      hasMissingWeightByPosition[pos] = true;
    else
      hasMissingWeightByPosition.try_emplace(pos, false);
  }

  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    const std::string pos = NormalizePosition(truss.positionName);
    if (truss.weightKg <= 0.0f)
      hasMissingWeightByPosition[pos] = true;
    else
      hasMissingWeightByPosition.try_emplace(pos, false);
  }

  for (const auto &[uuid, support] : scene.supports) {
    (void)uuid;
    const std::string pos = NormalizePosition(support.positionName);
    if (support.weightKg <= 0.0f)
      hasMissingWeightByPosition[pos] = true;
    else
      hasMissingWeightByPosition.try_emplace(pos, false);
  }

  return hasMissingWeightByPosition;
}

std::unordered_map<std::string, float>
BuildRoundedRiggingTotalByHangPosition(const MvrScene &scene) {
  std::unordered_map<std::string, float> roundedTotals;

  auto accumulateWeight = [&](const std::string &positionName, float weightKg) {
    roundedTotals[NormalizePosition(positionName)] += weightKg;
  };

  for (const auto &[uuid, fixture] : scene.fixtures) {
    (void)uuid;
    accumulateWeight(fixture.positionName, fixture.weightKg);
  }
  for (const auto &[uuid, truss] : scene.trusses) {
    (void)uuid;
    accumulateWeight(truss.positionName, truss.weightKg);
  }
  for (const auto &[uuid, support] : scene.supports) {
    (void)uuid;
    accumulateWeight(support.positionName, support.weightKg);
  }

  for (auto &[positionName, total] : roundedTotals)
    total = RoundRiggingTotalForHangPosition(total);

  return roundedTotals;
}

void ApplyForImportedSupports(MvrScene &scene,
                              const std::vector<std::string> &importedSupportUuids,
                              const std::unordered_map<std::string, float>
                                  &roundedRiggingTotalByPosition) {
  std::unordered_set<std::string> targetPositions;
  targetPositions.reserve(importedSupportUuids.size());

  for (const std::string &uuid : importedSupportUuids) {
    auto it = scene.supports.find(uuid);
    if (it == scene.supports.end())
      continue;
    targetPositions.insert(NormalizePosition(it->second.positionName));
  }

  for (const std::string &positionName : targetPositions) {
    std::vector<Support *> supports = GetSupportsByPosition(scene, positionName);
    if (supports.empty())
      continue;

    auto roundedTotalIt = roundedRiggingTotalByPosition.find(positionName);
    if (roundedTotalIt == roundedRiggingTotalByPosition.end())
      continue;
    const float roundedTotal = roundedTotalIt->second;
    if (roundedTotal <= 0.0f)
      continue;

    const bool areCollinear = AreSupportsCollinear(supports);
    const std::vector<float> *percentages =
        areCollinear ? GetLoadPercentagesForCount(supports.size()) : nullptr;

    if (percentages) {
      Point3 origin = ToPoint(*supports.front());
      Point3 direction = {1.0f, 0.0f, 0.0f};
      for (size_t i = 1; i < supports.size(); ++i) {
        Point3 candidate = Subtract(ToPoint(*supports[i]), origin);
        if (Norm(candidate) > kCollinearTolerance) {
          direction = candidate;
          break;
        }
      }

      std::sort(supports.begin(), supports.end(), [&](const Support *a, const Support *b) {
        const float aProj = Dot(Subtract(ToPoint(*a), origin), direction);
        const float bProj = Dot(Subtract(ToPoint(*b), origin), direction);
        return aProj < bProj;
      });

      for (size_t i = 0; i < supports.size(); ++i) {
        const float share = roundedTotal * ((*percentages)[i] / 100.0f);
        supports[i]->weightKg = RoundUpToNextFiveKg(share);
        supports[i]->weightSource = "Manual";
      }
      continue;
    }

    const float split = roundedTotal / static_cast<float>(supports.size());
    const float roundedSplit = RoundUpToNextFiveKg(split);
    for (Support *support : supports) {
      support->weightKg = roundedSplit;
      support->weightSource = "Manual";
    }
  }
}

} // namespace HoistWeightDistribution
