/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "JSONRPCTestUtils.h"
#include "interfaces/json-rpc/VideoLibrary.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoInfoTag.h"

#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;
namespace UPDATED_DETAIL = KODI::VIDEO::UPDATED_DETAIL;

namespace
{
class CTestVideoLibrary : public CVideoLibrary
{
public:
  using CVideoLibrary::UpdateVideoTag;
};

std::set<std::string, std::less<>> UpdatedDetails(const CVariant& parameterObject)
{
  CVideoInfoTag details;
  KODI::ART::Artwork artwork;
  std::set<std::string, std::less<>> removedArtwork;
  std::set<std::string, std::less<>> updatedDetails;
  CTestVideoLibrary::UpdateVideoTag(parameterObject, details, artwork, removedArtwork,
                                    updatedDetails);
  return updatedDetails;
}
} // unnamed namespace

TEST(TestVideoLibraryUpdatedDetails, EachFieldIsNamedAsTheDatabaseReadsIt)
{
  for (const auto& [json, detail] : {
           std::pair{R"({"uniqueId": {"imdb": "tt0000001"}})", UPDATED_DETAIL::UNIQUE_ID},
           std::pair{R"({"imdbNumber": "tt0000001"})", UPDATED_DETAIL::UNIQUE_ID},
           std::pair{R"({"showLink": ["Planetes"]})", UPDATED_DETAIL::SHOW_LINK},
           std::pair{R"({"dateAdded": "2026-10-08 12:00:00"})", UPDATED_DETAIL::DATE_ADDED},
           std::pair{R"({"genre": ["value"]})", UPDATED_DETAIL::GENRE},
           std::pair{R"({"studio": ["value"]})", UPDATED_DETAIL::STUDIO},
           std::pair{R"({"country": ["value"]})", UPDATED_DETAIL::COUNTRY},
           std::pair{R"({"tag": ["value"]})", UPDATED_DETAIL::TAG},
           std::pair{R"({"director": ["value"]})", UPDATED_DETAIL::DIRECTOR},
           std::pair{R"({"writer": ["value"]})", UPDATED_DETAIL::WRITER},
       })
    EXPECT_TRUE(UpdatedDetails(ParseJson(json)).contains(detail)) << json;
}
