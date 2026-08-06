/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/Geometry.h"

#include <string>
#include <vector>

namespace KODI::VIDEO::GEOMETRY
{

struct SamplingParams
{
  unsigned int points{9}; //!< sample positions across the title

  /*!
   * \brief First sample position, as a fraction of the runtime.
   *
   * Never zero. Opening frames are studio logos and idents whose geometry is not the
   * title's, and reading one is how a scope film gets recorded as full frame.
   */
  double windowStart{0.10};

  /*!
   * \brief Last sample position, as a fraction of the runtime.
   *
   * Short of the end for the same reason it is short of the start: a ten-minute credit
   * roll on a hundred-minute film puts a 90% sample squarely in credits, which are
   * frequently full-container on scope titles.
   */
  double windowEnd{0.85};

  /*!
   * \brief Pictures to decode at each position.
   *
   * One by default. More than one does not vote - agreement between adjacent frames is the
   * wrong quantity, because correlated errors agree - but it does measure whether the
   * content at this position is stable, which is a reason to discard the position outright.
   * Nearly free when used, because the cost of a position is the seek and not the decode.
   */
  unsigned int picturesPerPoint{1};

  /*!
   * \brief Points to use on a second pass, when the first pass looks inconclusive.
   *
   * Deciding that a title varies is sampling-density limited, and measurably so: on a
   * hybrid IMAX title whose geometry changes repeatedly, nine points landed on no
   * full-frame section at all and reported a fixed ratio, while twenty-seven found both
   * geometries. Paying that cost on every title would triple a library sweep, so it is
   * paid only where the first pass gives a reason to. Zero disables escalation.
   */
  unsigned int escalatedPoints{27};

  /*!
   * \brief Discard rate above which the first pass is treated as inconclusive.
   *
   * A title whose samples are mostly being thrown away is not a title we have measured. On
   * the corpus this separates cleanly: 0% and 11% discarded on two fixed-ratio titles
   * against 44% on the varying one.
   */
  float escalateDiscardShare{0.34f};
};

/*!
 * \brief Is a first sampling pass inconclusive enough to be worth densifying?
 *
 * \param clusters distinct geometries the first pass found
 * \param usable samples that survived
 * \param discarded samples thrown away as degenerate or untrustworthy
 */
bool ShouldEscalate(size_t clusters,
                    unsigned int usable,
                    unsigned int discarded,
                    const SamplingParams& params = {});

/*!
 * \brief Offsets from \p candidates that are not already covered by \p sampled.
 *
 * Escalation reuses the first pass rather than repeating it; positions within a second of
 * one already measured would only distort the cluster counts that decide `varies`.
 */
std::vector<double> UnsampledOffsets(const std::vector<double>& candidates,
                                     const std::vector<double>& sampled,
                                     double minSeparation = 1.0);

/*!
 * \brief Positions, in seconds, at which to sample a title of the given duration.
 *
 * Spread across the window, endpoints included within it but never at t=0 or the very end.
 * Returns empty for a duration or point count that cannot be sampled.
 */
std::vector<double> SampleOffsets(double durationSeconds, const SamplingParams& params = {});

/*!
 * \brief The region of a packed stereoscopic frame holding a single view.
 *
 * An edge walk over a side-by-side or top-and-bottom frame returns nonsense: for
 * top-and-bottom the inter-view boundary is invisible to it and the rectangle spans both
 * views. Both views carry the same geometry, so either will do.
 *
 * \return the view's rectangle, or an empty rectangle for mono content, meaning the whole
 *         frame
 */
CRectInt StereoViewRect(const std::string& stereoMode, unsigned int width, unsigned int height);

} // namespace KODI::VIDEO::GEOMETRY
