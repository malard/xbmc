/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
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
}

//! \brief Chapters asked of a playlist that is not playing fail as the other verbs do
TEST(TestPlayerTargets, ChaptersOfAnIdlePlaylistAreNothingPlaying)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::GetChapters(Named("audio"), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
}

class TestPlayerTargetParameter : public JSONServiceDescriptionTestBase
{
};

/*!
 \brief A playlist the caller does not name reaches the handler as "playing"

 The validator fills an omitted parameter with its default, and the first value of an enum
 without one, so a target that named a playlist first would name it for every call that names
 none.
 */
TEST_F(TestPlayerTargetParameter, AnOmittedPlaylistIsPlaying)
{
  AddShippedServiceDescription();

  int checked = 0;
  for (const auto& [name, method] : ShippedMethods())
  {
    if (name.rfind("Player.", 0) != 0 || Param(method, "playlist") == nullptr)
      continue;

    // a call with a required parameter cannot be made without naming something
    bool hasRequired = false;
    for (const auto& [param, descriptor] : Params(method))
      hasRequired = hasRequired || descriptor["required"].asBoolean();
    if (hasRequired)
      continue;

    ++checked;
    CVariant output;
    ASSERT_EQ(OK, Call(name.c_str(), "{}", output)) << name << ": " << ToJson(output);
    EXPECT_EQ("playing", output["playlist"].asString()) << name;
  }
  EXPECT_GT(checked, 0);

  CVariant output;
  ASSERT_EQ(OK, Call("Player.Stop", R"({"playlist": "audio"})", output));
  EXPECT_EQ("audio", output["playlist"].asString());
  ASSERT_EQ(OK, Call("Player.Stop", R"({"playlist": "picture"})", output));
  EXPECT_EQ("picture", output["playlist"].asString());
}

//! \brief The picture playlist names the slideshow, which is not running
TEST(TestPlayerTargets, AVerbOnAnIdleSlideshowIsNothingPlaying)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::Stop(Named("picture"), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_EQ("picture", result["target"]["playlist"].asString());
}
