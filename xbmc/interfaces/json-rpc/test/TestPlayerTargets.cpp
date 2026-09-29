/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/PlayerOperations.h"
#include "utils/Variant.h"

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
CVariant Named(const char* playlist)
{
  CVariant params(CVariant::VariantTypeObject);
  params["playlist"] = playlist;
  return params;
}
} // namespace

TEST(TestPlayerTargets, AVerbOnThePlayerFailsWhenTheNamedPlayListIsIdle)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::Stop(Named("audio"), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_EQ("audio", result["target"]["playlist"].asString());

  result = CVariant();
  EXPECT_EQ(FailedToExecute, CPlayerOperations::PlayPause(Named("video"), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_EQ("video", result["target"]["playlist"].asString());
}

//! \brief With nothing playing, a verb that cannot act says so rather than blaming the media
TEST(TestPlayerTargets, AVerbFailsForNothingPlayingWhenNothingPlays)
{
  CVariant params(CVariant::VariantTypeObject);
  params["value"]["seconds"] = 10;

  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::Seek(params, result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_FALSE(result.isMember("target"));

  result = CVariant();
  EXPECT_EQ(FailedToExecute,
            CPlayerOperations::Zoom(CVariant(CVariant::VariantTypeObject), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
}
