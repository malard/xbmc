/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "ServiceDescription.h"

#include <array>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

//! \brief Every kind of video library item
constexpr std::array<const char*, 6> KINDS{
    "movie", "set", "tvshow", "season", "episode", "musicvideo",
};

} // unnamed namespace

//! \brief Drives the shipped schema, so what is validated is what a client reaches
class TestVideoLibraryRefreshSchema : public JSONServiceDescriptionTestBase
{
public:
  void SetUp() override
  {
    JSONServiceDescriptionTestBase::SetUp();

    AddShippedServiceDescription();
  }
};

TEST_F(TestVideoLibraryRefreshSchema, EveryKindOfLibraryItemCanBeNamed)
{
  for (const char* kind : KINDS)
  {
    CVariant output;
    const std::string params{R"({"item": {"kind": ")" + std::string(kind) + R"(", "id": 7}})"};

    EXPECT_EQ(OK, Call("VideoLibrary.Refresh", params, output)) << kind;
    EXPECT_EQ(kind, output["item"]["kind"].asString()) << kind;
    EXPECT_EQ(7, output["item"]["id"].asInteger()) << kind;
  }
}

/*!
 The four deprecated methods each named one kind, so a caller sent off one of
 them has to find its own kind here or the deprecation strands it.
 */
TEST_F(TestVideoLibraryRefreshSchema, TheDeprecatedMethodsStillTakeWhatTheyAlwaysDid)
{
  CVariant output;
  EXPECT_EQ(OK, Call("VideoLibrary.RefreshMovie", R"({"movieId": 7})", output));
  EXPECT_EQ(OK, Call("VideoLibrary.RefreshTVShow", R"({"tvShowId": 7})", output));
  EXPECT_EQ(OK, Call("VideoLibrary.RefreshEpisode", R"({"episodeId": 7})", output));
  EXPECT_EQ(OK, Call("VideoLibrary.RefreshMusicVideo", R"({"musicVideoId": 7})", output));
}

TEST_F(TestVideoLibraryRefreshSchema, AnItemNamesOneKindAndNamesItProperly)
{
  CVariant output;
  EXPECT_EQ(InvalidParams, Call("VideoLibrary.Refresh", R"({})", output));
  EXPECT_EQ(InvalidParams, Call("VideoLibrary.Refresh", R"({"item": {}})", output));
  EXPECT_EQ(InvalidParams, Call("VideoLibrary.Refresh", R"({"item": {"movieId": 7}})", output));
  EXPECT_EQ(InvalidParams, Call("VideoLibrary.Refresh",
                                R"({"item": {"kind": "movie", "id": 7, "tvShowId": 7}})", output));

  // Not a library item this method can refresh, however well formed
  EXPECT_EQ(InvalidParams,
            Call("VideoLibrary.Refresh", R"({"item": {"kind": "album", "id": 7}})", output));

  // A library id starts at 1, so the id no row can have is refused rather than looked up
  EXPECT_EQ(InvalidParams,
            Call("VideoLibrary.Refresh", R"({"item": {"kind": "movie", "id": 0}})", output));
}

TEST_F(TestVideoLibraryRefreshSchema, TheRefreshOptionsCarryOverWithTheirDefaults)
{
  CVariant output;
  ASSERT_EQ(OK, Call("VideoLibrary.Refresh", R"({"item": {"kind": "tvshow", "id": 7}})", output));
  EXPECT_FALSE(output["ignoreNfo"].asBoolean());
  EXPECT_FALSE(output["refreshEpisodes"].asBoolean());
  EXPECT_EQ("", output["title"].asString());

  ASSERT_EQ(
      OK, Call("VideoLibrary.Refresh",
               R"({"item": {"kind": "tvshow", "id": 7}, "ignoreNfo": true, "refreshEpisodes": true,
                         "title": "Planetes"})",
               output));
  EXPECT_TRUE(output["ignoreNfo"].asBoolean());
  EXPECT_TRUE(output["refreshEpisodes"].asBoolean());
  EXPECT_EQ("Planetes", output["title"].asString());
}
