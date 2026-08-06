/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ContentBarDetector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>

namespace KODI::VIDEO::GEOMETRY
{

namespace
{

constexpr unsigned int BUCKET_COUNT = 256;

/*!
 * \brief A luma or chroma histogram, always in 8-bit equivalent buckets.
 *
 * Working in 8-bit buckets whatever the source depth is what keeps the detector honest:
 * there is no path along which a constant meets a plane of a different depth, because
 * every threshold is expressed in the same units the histogram is. The quantisation costs
 * less than one 8-bit code value of threshold precision.
 */
class CHistogram
{
public:
  void Reset()
  {
    m_bins.fill(0);
    m_total = 0;
  }

  void Add(unsigned int bucket)
  {
    ++m_bins[bucket];
    ++m_total;
  }

  uint32_t Total() const { return m_total; }

  unsigned int Percentile(double quantile) const
  {
    if (m_total == 0)
      return 0;

    const uint32_t target = static_cast<uint32_t>(quantile * m_total);
    uint32_t accumulated = 0;
    for (unsigned int bucket = 0; bucket < BUCKET_COUNT; ++bucket)
    {
      accumulated += m_bins[bucket];
      if (accumulated > target)
        return bucket;
    }
    return BUCKET_COUNT - 1;
  }

private:
  std::array<uint32_t, BUCKET_COUNT> m_bins{};
  uint32_t m_total{0};
};

/*!
 * \brief A strided run of samples through one plane, in 8-bit equivalent buckets.
 *
 * Rows and columns differ only in their origin and step, so both walks share one
 * implementation. The template is instantiated once for 8-bit planes and once for deeper
 * ones, which keeps the per-sample path free of a bit-depth branch.
 */
template<typename T>
class CSampleLine
{
public:
  CSampleLine() = default;

  CSampleLine(const PlaneRef& plane,
              unsigned int shift,
              std::ptrdiff_t origin,
              std::ptrdiff_t step,
              unsigned int count)
    : m_base(plane.data),
      m_shift(shift),
      m_origin(origin),
      m_step(step),
      m_count(count)
  {
  }

  unsigned int Count() const { return m_count; }

  unsigned int operator[](unsigned int index) const
  {
    const T* sample =
        reinterpret_cast<const T*>(m_base + m_origin + static_cast<std::ptrdiff_t>(index) * m_step);
    return static_cast<unsigned int>(*sample) >> m_shift;
  }

private:
  const uint8_t* m_base{nullptr};
  unsigned int m_shift{0};
  std::ptrdiff_t m_origin{0};
  std::ptrdiff_t m_step{0};
  unsigned int m_count{0};
};

//! \brief Identifies one line of the frame: a row, or a column, and the span walked along it.
struct LineSpec
{
  bool row{true};
  unsigned int index{0}; //!< row number, or column number
  unsigned int begin{0}; //!< inclusive start along the line
  unsigned int end{0}; //!< exclusive end along the line
};

enum class LineClass
{
  Bar,
  Overlay,
  Picture,
};

struct LineOutcome
{
  LineClass classification{LineClass::Picture};
  unsigned int high{0}; //!< p99.5 of the bar portion
  unsigned int low{0}; //!< p0.5 of the bar portion
  unsigned int spread{0}; //!< interquartile range of the bar portion
  bool suspectedOverlay{false};
};

template<typename T>
class CDetector
{
public:
  CDetector(const FrameRef& frame, const DetectorParams& params);

  DetectionResult Run();

private:
  CSampleLine<T> Luma(const LineSpec& line) const;
  CSampleLine<T> Chroma(const PlaneRef& plane, const LineSpec& line) const;

  LineOutcome Classify(const LineSpec& line);
  bool ChromaIsNeutral(const LineSpec& line);
  EdgeMetrics MeasureEdge(const LineSpec& bar, const LineSpec& picture, unsigned int thickness);

  LineSpec Row(unsigned int y, unsigned int x0, unsigned int x1) const { return {true, y, x0, x1}; }
  LineSpec Column(unsigned int x, unsigned int y0, unsigned int y1) const
  {
    return {false, x, y0, y1};
  }

  //! \brief Fold one classified bar line into the running quality figures.
  void Accumulate(const LineOutcome& outcome);

  const FrameRef& m_frame;
  const DetectorParams& m_params;

  CRectInt m_roi;
  unsigned int m_shift{0};
  unsigned int m_bytes{1};
  unsigned int m_threshold{0}; //!< 8-bit equivalent bar threshold
  unsigned int m_fullScale{219}; //!< 8-bit equivalent nominal signal excursion
  unsigned int m_chromaShiftX{1};
  unsigned int m_chromaShiftY{1};
  bool m_hasChroma{false};
  bool m_rangeAssumed{false};

  CHistogram m_histogram;
  CHistogram m_remainder;
  CHistogram m_chromaU;
  CHistogram m_chromaV;

  unsigned int m_barLines{0};
  unsigned int m_overlayLines{0};
  bool m_overlaySuspected{false};
  //! Distributions across bar lines, not maxima. A single marginal line at the transition
  //! is where ringing lives and says nothing about the bar as a whole - taking the worst
  //! here would be the maximum-deviation mistake again, one level up.
  CHistogram m_lineHighs;
  CHistogram m_lineSpreads;
  unsigned int m_lowestSeen{BUCKET_COUNT - 1};
};

template<typename T>
CDetector<T>::CDetector(const FrameRef& frame, const DetectorParams& params)
  : m_frame(frame),
    m_params(params)
{
  m_shift = frame.bitDepth > 8 ? frame.bitDepth - 8 : 0;
  m_bytes = sizeof(T);

  ColorRange range = frame.range;
  if (range == ColorRange::Unspecified)
  {
    range = ColorRange::Limited;
    m_rangeAssumed = true;
  }

  const unsigned int black = range == ColorRange::Full ? 0u : 16u;
  m_threshold = black + m_params.margin;
  m_fullScale = range == ColorRange::Full ? 255u : 219u;

  m_chromaShiftX = frame.subsampling == ChromaSubsampling::YUV444 ? 0u : 1u;
  m_chromaShiftY = frame.subsampling == ChromaSubsampling::YUV420 ? 1u : 0u;
  m_hasChroma = frame.u.data != nullptr && frame.v.data != nullptr;
}

template<typename T>
CSampleLine<T> CDetector<T>::Luma(const LineSpec& line) const
{
  const std::ptrdiff_t stride = m_frame.y.strideBytes;
  const std::ptrdiff_t bytes = static_cast<std::ptrdiff_t>(m_bytes);

  if (line.row)
  {
    return {m_frame.y, m_shift,
            static_cast<std::ptrdiff_t>(line.index) * stride + line.begin * bytes, bytes,
            line.end - line.begin};
  }
  return {m_frame.y, m_shift, static_cast<std::ptrdiff_t>(line.begin) * stride + line.index * bytes,
          stride, line.end - line.begin};
}

template<typename T>
CSampleLine<T> CDetector<T>::Chroma(const PlaneRef& plane, const LineSpec& line) const
{
  const std::ptrdiff_t stride = plane.strideBytes;
  const std::ptrdiff_t bytes = static_cast<std::ptrdiff_t>(m_bytes);

  if (line.row)
  {
    const unsigned int cy = line.index >> m_chromaShiftY;
    const unsigned int begin = line.begin >> m_chromaShiftX;
    const unsigned int end = (line.end + (1u << m_chromaShiftX) - 1) >> m_chromaShiftX;
    return {plane, m_shift, static_cast<std::ptrdiff_t>(cy) * stride + begin * bytes, bytes,
            end > begin ? end - begin : 0};
  }

  const unsigned int cx = line.index >> m_chromaShiftX;
  const unsigned int begin = line.begin >> m_chromaShiftY;
  const unsigned int end = (line.end + (1u << m_chromaShiftY) - 1) >> m_chromaShiftY;
  return {plane, m_shift, static_cast<std::ptrdiff_t>(begin) * stride + cx * bytes, stride,
          end > begin ? end - begin : 0};
}

/*!
 * \brief Is the median chroma of this line achromatic?
 *
 * A true bar carries no colour. Dark saturated picture - a night sky, sodium haze - can be
 * both dark and flat in luma, and on PQ the luma tests barely discriminate at all because
 * 0.005 nits is only a dozen code values above black. The median is used rather than a
 * mean so that a bright overlay occupying a minority of the line cannot drag it.
 */
template<typename T>
bool CDetector<T>::ChromaIsNeutral(const LineSpec& line)
{
  if (!m_hasChroma)
    return true;

  const CSampleLine<T> u = Chroma(m_frame.u, line);
  const CSampleLine<T> v = Chroma(m_frame.v, line);
  if (u.Count() == 0)
    return true;

  m_chromaU.Reset();
  m_chromaV.Reset();
  for (unsigned int i = 0; i < u.Count(); ++i)
  {
    m_chromaU.Add(u[i]);
    m_chromaV.Add(v[i]);
  }

  const int neutral = 128;
  const int tolerance = static_cast<int>(m_params.chromaTolerance);
  const int medianU = static_cast<int>(m_chromaU.Percentile(0.5));
  const int medianV = static_cast<int>(m_chromaV.Percentile(0.5));

  return std::abs(medianU - neutral) <= tolerance && std::abs(medianV - neutral) <= tolerance;
}

template<typename T>
LineOutcome CDetector<T>::Classify(const LineSpec& line)
{
  LineOutcome outcome;

  const CSampleLine<T> luma = Luma(line);
  const unsigned int count = luma.Count();
  if (count == 0)
    return outcome;

  // One pass builds the histogram and locates the outermost above-threshold runs, so the
  // overlay test costs nothing unless the line fails outright.
  m_histogram.Reset();
  unsigned int runStart = 0;
  unsigned int runLength = 0;
  unsigned int lowest = count;
  unsigned int highest = 0;

  for (unsigned int i = 0; i < count; ++i)
  {
    const unsigned int value = luma[i];
    m_histogram.Add(value);

    if (value > m_threshold)
    {
      if (runLength == 0)
        runStart = i;
      ++runLength;
    }
    else
    {
      if (runLength >= m_params.minRunLength)
      {
        lowest = std::min(lowest, runStart);
        highest = std::max(highest, runStart + runLength - 1);
      }
      runLength = 0;
    }
  }
  if (runLength >= m_params.minRunLength)
  {
    lowest = std::min(lowest, runStart);
    highest = std::max(highest, runStart + runLength - 1);
  }

  const unsigned int high = m_histogram.Percentile(0.995);
  const unsigned int low = m_histogram.Percentile(0.005);
  const unsigned int spread = m_histogram.Percentile(0.75) - m_histogram.Percentile(0.25);

  outcome.high = high;
  outcome.low = low;
  outcome.spread = spread;

  // Dark and flat, judged by robust statistics. The high percentile tolerates half a
  // percent of the line being an outlier - encoder ringing, a stuck pixel - which is what
  // defeats a maximum-deviation test over a 4K-wide row. The interquartile range is the
  // dispersion measure: it is what separates a genuinely uniform bar from a dark scene
  // that merely stays below the threshold.
  if (high <= m_threshold && spread <= m_params.flatBand && ChromaIsNeutral(line))
  {
    outcome.classification = LineClass::Bar;
    return outcome;
  }

  if (lowest > highest)
    return outcome; // nothing bright enough to be an overlay

  const unsigned int extent = highest - lowest + 1;
  if (static_cast<float>(extent) >= m_params.overlaySuspectExtent * static_cast<float>(count))
    return outcome;

  m_remainder.Reset();
  for (unsigned int i = 0; i < count; ++i)
  {
    if (i < lowest || i > highest)
      m_remainder.Add(luma[i]);
  }

  if (m_remainder.Total() == 0)
    return outcome;

  const unsigned int remainderHigh = m_remainder.Percentile(0.995);
  const unsigned int remainderSpread = m_remainder.Percentile(0.75) - m_remainder.Percentile(0.25);

  if (remainderHigh > m_threshold || remainderSpread > m_params.flatBand || !ChromaIsNeutral(line))
    return outcome;

  outcome.high = remainderHigh;
  outcome.low = m_remainder.Percentile(0.005);
  outcome.spread = remainderSpread;

  // Subtitles, corner logos and timecode burn-ins are all horizontally localised; real
  // picture is not. One rule covers all three. Beyond the width bound the line is kept as
  // picture, which loses the bar - the wider, safer direction, and exactly what would have
  // happened with no overlay rule at all - and the caller is told why.
  if (static_cast<float>(extent) < m_params.overlayMaxExtent * static_cast<float>(count))
    outcome.classification = LineClass::Overlay;
  else
    outcome.suspectedOverlay = true;

  return outcome;
}

template<typename T>
void CDetector<T>::Accumulate(const LineOutcome& outcome)
{
  ++m_barLines;
  m_lineHighs.Add(outcome.high);
  m_lineSpreads.Add(outcome.spread);
  m_lowestSeen = std::min(m_lowestSeen, outcome.low);
  if (outcome.classification == LineClass::Overlay)
    ++m_overlayLines;
}

/*!
 * \brief How sharp is the step from the last bar line to the first picture line?
 *
 * Two numbers, because sharpness is two things: how large the step is, and how much of the
 * boundary shows it. A genuine letterbox edge is a large step across most of the boundary;
 * a dark scene's false boundary is a small step across a fraction of it. What a soft edge
 * is then permitted to do is the caller's policy, not this function's.
 */
template<typename T>
EdgeMetrics CDetector<T>::MeasureEdge(const LineSpec& bar,
                                      const LineSpec& picture,
                                      unsigned int thickness)
{
  EdgeMetrics metrics;
  metrics.thickness = thickness;
  if (thickness == 0)
    return metrics;

  const CSampleLine<T> barLine = Luma(bar);
  const CSampleLine<T> pictureLine = Luma(picture);
  const unsigned int count = std::min(barLine.Count(), pictureLine.Count());
  if (count == 0)
    return metrics;

  m_histogram.Reset();
  m_remainder.Reset();
  unsigned int stepped = 0;
  for (unsigned int i = 0; i < count; ++i)
  {
    const unsigned int barValue = barLine[i];
    const unsigned int pictureValue = pictureLine[i];
    m_histogram.Add(barValue);
    m_remainder.Add(pictureValue);
    if (pictureValue > barValue + m_params.margin)
      ++stepped;
  }

  const int barCeiling = static_cast<int>(m_histogram.Percentile(0.995));
  const int pictureLevel = static_cast<int>(m_remainder.Percentile(0.9));
  const int step = pictureLevel - barCeiling;

  metrics.measured = true;
  metrics.step = step > 0 ? static_cast<float>(step) / static_cast<float>(m_fullScale) : 0.0f;
  metrics.coverage = static_cast<float>(stepped) / static_cast<float>(count);
  return metrics;
}

template<typename T>
DetectionResult CDetector<T>::Run()
{
  DetectionResult result;
  result.rangeAssumed = m_rangeAssumed;
  result.chromaTested = m_hasChroma;
  result.thresholdUsed = m_threshold << m_shift;

  m_roi = m_frame.roi;
  if (m_roi.IsEmpty())
    m_roi = CRectInt(0, 0, static_cast<int>(m_frame.width), static_cast<int>(m_frame.height));

  m_roi.x1 = std::max(0, m_roi.x1);
  m_roi.y1 = std::max(0, m_roi.y1);
  m_roi.x2 = std::min(static_cast<int>(m_frame.width), m_roi.x2);
  m_roi.y2 = std::min(static_cast<int>(m_frame.height), m_roi.y2);

  result.rect = m_roi;

  if (m_frame.y.data == nullptr || m_roi.Width() <= 0 || m_roi.Height() <= 0)
  {
    result.degenerate = true;
    return result;
  }

  const unsigned int x0 = static_cast<unsigned int>(m_roi.x1);
  const unsigned int x1 = static_cast<unsigned int>(m_roi.x2);
  const unsigned int y0 = static_cast<unsigned int>(m_roi.y1);
  const unsigned int y1 = static_cast<unsigned int>(m_roi.y2);

  // Rows first. Columns cannot be judged until the rows are known, because on a
  // letterboxed frame every column contains the bars and would be dark and flat all the
  // way across.
  unsigned int top = y0;
  while (top < y1)
  {
    const LineOutcome outcome = Classify(Row(top, x0, x1));
    m_overlaySuspected |= outcome.suspectedOverlay;
    if (outcome.classification == LineClass::Picture)
      break;
    Accumulate(outcome);
    ++top;
  }

  unsigned int bottom = y1;
  while (bottom > top)
  {
    const LineOutcome outcome = Classify(Row(bottom - 1, x0, x1));
    m_overlaySuspected |= outcome.suspectedOverlay;
    if (outcome.classification == LineClass::Picture)
      break;
    Accumulate(outcome);
    --bottom;
  }

  if (bottom <= top || bottom - top < m_params.minContentExtent)
  {
    result.degenerate = true;
    result.rect = m_roi;
    return result;
  }

  // A finding thinner than the floor is not a bar. Discard it before the column walk, so
  // the columns are judged over the rows we actually believe in.
  const auto discardSubFloor = [&](unsigned int& edge, unsigned int limit, bool low)
  {
    const unsigned int thickness = low ? edge - limit : limit - edge;
    if (thickness == 0 || thickness >= m_params.edgeThicknessFloor)
      return;
    edge = limit;
    result.subFloorEdgesIgnored = true;
  };
  discardSubFloor(top, y0, true);
  discardSubFloor(bottom, y1, false);

  unsigned int left = x0;
  while (left < x1)
  {
    const LineOutcome outcome = Classify(Column(left, top, bottom));
    m_overlaySuspected |= outcome.suspectedOverlay;
    if (outcome.classification == LineClass::Picture)
      break;
    Accumulate(outcome);
    ++left;
  }

  unsigned int right = x1;
  while (right > left)
  {
    const LineOutcome outcome = Classify(Column(right - 1, top, bottom));
    m_overlaySuspected |= outcome.suspectedOverlay;
    if (outcome.classification == LineClass::Picture)
      break;
    Accumulate(outcome);
    --right;
  }

  if (right <= left || right - left < m_params.minContentExtent)
  {
    result.degenerate = true;
    result.rect = m_roi;
    return result;
  }

  discardSubFloor(left, x0, true);
  discardSubFloor(right, x1, false);

  result.rect = CRectInt(static_cast<int>(left), static_cast<int>(top), static_cast<int>(right),
                         static_cast<int>(bottom));

  const unsigned int topBar = top - y0;
  const unsigned int bottomBar = y1 - bottom;
  const unsigned int leftBar = left - x0;
  const unsigned int rightBar = x1 - right;

  result.top = MeasureEdge(Row(top - 1, left, right), Row(top, left, right), topBar);
  result.bottom = MeasureEdge(Row(bottom, left, right), Row(bottom - 1, left, right), bottomBar);
  result.left = MeasureEdge(Column(left - 1, top, bottom), Column(left, top, bottom), leftBar);
  result.right = MeasureEdge(Column(right, top, bottom), Column(right - 1, top, bottom), rightBar);

  result.overlayLines = m_overlayLines;
  result.overlaySuspected = m_overlaySuspected;
  result.blackFloorObserved = m_barLines > 0 ? (m_lowestSeen << m_shift) : 0;

  // Quality. Each factor is evidence about the reading, never a correction to it.
  const float margin = static_cast<float>(m_params.margin);
  const float band = static_cast<float>(m_params.flatBand);

  result.separation = 1.0f;
  result.flatness = 1.0f;
  if (m_barLines > 0)
  {
    const float highs = static_cast<float>(m_lineHighs.Percentile(0.9));
    const float spreads = static_cast<float>(m_lineSpreads.Percentile(0.9));
    result.separation = std::clamp((static_cast<float>(m_threshold) - highs) / margin, 0.0f, 1.0f);
    // Full marks until half the permitted dispersion is used; a bar with a little dither
    // is still plainly a bar.
    result.flatness = std::clamp((band - spreads) / (band * 0.5f), 0.0f, 1.0f);
  }

  // Asymmetry is scaled against the thicker bar, or the thickness floor when both are
  // slight - otherwise a nought-versus-two-pixel difference reads as total disagreement.
  // Off-centre bars are real, and reporting a rect rather than a ratio exists precisely so
  // they can be expressed, so this may cost confidence but must never move an edge.
  const auto asymmetry = [this](unsigned int a, unsigned int b)
  {
    const unsigned int reference = std::max({a, b, m_params.edgeThicknessFloor});
    const unsigned int difference = a > b ? a - b : b - a;
    return std::clamp(static_cast<float>(difference) / static_cast<float>(reference), 0.0f, 1.0f);
  };
  const float worstAsymmetry = std::max(asymmetry(topBar, bottomBar), asymmetry(leftBar, rightBar));
  result.symmetry = 1.0f - m_params.symmetryPenalty * worstAsymmetry;

  const auto score = [this](const EdgeMetrics& edge)
  { return std::clamp(edge.step / m_params.edgeStepReference, 0.0f, 1.0f) * edge.coverage; };

  // We are only as confident as the weakest evidence we have; averaging would let a single
  // soft boundary be outvoted, and a soft boundary is how a mask closes onto picture.
  // Every surviving edge is at least the thickness floor, so none of them is a sliver that
  // could speak for the frame without carrying any geometry.
  float confidence = 1.0f;
  bool haveEvidence = false;
  if (m_barLines > 0)
  {
    confidence = std::min(result.separation, result.flatness);
    haveEvidence = true;
  }
  for (const EdgeMetrics* edge : {&result.top, &result.bottom, &result.left, &result.right})
  {
    if (!edge->measured)
      continue;
    confidence = haveEvidence ? std::min(confidence, score(*edge)) : score(*edge);
    haveEvidence = true;
  }

  confidence *= result.symmetry;
  if (result.rangeAssumed)
    confidence *= m_params.rangeAssumedPenalty;
  if (!result.chromaTested)
    confidence *= m_params.chromaAbsentPenalty;

  // overlaySuspected deliberately carries no penalty. It means the walk stopped on a
  // bounded bright run it would not admit, so the rect may be too *wide* - the harmless
  // direction. Confidence exists to guard against readings that are too narrow, and
  // docking it for erring wide would tell a consumer to distrust the safe answer. It is
  // reported so the sampler can weigh it; measured on real content (The Rat Catcher,
  // 2026-08-06) it fires on ordinary dark picture columns beside a pillarbox and halved
  // the score on eight consecutive correct readings.

  result.confidence = std::clamp(confidence, 0.0f, 1.0f);
  return result;
}

} // unnamed namespace

DetectionResult DetectContentRect(const FrameRef& frame, const DetectorParams& params)
{
  if (frame.width == 0 || frame.height == 0 || frame.bitDepth < 8 || frame.bitDepth > 16)
  {
    DetectionResult rejected;
    rejected.degenerate = true;
    return rejected;
  }

  if (frame.bitDepth == 8)
    return CDetector<uint8_t>(frame, params).Run();

  return CDetector<uint16_t>(frame, params).Run();
}

} // namespace KODI::VIDEO::GEOMETRY
