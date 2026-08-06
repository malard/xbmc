/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FrameSampling.h"

#include <algorithm>
#include <cmath>

namespace KODI::VIDEO::GEOMETRY
{

std::vector<double> SampleOffsets(double durationSeconds, const SamplingParams& params)
{
  std::vector<double> offsets;
  if (durationSeconds <= 0.0 || params.points == 0)
    return offsets;

  const double start = std::clamp(params.windowStart, 0.0, 1.0);
  const double end = std::clamp(params.windowEnd, start, 1.0);

  offsets.reserve(params.points);
  if (params.points == 1)
  {
    offsets.push_back(durationSeconds * (start + end) / 2.0);
    return offsets;
  }

  for (unsigned int i = 0; i < params.points; ++i)
  {
    const double fraction =
        start + (end - start) * static_cast<double>(i) / static_cast<double>(params.points - 1);
    offsets.push_back(durationSeconds * fraction);
  }
  return offsets;
}

bool ShouldEscalate(size_t clusters,
                    unsigned int usable,
                    unsigned int discarded,
                    const SamplingParams& params)
{
  if (params.escalatedPoints <= params.points)
    return false;

  // More than one geometry already found: densify to establish whether that is the title
  // varying or a single odd reading, which a short pass cannot tell apart.
  if (clusters > 1)
    return true;

  const unsigned int total = usable + discarded;
  if (total == 0)
    return true; // nothing survived at all, so nothing has been measured

  return static_cast<float>(discarded) / static_cast<float>(total) > params.escalateDiscardShare;
}

std::vector<double> UnsampledOffsets(const std::vector<double>& candidates,
                                     const std::vector<double>& sampled,
                                     double minSeparation)
{
  std::vector<double> wanted;
  for (const double candidate : candidates)
  {
    const bool covered = std::any_of(sampled.begin(), sampled.end(), [&](double taken)
                                     { return std::abs(taken - candidate) < minSeparation; });
    if (!covered)
      wanted.push_back(candidate);
  }
  return wanted;
}

CRectInt StereoViewRect(const std::string& stereoMode, unsigned int width, unsigned int height)
{
  const int w = static_cast<int>(width);
  const int h = static_cast<int>(height);

  if (stereoMode == "left_right" || stereoMode == "right_left")
    return {0, 0, w / 2, h};

  if (stereoMode == "top_bottom" || stereoMode == "bottom_top")
    return {0, 0, w, h / 2};

  // Anything else - mono, empty, or a packing we do not recognise - is left whole. An
  // unrecognised mode must not be guessed at: halving a mono frame would report a
  // pillarbox that is not there, and that is the direction that masks real picture.
  return {};
}

} // namespace KODI::VIDEO::GEOMETRY
