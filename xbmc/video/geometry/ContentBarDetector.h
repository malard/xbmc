/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/Geometry.h"

#include <cstdint>

namespace KODI::VIDEO::GEOMETRY
{

/*!
 * \brief Chroma plane subsampling of the frame handed to the detector.
 */
enum class ChromaSubsampling
{
  YUV420,
  YUV422,
  YUV444,
};

/*!
 * \brief Signal range of the luma plane.
 *
 * Must be taken from the stream's color_range, never assumed. Unspecified is resolved to
 * Limited, which is both the overwhelmingly common truth for YUV and the direction that
 * still works when it is wrong: a limited-range threshold applied to full-range content is
 * merely more generous, whereas a full-range threshold applied to limited-range content
 * sits below studio black and reports no bars at all.
 */
enum class ColorRange
{
  Unspecified,
  Limited,
  Full,
};

/*!
 * \brief A non-owning view of one decoded plane.
 *
 * Samples deeper than 8 bits are little-endian, low-aligned uint16_t, as produced by
 * FFmpeg's YUV*P10/P12/P16 formats.
 */
struct PlaneRef
{
  const uint8_t* data{nullptr};
  int strideBytes{0};
};

/*!
 * \brief A non-owning view of one decoded frame.
 *
 * Deliberately not VideoPicture: that type reaches into libavcodec, the video buffer pool
 * and CProcessInfo, none of which the detector may depend on. Building a FrameRef from a
 * VideoPicture is the caller's job.
 */
struct FrameRef
{
  unsigned int width{0}; //!< full coded luma width
  unsigned int height{0}; //!< full coded luma height
  unsigned int bitDepth{8}; //!< bits per component, 8..16
  ChromaSubsampling subsampling{ChromaSubsampling::YUV420};
  ColorRange range{ColorRange::Unspecified};

  PlaneRef y;
  PlaneRef u; //!< optional; when either chroma plane is absent the neutrality test is skipped
  PlaneRef v;

  /*!
   * \brief Region to analyse, in full-frame coded coordinates. Empty means the whole frame.
   *
   * This is how a stereoscopic frame is handled: the caller, which is the only party that
   * can see stereo_mode, passes the left or top view. The returned rectangle is still
   * expressed in full-frame coordinates.
   */
  CRectInt roi;
};

/*!
 * \brief Derived constants, exposed so tests can pin them.
 *
 * Every value is a derivation rather than a tuning knob, and production code is expected
 * to accept the defaults. Code-value quantities are 8-bit equivalents and are scaled
 * linearly with bit depth internally.
 */
struct DetectorParams
{
  unsigned int margin{12}; //!< added to reference black to give the bar threshold
  unsigned int flatBand{12}; //!< permitted interquartile range within a bar line
  unsigned int chromaTolerance{16}; //!< permitted deviation of median Cb/Cr from neutral
  unsigned int minRunLength{4}; //!< above-threshold pixels needed to count as an overlay run
  unsigned int minContentExtent{16}; //!< below this many lines the reading is degenerate

  /*!
   * \brief Fewer lines than this on an edge is not a bar, and is discarded.
   *
   * Not a policy about what to do with a thin bar - a statement about what a bar is. Real
   * letterboxing is tens to hundreds of lines and encoders align to even or mod-8 sizes,
   * so a one or two line finding is a boundary artefact. Measured on 135 samples of
   * already-cropped content (2026-08-06), every spurious sub-floor finding was exactly one
   * line. Keeping them would mean the acceptance criterion "must not invent bars on
   * content already coded at its true aspect" is met only approximately, and every
   * consumer would have to re-filter. Matches the plan's jitter floor.
   */
  unsigned int edgeThicknessFloor{8};
  float overlayMaxExtent{0.5f}; //!< an overlay may not span more of the line than this
  float overlaySuspectExtent{0.8f}; //!< wider than overlayMaxExtent but still bounded
  float edgeStepReference{0.15f}; //!< luma step, as a fraction of full scale, scoring 1.0

  //! \brief Advisory penalties. Each scales confidence; none may move an edge. There is
  //! deliberately no penalty for overlaySuspected - see DetectionResult::overlaySuspected.
  float symmetryPenalty{0.5f}; //!< most confidence fully asymmetric bars may cost
  float rangeAssumedPenalty{0.8f}; //!< applied when color_range had to be guessed
  float chromaAbsentPenalty{0.9f}; //!< applied when the neutrality test could not run
};

/*!
 * \brief Per-edge boundary sharpness.
 *
 * A genuine letterbox boundary is a large luma step sustained across most of the boundary.
 * A dark scene's false boundary is a small step over a fraction of it. Both numbers are
 * reported; deciding what a soft edge is allowed to do is the caller's policy, not this
 * function's.
 */
struct EdgeMetrics
{
  bool measured{false}; //!< false when this edge has no bar at all, so no boundary exists
  unsigned int thickness{0}; //!< bar lines removed on this edge
  float step{0.0f}; //!< luma step across the boundary as a fraction of full scale
  float coverage{0.0f}; //!< fraction of the boundary showing that step
};

/*!
 * \brief The detector's per-frame answer.
 */
struct DetectionResult
{
  CRectInt rect; //!< content rectangle in full-frame coded coordinates

  /*!
   * \brief No picture was found, so this frame carries no reading at all.
   *
   * Set on a cut to black, where every line is dark and flat. Such a frame is not a very
   * narrow reading and must never be combined as one. rect is deliberately left at the
   * full analysed region so that a caller which ignores this flag still fails wide.
   */
  bool degenerate{false};

  float confidence{0.0f}; //!< [0,1], geometric mean of the factors below

  EdgeMetrics top;
  EdgeMetrics bottom;
  EdgeMetrics left;
  EdgeMetrics right;

  float separation{0.0f}; //!< threshold headroom on the worst bar line
  float flatness{0.0f}; //!< dispersion headroom on the worst bar line
  float symmetry{0.0f}; //!< top/bottom and left/right bar agreement, advisory only

  unsigned int overlayLines{0}; //!< bar lines admitted by the overlay rule
  bool overlaySuspected{false}; //!< a bounded run too wide to admit stopped the walk
  bool rangeAssumed{false}; //!< color_range was Unspecified and Limited was assumed
  bool chromaTested{false}; //!< both chroma planes were present
  bool subFloorEdgesIgnored{false}; //!< an edge thinner than the floor was found and dropped

  unsigned int thresholdUsed{0}; //!< bar threshold in native code values
  unsigned int blackFloorObserved{0}; //!< darkest bar level seen, in native code values
};

/*!
 * \brief Find the content rectangle of a single decoded frame.
 *
 * Pure: no I/O, no services, no state carried between calls. A line is a bar line when it
 * is both dark and flat, judged by robust statistics rather than by a maximum deviation,
 * and when its chroma is achromatic. Rows are resolved first; columns are then resolved
 * over the rows that survived, because on a letterboxed frame every column contains the
 * bars and an independent column walk would consume the whole frame.
 *
 * Temporal policy - what a low-confidence or degenerate sample is permitted to do - is
 * explicitly not decided here.
 */
DetectionResult DetectContentRect(const FrameRef& frame, const DetectorParams& params = {});

} // namespace KODI::VIDEO::GEOMETRY
