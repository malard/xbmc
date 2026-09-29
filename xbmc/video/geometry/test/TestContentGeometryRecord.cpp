/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "test/TestUtils.h"
#include "video/geometry/ContentGeometryRecord.h"
#include "video/geometry/test/GeometryTestHelpers.h"

#include <gtest/gtest.h>

using namespace KODI::VIDEO::GEOMETRY;
using namespace KODI::VIDEO::GEOMETRY::TEST;

TEST(TestFileIdentity, AnIdentityMatchesItself)
{
  const FileIdentity identity{1234567890, 1700000000};

  EXPECT_TRUE(identity.IsKnown());
  EXPECT_TRUE(identity.Matches(identity));
}

TEST(TestFileIdentity, EitherFieldDifferingIsADifferentFile)
{
  const FileIdentity stored{1234567890, 1700000000};

  EXPECT_FALSE(stored.Matches({1234567891, 1700000000}));
  EXPECT_FALSE(stored.Matches({1234567890, 1700000001}));
}

//! Neither field alone: a re-crop can preserve the byte count, and a restore can preserve
//! the size while changing the mtime.
TEST(TestFileIdentity, NeitherFieldAloneDecides)
{
  const FileIdentity recropped{1234567890, 1700009999};
  const FileIdentity restored{9999999999, 1700000000};
  const FileIdentity original{1234567890, 1700000000};

  EXPECT_FALSE(original.Matches(recropped));
  EXPECT_FALSE(original.Matches(restored));
}

//! An unknown identity never matches, so a caller that could not identify the file ends up
//! with no rectangle.
TEST(TestFileIdentity, AnUnknownIdentityMatchesNothingIncludingItself)
{
  const FileIdentity unknown;
  const FileIdentity known{100, 200};

  EXPECT_FALSE(unknown.IsKnown());
  EXPECT_FALSE(unknown.Matches(unknown));
  EXPECT_FALSE(unknown.Matches(known));
  EXPECT_FALSE(known.Matches(unknown));
}

TEST(TestFileIdentity, PartiallyKnownIsUnknown)
{
  EXPECT_FALSE((FileIdentity{100, -1}).IsKnown());
  EXPECT_FALSE((FileIdentity{-1, 200}).IsKnown());
}

TEST(TestFileIdentity, AMissingFileHasNoIdentity)
{
  EXPECT_FALSE(GetFileIdentity("special://temp/no-such-file-content-geometry.mkv").IsKnown());
}

TEST(TestFileIdentity, ARealFileHasOneAndItIsStable)
{
  XFILE::CFile* file{XBMC_CREATETEMPFILE(".mkv")};
  ASSERT_NE(nullptr, file);
  const std::string path{XBMC_TEMPFILEPATH(file)};

  const FileIdentity identity{GetFileIdentity(path)};
  EXPECT_TRUE(identity.IsKnown());

  // Reading it twice must give the same answer.
  EXPECT_TRUE(identity.Matches(GetFileIdentity(path)));

  EXPECT_TRUE(XBMC_DELETETEMPFILE(file));
}

TEST(TestGeometrySections, RoundTripThroughTheStoredForm)
{
  const std::vector<CRectInt> sections{CRectInt{0, 140, 1920, 940}, CRectInt{240, 0, 1680, 1080}};

  EXPECT_EQ("0,140,1920,800;240,0,1440,1080", EncodeGeometrySections(sections));
  EXPECT_EQ(sections, DecodeGeometrySections(EncodeGeometrySections(sections)));
}

//! A stereoscopic scan measures one view, whose frame is an offset region of the picture, so
//! an origin left of the frame's is a real measurement rather than a corrupt one.
TEST(TestGeometrySections, ANegativeOriginSurvives)
{
  const std::vector<CRectInt> sections{CRectInt{-120, -8, 840, 568}};

  EXPECT_EQ(sections, DecodeGeometrySections(EncodeGeometrySections(sections)));
}

TEST(TestGeometrySections, NoSectionsIsAnEmptyValue)
{
  EXPECT_TRUE(EncodeGeometrySections({}).empty());
  EXPECT_TRUE(DecodeGeometrySections("").empty());
}

//! A value that stops making sense costs the shapes past that point rather than the record.
TEST(TestGeometrySections, AMalformedValueKeepsWhatParsed)
{
  EXPECT_TRUE(DecodeGeometrySections("not a rectangle").empty());
  EXPECT_TRUE(DecodeGeometrySections("0,140,1920").empty()) << "a short shape is not a shape";

  const std::vector<CRectInt> first{CRectInt{0, 140, 1920, 940}};
  EXPECT_EQ(first, DecodeGeometrySections("0,140,1920,800;240,0"));
}

/*!
 * The column is plain text a user can edit, and the far edge of a shape is an origin plus a
 * size - so two values each inside the range can still add to something outside it.
 */
TEST(TestGeometrySections, AnExtentOutsideTheRangeIsRefused)
{
  EXPECT_TRUE(DecodeGeometrySections("2000000000,0,2000000000,1080").empty());
  EXPECT_TRUE(DecodeGeometrySections("0,2000000000,1920,2000000000").empty());
  EXPECT_TRUE(DecodeGeometrySections("-2000000000,0,-2000000000,1080").empty());

  // A value outside the range on its own was already refused, and still is.
  EXPECT_TRUE(DecodeGeometrySections("99999999999,0,1920,1080").empty());

  // The shapes read before the impossible one are kept, as with any other malformed tail.
  const std::vector<CRectInt> first{CRectInt{0, 140, 1920, 940}};
  EXPECT_EQ(first, DecodeGeometrySections("0,140,1920,800;2000000000,0,2000000000,1080"));
}

//! Either separator is accepted wherever it appears - see DecodeGeometrySections().
TEST(TestGeometrySections, EitherSeparatorReadsTheSameShapes)
{
  const std::vector<CRectInt> sections{CRectInt{0, 140, 1920, 940}, CRectInt{240, 0, 1920, 1080}};

  EXPECT_EQ(sections, DecodeGeometrySections("0,140,1920,800;240,0,1680,1080"));
  EXPECT_EQ(sections, DecodeGeometrySections("0;140;1920;800,240,0,1680,1080"));
}

TEST(TestContentGeometryRecord, DefaultsToTheCurrentAlgorithmVersion)
{
  EXPECT_EQ(CONTENT_GEOMETRY_ALGORITHM_VERSION, ContentGeometryRecord{}.algorithmVersion);
}

TEST(TestContentGeometryRecord, ARecordWithoutACodedFrameIsNotValid)
{
  ContentGeometryRecord record;
  EXPECT_FALSE(record.IsValid());

  record.coded = CRectInt{0, 0, 1920, 1080};
  EXPECT_TRUE(record.IsValid());
}

TEST(TestContentGeometryLookup, MissingCarriesNoRecord)
{
  EXPECT_FALSE(ContentGeometryLookup{}.HasRecord());
  EXPECT_EQ(ContentGeometryState::MISSING, ContentGeometryLookup{}.state);
}


