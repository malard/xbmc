/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "video/geometry/FrameSampling.h"

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

using namespace KODI::VIDEO::GEOMETRY;

namespace
{
constexpr double RUNTIME = 6000.0; // a 100 minute film
}

TEST(TestFrameSampling, DefaultScheduleSpansTheWindow)
{
  const std::vector<double> offsets = SampleOffsets(RUNTIME);

  ASSERT_EQ(9u, offsets.size());
  EXPECT_DOUBLE_EQ(RUNTIME * 0.10, offsets.front());
  EXPECT_DOUBLE_EQ(RUNTIME * 0.85, offsets.back());
}

/*!
 * The trap that produced a false reading before it was understood: opening frames are
 * studio logos and idents whose geometry is not the title's.
 */
TEST(TestFrameSampling, NeverSamplesTheStart)
{
  for (unsigned int points = 1; points <= 40; ++points)
  {
    SamplingParams params;
    params.points = points;
    for (const double offset : SampleOffsets(RUNTIME, params))
      EXPECT_GT(offset, 0.0) << "points = " << points;
  }
}

/*!
 * A ten minute credit roll on a hundred minute film puts a 90% sample squarely in credits,
 * which are frequently full-container on scope titles. Measured on a ten-title corpus, the
 * 90% position was discarded more often than any other.
 */
TEST(TestFrameSampling, StopsShortOfTheEnd)
{
  for (unsigned int points = 1; points <= 40; ++points)
  {
    SamplingParams params;
    params.points = points;
    for (const double offset : SampleOffsets(RUNTIME, params))
      EXPECT_LE(offset, RUNTIME * 0.85) << "points = " << points;
  }
}

TEST(TestFrameSampling, OffsetsAreOrderedAndDistinct)
{
  const std::vector<double> offsets = SampleOffsets(RUNTIME);
  for (size_t i = 1; i < offsets.size(); ++i)
    EXPECT_GT(offsets[i], offsets[i - 1]);
}

TEST(TestFrameSampling, SinglePointLandsMidWindow)
{
  SamplingParams params;
  params.points = 1;

  const std::vector<double> offsets = SampleOffsets(RUNTIME, params);
  ASSERT_EQ(1u, offsets.size());
  EXPECT_DOUBLE_EQ(RUNTIME * 0.475, offsets.front());
}

TEST(TestFrameSampling, UnusableInputsYieldNoSchedule)
{
  EXPECT_TRUE(SampleOffsets(0.0).empty());
  EXPECT_TRUE(SampleOffsets(-1.0).empty());

  SamplingParams none;
  none.points = 0;
  EXPECT_TRUE(SampleOffsets(RUNTIME, none).empty());
}

//! A very short item still gets sampled, just proportionally.
TEST(TestFrameSampling, ShortItem)
{
  const std::vector<double> offsets = SampleOffsets(20.0);

  ASSERT_EQ(9u, offsets.size());
  EXPECT_GT(offsets.front(), 0.0);
  EXPECT_LE(offsets.back(), 17.0);
}

TEST(TestFrameSampling, InvertedWindowDoesNotProduceGarbage)
{
  SamplingParams params;
  params.windowStart = 0.9;
  params.windowEnd = 0.1;

  for (const double offset : SampleOffsets(RUNTIME, params))
  {
    EXPECT_GT(offset, 0.0);
    EXPECT_LE(offset, RUNTIME);
  }
}

// --- escalation -----------------------------------------------------------------------

/*!
 * Measured on a hybrid IMAX title: nine points landed on no full-frame section at all and
 * reported a fixed ratio, where twenty-seven found both geometries. What marked the short
 * pass as untrustworthy was not the rectangle - every surviving sample agreed - but that
 * four of nine samples had been thrown away.
 */
TEST(TestFrameSampling, HighDiscardRateEscalates)
{
  EXPECT_TRUE(ShouldEscalate(1, 5, 4)); // the varying title: 44% discarded
  EXPECT_FALSE(ShouldEscalate(1, 8, 1)); // 11% discarded
  EXPECT_FALSE(ShouldEscalate(1, 9, 0)); // unanimous
}

TEST(TestFrameSampling, MoreThanOneGeometryEscalates)
{
  EXPECT_TRUE(ShouldEscalate(2, 9, 0)) << "densify before believing a title varies";
}

TEST(TestFrameSampling, NothingSurvivingEscalates)
{
  EXPECT_TRUE(ShouldEscalate(0, 0, 9));
}

TEST(TestFrameSampling, EscalationCanBeDisabled)
{
  SamplingParams params;
  params.escalatedPoints = 0;

  EXPECT_FALSE(ShouldEscalate(2, 5, 4, params));

  params.escalatedPoints = params.points; // no denser than we already were
  EXPECT_FALSE(ShouldEscalate(2, 5, 4, params));
}

TEST(TestFrameSampling, EscalationOnlyAddsPositionsNotAlreadyMeasured)
{
  const std::vector<double> first = SampleOffsets(RUNTIME);

  SamplingParams denser;
  denser.points = 27;
  const std::vector<double> all = SampleOffsets(RUNTIME, denser);
  const std::vector<double> extra = UnsampledOffsets(all, first);

  // Nothing already measured is measured again...
  for (const double offset : extra)
  {
    for (const double taken : first)
      EXPECT_GE(std::abs(offset - taken), 1.0);
  }

  // ...and nothing wanted is dropped: every denser position is either scheduled or was
  // already covered. Only the few positions the two schedules share drop out - three of
  // twenty-seven here, not nine, because the schedules do not nest.
  for (const double offset : all)
  {
    const bool scheduled = std::find(extra.begin(), extra.end(), offset) != extra.end();
    const bool covered = std::any_of(first.begin(), first.end(),
                                     [&](double taken) { return std::abs(taken - offset) < 1.0; });
    EXPECT_TRUE(scheduled || covered) << "position " << offset << " would never be sampled";
  }

  EXPECT_GT(extra.size(), first.size());
  EXPECT_LT(extra.size(), all.size());
}

// --- stereoscopic ---------------------------------------------------------------------

TEST(TestFrameSampling, SideBySideSelectsOneView)
{
  const CRectInt view = StereoViewRect("left_right", 3840, 2160);

  EXPECT_EQ(0, view.x1);
  EXPECT_EQ(0, view.y1);
  EXPECT_EQ(1920, view.x2);
  EXPECT_EQ(2160, view.y2);

  EXPECT_EQ(view.x2, StereoViewRect("right_left", 3840, 2160).x2);
}

TEST(TestFrameSampling, TopAndBottomSelectsOneView)
{
  const CRectInt view = StereoViewRect("top_bottom", 3840, 2160);

  EXPECT_EQ(3840, view.x2);
  EXPECT_EQ(1080, view.y2);

  EXPECT_EQ(view.y2, StereoViewRect("bottom_top", 3840, 2160).y2);
}

/*!
 * An unrecognised mode must be left whole rather than guessed at. Halving a mono frame
 * would report a pillarbox that is not there, and that is the direction that masks real
 * picture.
 */
TEST(TestFrameSampling, MonoAndUnknownModesAreLeftWhole)
{
  EXPECT_TRUE(StereoViewRect("", 3840, 2160).IsEmpty());
  EXPECT_TRUE(StereoViewRect("mono", 3840, 2160).IsEmpty());
  EXPECT_TRUE(StereoViewRect("block_lr", 3840, 2160).IsEmpty());
  EXPECT_TRUE(StereoViewRect("anaglyph_cyan_red", 3840, 2160).IsEmpty());
}
