/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ContentGeometryCombiner.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace KODI::VIDEO::GEOMETRY
{

namespace
{

//! \brief A cluster while it is still being built, keeping its members for re-centring.
struct WorkingCluster
{
  CRectInt centre;
  float weight{0.0f};
  std::vector<CRectInt> members;
};

std::array<int, 4> Edges(const CRectInt& rect)
{
  return {rect.x1, rect.y1, rect.x2, rect.y2};
}

//! \brief Chebyshev distance: the furthest any single edge has moved.
int Distance(const CRectInt& a, const CRectInt& b)
{
  const std::array<int, 4> lhs = Edges(a);
  const std::array<int, 4> rhs = Edges(b);
  int furthest = 0;
  for (size_t i = 0; i < lhs.size(); ++i)
    furthest = std::max(furthest, std::abs(lhs[i] - rhs[i]));
  return furthest;
}

//! \brief Component-wise median, so one early member cannot define where a cluster sits.
CRectInt MedianRect(const std::vector<CRectInt>& members)
{
  std::array<int, 4> centre{};
  std::vector<int> values;
  values.reserve(members.size());

  for (size_t component = 0; component < centre.size(); ++component)
  {
    values.clear();
    for (const CRectInt& member : members)
      values.push_back(Edges(member)[component]);
    std::sort(values.begin(), values.end());
    centre[component] = values[values.size() / 2];
  }

  return {centre[0], centre[1], centre[2], centre[3]};
}

} // unnamed namespace

CombinedGeometry CombineGeometrySamples(std::span<const GeometrySample> samples,
                                        const CRectInt& coded,
                                        const CombinerParams& params)
{
  CombinedGeometry result;
  result.rect = coded;

  std::vector<WorkingCluster> clusters;

  for (const GeometrySample& sample : samples)
  {
    // A degenerate frame - a cut to black, where every line is dark and flat - is not a
    // very narrow reading. It is no reading. Combining it as narrow is the single most
    // likely way a mask ends up closing onto picture.
    if (sample.degenerate || sample.confidence < params.minConfidence)
    {
      ++result.discarded;
      continue;
    }

    ++result.usable;

    auto match = std::find_if(
        clusters.begin(), clusters.end(), [&](const WorkingCluster& cluster)
        { return Distance(cluster.centre, sample.rect) <= static_cast<int>(params.tolerance); });

    if (match == clusters.end())
      clusters.push_back({sample.rect, sample.confidence, {sample.rect}});
    else
    {
      match->weight += sample.confidence;
      match->members.push_back(sample.rect);
    }
  }

  if (clusters.empty())
    return result; // no reading; rect stays at the coded frame, which is never narrower

  for (WorkingCluster& cluster : clusters)
    cluster.centre = MedianRect(cluster.members);

  std::sort(clusters.begin(), clusters.end(),
            [](const WorkingCluster& a, const WorkingCluster& b)
            {
              if (a.weight != b.weight)
                return a.weight > b.weight;
              return a.members.size() > b.members.size();
            });

  float totalWeight = 0.0f;
  for (const WorkingCluster& cluster : clusters)
  {
    totalWeight += cluster.weight;
    result.clusters.push_back(
        {cluster.centre, static_cast<unsigned int>(cluster.members.size()), cluster.weight});
  }

  result.hasReading = true;
  result.rect = result.clusters.front().rect;
  result.share = totalWeight > 0.0f ? result.clusters.front().weight / totalWeight : 0.0f;

  // Counts, not weight - see CombinedGeometry::varies for why this must not be weighted.
  const unsigned int counted = result.usable;
  result.varies =
      std::any_of(result.clusters.begin() + 1, result.clusters.end(),
                  [&](const GeometryCluster& cluster)
                  {
                    return cluster.samples >= params.minRivalSamples &&
                           static_cast<float>(cluster.samples) / static_cast<float>(counted) >=
                               params.variesShare;
                  });

  return result;
}

} // namespace KODI::VIDEO::GEOMETRY
