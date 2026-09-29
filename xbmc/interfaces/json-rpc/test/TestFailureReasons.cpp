/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/DatabaseOperations.h"
#include "interfaces/json-rpc/GUIOperations.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

//! \brief The video frame alone cannot be captured while no video plays
TEST(TestFailureReasons, AVideoScreenshotWithNothingPlayingIsNothingPlaying)
{
  CVariant params(CVariant::VariantTypeObject);
  params["content"] = "video";

  CVariant result;
  EXPECT_EQ(FailedToExecute, CGUIOperations::TakeScreenshot(params, result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
}

//! \brief Screenshot deletion is off unless advancedsettings turns it on
TEST(TestFailureReasons, DeletingScreenshotsWhileItIsOffIsDisabled)
{
  CVariant result;
  EXPECT_EQ(Unavailable,
            CGUIOperations::DeleteScreenshots(CVariant(CVariant::VariantTypeObject), result));
  EXPECT_EQ("disabled", result["reason"].asString());
}

//! \brief The test environment opens only the add-on database
TEST(TestFailureReasons, ADatabaseNotYetOpenedIsDatabaseNotOpen)
{
  CVariant params(CVariant::VariantTypeObject);
  params["type"] = "videos";

  CVariant result;
  EXPECT_EQ(FailedToExecute, CDatabaseOperations::GetDatabaseName(params, result));
  EXPECT_EQ("database-not-open", result["reason"].asString());
  EXPECT_EQ("videos", result["target"]["type"].asString());
}
