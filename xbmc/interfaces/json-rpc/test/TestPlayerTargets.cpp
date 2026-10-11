/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "interfaces/json-rpc/PlayerOperations.h"
#include "utils/Variant.h"

#include <initializer_list>

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

CVariant Properties(std::initializer_list<const char*> names)
{
  CVariant params(CVariant::VariantTypeObject);
  params["properties"] = CVariant(CVariant::VariantTypeArray);
  for (const char* name : names)
    params["properties"].push_back(name);
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

//! \brief An idle player refuses what only playback can answer rather than answering defaults
TEST(TestPlayerTargets, APlaybackPropertyIsNothingPlayingWhenNothingPlays)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute,
            CPlayerOperations::GetProperties(Properties({"hasMedia", "speed", "type"}), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_FALSE(result.isMember("target"));
}

TEST(TestPlayerTargets, APlaybackPropertyOfAnIdleNamedPlayListIsNothingPlaying)
{
  CVariant params = Properties({"speed"});
  params["playlist"] = "audio";

  CVariant result;
  EXPECT_EQ(FailedToExecute, CPlayerOperations::GetProperties(params, result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
  EXPECT_EQ("audio", result["target"]["playlist"].asString());
}

TEST(TestPlayerTargets, HasMediaAnswersWhenNothingPlays)
{
  CVariant result;
  ASSERT_EQ(OK, CPlayerOperations::GetProperties(Properties({"hasMedia"}), result));
  EXPECT_FALSE(result["hasMedia"].asBoolean(true));
}

TEST(TestPlayerTargets, HasMediaIsTrueForARunningSlideshow)
{
  const auto playLists = CServiceBroker::GetPlayLists();
  playLists->SetSlideShowRunning(true);

  CVariant result;
  EXPECT_EQ(OK, CPlayerOperations::GetProperties(Properties({"hasMedia", "type"}), result));
  EXPECT_TRUE(result["hasMedia"].asBoolean());
  EXPECT_EQ("picture", result["type"].asString());

  CVariant params = Properties({"hasMedia"});
  params["playlist"] = "audio";
  EXPECT_EQ(OK, CPlayerOperations::GetProperties(params, result));
  EXPECT_FALSE(result["hasMedia"].asBoolean(true));

  playLists->SetSlideShowRunning(false);
}

TEST(TestPlayerTargets, TheItemIsNothingPlayingWhenNothingPlays)
{
  CVariant result;
  EXPECT_EQ(FailedToExecute,
            CPlayerOperations::GetItem(CVariant(CVariant::VariantTypeObject), result));
  EXPECT_EQ("nothing-playing", result["reason"].asString());
}
