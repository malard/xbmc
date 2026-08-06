/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/Geometry.h"

#include <span>
#include <vector>

namespace KODI::VIDEO::GEOMETRY
{

/*!
 * \brief One frame's reading, as produced by the detector at one sample point.
 */
struct GeometrySample
{
  CRectInt rect; //!< content rectangle in coded space
  float confidence{0.0f};
  bool degenerate{false}; //!< carries no reading at all - never combine as a narrow one
  double position{0.0}; //!< seconds into the title, retained for diagnostics
};

struct CombinerParams
{
  /*!
   * \brief Edges closer together than this are the same edge.
   *
   * The plan's jitter floor. A genuine letterbox boundary is in the same place in every
   * frame because it is baked into the coding, so this only has to absorb the odd line of
   * boundary ringing.
   */
  unsigned int tolerance{8};

  float minConfidence{0.05f}; //!< below this the detector has already said do not trust it

  /*!
   * \brief Share of samples a rival cluster needs before the title counts as varying.
   *
   * Judged on sample counts, never on confidence weight - see CombinedGeometry::varies.
   */
  float variesShare{0.20f};

  /*!
   * \brief Samples a rival cluster needs before it is a cluster at all.
   *
   * One sample is a point, and stationarity is a claim about repetition. Without this a
   * single unusual reading on a short scan declares a fixed-ratio title variable.
   */
  unsigned int minRivalSamples{2};
};

/*!
 * \brief A group of samples that agreed on the same rectangle.
 */
struct GeometryCluster
{
  CRectInt rect; //!< the median of the cluster's members, component-wise
  unsigned int samples{0};
  float weight{0.0f}; //!< summed confidence
};

struct CombinedGeometry
{
  CRectInt rect; //!< the answer; the coded rectangle when there is no reading

  /*!
   * \brief The title's geometry changes partway through.
   *
   * Decided on how *often* the geometry differs, not on how confident each reading was,
   * and it has to be. Confidence is systematically biased for this purpose: a "no bars"
   * reading scores top marks by construction, because there are no edges and so no soft
   * boundary to dock it for, while a letterboxed reading of the same title always scores
   * lower. Weighting by confidence therefore suppresses exactly the minority geometry this
   * flag exists to find. Measured on a variable-aspect title, it hid the second geometry
   * completely.
   */
  bool varies{false};

  bool hasReading{false}; //!< false when no sample survived; rect is then the coded frame
  float share{0.0f}; //!< dominant cluster's share of the surviving weight
  unsigned int usable{0};
  unsigned int discarded{0};

  //! \brief Every cluster, dominant first. Retained because a wrong cached answer can only
  //! be post-mortemed if the samples behind it survived.
  std::vector<GeometryCluster> clusters;
};

/*!
 * \brief Reduce per-sample readings to one rectangle, plus whether the title varies.
 *
 * Pure. Boundary stationarity: a real letterbox edge is in the same place in every frame of
 * the film, while a false edge from a dark scene wanders, because it is tracking content.
 * So cluster whole rectangles rather than combining edges independently - the answer is then
 * always a rectangle that actually occurred, rather than a mix of edges that never did - and
 * keep the dominant stationary cluster.
 *
 * Deliberately not a median, and emphatically not the widest. Widest is maximally
 * outlier-sensitive in the dangerous direction: one full-frame sample from an ident or a
 * credit card collapses a scope film to "no bars" for the lifetime of the cached value.
 *
 * \param coded the coded frame rectangle, returned when nothing survives - uncertainty must
 *              always resolve wider, never narrower
 */
CombinedGeometry CombineGeometrySamples(std::span<const GeometrySample> samples,
                                        const CRectInt& coded,
                                        const CombinerParams& params = {});

} // namespace KODI::VIDEO::GEOMETRY
