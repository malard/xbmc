/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "video/geometry/ContentGeometryCombiner.h"

#include <vector>

#include <gtest/gtest.h>

using namespace KODI::VIDEO::GEOMETRY;

namespace
{

constexpr int WIDTH = 3840;
constexpr int HEIGHT = 2160;
const CRectInt CODED{0, 0, WIDTH, HEIGHT};

//! \brief A letterboxed reading with equal bars top and bottom.
GeometrySample Letterbox(int bar, float confidence, double position = 0.0)
{
  return {CRectInt{0, bar, WIDTH, HEIGHT - bar}, confidence, false, position};
}

GeometrySample FullFrame(float confidence, double position = 0.0)
{
  return {CODED, confidence, false, position};
}

GeometrySample Degenerate(double position = 0.0)
{
  return {CODED, 0.0f, true, position};
}

CombinedGeometry Combine(const std::vector<GeometrySample>& samples,
                         const CombinerParams& params = {})
{
  return CombineGeometrySamples(samples, CODED, params);
}

void ExpectRect(const CombinedGeometry& result, int x1, int y1, int x2, int y2)
{
  EXPECT_EQ(x1, result.rect.x1);
  EXPECT_EQ(y1, result.rect.y1);
  EXPECT_EQ(x2, result.rect.x2);
  EXPECT_EQ(y2, result.rect.y2);
}

} // namespace

TEST(TestContentGeometryCombiner, UnanimousSamples)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 9; ++i)
    samples.push_back(Letterbox(263, 0.9f, i * 600.0));

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 263, WIDTH, HEIGHT - 263);
  EXPECT_TRUE(result.hasReading);
  EXPECT_FALSE(result.varies);
  EXPECT_FLOAT_EQ(1.0f, result.share);
  EXPECT_EQ(1u, result.clusters.size());
  EXPECT_EQ(9u, result.usable);
}

/*!
 * The Menu, 2026-08-06: eight samples at 263/263 and one dark-scene reading of 3062x1015.
 * The outlier must vanish without trace. A "widest wins" rule would take the outlier's
 * width in some frames and collapse the film; a median over edges would survive this but
 * not the variable-aspect case below.
 */
TEST(TestContentGeometryCombiner, LoneOutlierIsAbsorbed)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 8; ++i)
    samples.push_back(Letterbox(263, 0.8f, i * 600.0));
  samples.push_back({CRectInt{59, 869, 3121, 1884}, 0.4f, false, 3857.0});

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 263, WIDTH, HEIGHT - 263);
  EXPECT_FALSE(result.varies);
  EXPECT_EQ(2u, result.clusters.size());
  EXPECT_EQ(8u, result.clusters.front().samples);
}

/*!
 * A cut to black is dark and flat on every line, so the detector reports maximum bars. That
 * is not a very narrow reading, it is no reading, and combining it as narrow is the most
 * likely way a mask ends up closing onto picture.
 */
TEST(TestContentGeometryCombiner, DegenerateSamplesAreDiscarded)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 5; ++i)
    samples.push_back(Letterbox(263, 0.9f, i * 600.0));
  for (int i = 0; i < 4; ++i)
    samples.push_back(Degenerate(i * 700.0));

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 263, WIDTH, HEIGHT - 263);
  EXPECT_EQ(5u, result.usable);
  EXPECT_EQ(4u, result.discarded);
  EXPECT_FALSE(result.varies);
}

TEST(TestContentGeometryCombiner, LowConfidenceSamplesAreDiscarded)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 5; ++i)
    samples.push_back(Letterbox(263, 0.9f, i * 600.0));
  for (int i = 0; i < 3; ++i)
    samples.push_back(Letterbox(700, 0.01f, 4000.0 + i));

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 263, WIDTH, HEIGHT - 263);
  EXPECT_EQ(5u, result.usable);
  EXPECT_EQ(3u, result.discarded);
}

/*!
 * Mandalorian & Grogu, IMAX HYBRID: alternates between full frame and 2.40 through the
 * film. Two stationary clusters is what varying geometry looks like.
 */
TEST(TestContentGeometryCombiner, TwoStationaryClustersMeanVaries)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 4; ++i)
    samples.push_back(FullFrame(1.0f, i * 800.0));
  for (int i = 0; i < 2; ++i)
    samples.push_back(Letterbox(275, 0.9f, 4000.0 + i * 800.0));

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 0, WIDTH, HEIGHT);
  EXPECT_TRUE(result.varies);
  EXPECT_EQ(2u, result.clusters.size());
}

/*!
 * The bias this rule exists to defeat, and the reason varies is counted rather than
 * weighted. A full-frame reading scores top marks by construction - no edges means no soft
 * boundary to dock it for - while the letterboxed readings of the same title score far
 * lower because they have boundaries. Weighted, the minority geometry disappears; counted,
 * it survives. Measured on a real variable-aspect title, where weighting hid it completely.
 */
TEST(TestContentGeometryCombiner, VariesIsCountedNotWeighted)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 4; ++i)
    samples.push_back(FullFrame(1.0f, i * 800.0));
  for (int i = 0; i < 2; ++i)
    samples.push_back(Letterbox(275, 0.07f, 4000.0 + i * 800.0));

  const CombinedGeometry result = Combine(samples);

  EXPECT_TRUE(result.varies) << "confidence weighting has suppressed the minority geometry";
  EXPECT_LT(result.share, 1.0f);
}

/*!
 * The Wolf of Wall Street: one sample of five landed in a genuine pillarboxed sequence.
 * One sample is a point, not a cluster, and stationarity is a claim about repetition - so
 * this is not enough to call the title variable, however real the reading turned out to be.
 */
TEST(TestContentGeometryCombiner, SingleRivalSampleIsNotACluster)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 4; ++i)
    samples.push_back(FullFrame(1.0f, i * 1500.0));
  samples.push_back({CRectInt{415, 0, WIDTH - 410, HEIGHT}, 0.26f, false, 8994.0});

  const CombinedGeometry result = Combine(samples);

  ExpectRect(result, 0, 0, WIDTH, HEIGHT);
  EXPECT_FALSE(result.varies);
  EXPECT_EQ(2u, result.clusters.size()) << "the reading must still be retained";
}

/*!
 * Never narrower under uncertainty. With nothing to go on the answer is the coded frame,
 * which shows bars at worst; anything narrower risks masking real picture.
 */
TEST(TestContentGeometryCombiner, NoUsableSamplesReportsTheCodedFrame)
{
  const CombinedGeometry result = Combine({Degenerate(100.0), Degenerate(200.0)});

  ExpectRect(result, 0, 0, WIDTH, HEIGHT);
  EXPECT_FALSE(result.hasReading);
  EXPECT_FALSE(result.varies);
  EXPECT_EQ(0u, result.usable);
}

TEST(TestContentGeometryCombiner, NoSamplesAtAllReportsTheCodedFrame)
{
  const CombinedGeometry result = Combine({});

  ExpectRect(result, 0, 0, WIDTH, HEIGHT);
  EXPECT_FALSE(result.hasReading);
}

//! Boundary ringing moves an edge by a line or two; that is the same edge.
TEST(TestContentGeometryCombiner, EdgesWithinToleranceAreOneCluster)
{
  const CombinedGeometry result =
      Combine({Letterbox(263, 0.9f), Letterbox(265, 0.9f), Letterbox(262, 0.9f),
               Letterbox(264, 0.9f), Letterbox(263, 0.9f)});

  EXPECT_EQ(1u, result.clusters.size());
  EXPECT_EQ(263, result.rect.y1) << "cluster centre should be the median of its members";
  EXPECT_FALSE(result.varies);
}

//! Beyond the tolerance they are different edges, and two of them means the title varies.
TEST(TestContentGeometryCombiner, EdgesBeyondToleranceAreSeparateClusters)
{
  const CombinedGeometry result =
      Combine({Letterbox(263, 0.9f), Letterbox(263, 0.9f), Letterbox(263, 0.9f),
               Letterbox(140, 0.9f), Letterbox(140, 0.9f)});

  EXPECT_EQ(2u, result.clusters.size());
  EXPECT_EQ(263, result.rect.y1);
  EXPECT_TRUE(result.varies);
}

/*!
 * The dominant cluster is chosen on weight, so a handful of confident samples beat a larger
 * group of doubtful ones - but only for the rectangle. varies is unaffected, by design.
 */
TEST(TestContentGeometryCombiner, DominantClusterIsChosenOnWeight)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 3; ++i)
    samples.push_back(Letterbox(263, 1.0f, i * 600.0));
  for (int i = 0; i < 4; ++i)
    samples.push_back(Letterbox(140, 0.2f, 3000.0 + i * 600.0));

  const CombinedGeometry result = Combine(samples);

  EXPECT_EQ(263, result.rect.y1);
  EXPECT_EQ(3u, result.clusters.front().samples);
}

TEST(TestContentGeometryCombiner, ClusterCentreIsTheMedianOfItsMembers)
{
  const CombinedGeometry result =
      Combine({Letterbox(260, 0.9f), Letterbox(263, 0.9f), Letterbox(266, 0.9f)});

  EXPECT_EQ(1u, result.clusters.size());
  EXPECT_EQ(263, result.clusters.front().rect.y1);
  EXPECT_EQ(HEIGHT - 263, result.clusters.front().rect.y2);
}

//! Pillarboxing, to prove clustering is on the whole rectangle and not on height alone.
TEST(TestContentGeometryCombiner, PillarboxAndLetterboxAreDistinguished)
{
  std::vector<GeometrySample> samples;
  for (int i = 0; i < 4; ++i)
    samples.push_back({CRectInt{480, 0, WIDTH - 480, HEIGHT}, 0.9f, false, i * 600.0});
  for (int i = 0; i < 4; ++i)
    samples.push_back(Letterbox(480, 0.9f, 3000.0 + i * 600.0));

  const CombinedGeometry result = Combine(samples);

  EXPECT_EQ(2u, result.clusters.size()) << "same bar thickness, different rectangle";
  EXPECT_TRUE(result.varies);
}
