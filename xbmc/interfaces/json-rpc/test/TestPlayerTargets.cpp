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
 \brief A playlist the caller does not name reaches the handler unnamed

 The validator fills an omitted parameter with its default, and the first value of an enum
 without one, which would name the video playlist for every call that names none.
 */
TEST_F(TestPlayerTargetParameter, AnOmittedPlaylistIsNotNamed)
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
    EXPECT_TRUE(output["playlist"].isNull()) << name << " names " << ToJson(output["playlist"]);
  }
  EXPECT_GT(checked, 0);

  CVariant output;
  ASSERT_EQ(OK, Call("Player.Stop", R"({"playlist": "audio"})", output));
  EXPECT_EQ("audio", output["playlist"].asString());
}
