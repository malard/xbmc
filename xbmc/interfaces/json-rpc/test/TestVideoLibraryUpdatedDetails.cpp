/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

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

TEST(TestVideoLibraryUpdatedDetails, UniqueIdIsNamedAsTheDatabaseReadsIt)
{
  CVariant parameterObject;
  parameterObject["uniqueId"]["imdb"] = "tt0000001";

  EXPECT_TRUE(UpdatedDetails(parameterObject).contains(UPDATED_DETAIL::UNIQUE_ID));
}

TEST(TestVideoLibraryUpdatedDetails, ImdbNumberIsNamedAsTheDatabaseReadsIt)
{
  CVariant parameterObject;
  parameterObject["imdbNumber"] = "tt0000001";

  EXPECT_TRUE(UpdatedDetails(parameterObject).contains(UPDATED_DETAIL::UNIQUE_ID));
}

TEST(TestVideoLibraryUpdatedDetails, ShowLinkIsNamedAsTheDatabaseReadsIt)
{
  CVariant parameterObject;
  parameterObject["showLink"].push_back("Planetes");

  EXPECT_TRUE(UpdatedDetails(parameterObject).contains(UPDATED_DETAIL::SHOW_LINK));
}

TEST(TestVideoLibraryUpdatedDetails, DateAddedIsNamedAsTheDatabaseReadsIt)
{
  CVariant parameterObject;
  parameterObject["dateAdded"] = "2026-10-08 12:00:00";

  EXPECT_TRUE(UpdatedDetails(parameterObject).contains(UPDATED_DETAIL::DATE_ADDED));
}

TEST(TestVideoLibraryUpdatedDetails, LinkTablesAreNamedAsTheDatabaseReadsThem)
{
  CVariant parameterObject;
  for (const char* field : {"genre", "studio", "country", "tag", "director", "writer"})
    parameterObject[field].push_back("value");

  const auto updated = UpdatedDetails(parameterObject);
  for (const char* detail : {UPDATED_DETAIL::GENRE, UPDATED_DETAIL::STUDIO,
                             UPDATED_DETAIL::COUNTRY, UPDATED_DETAIL::TAG,
                             UPDATED_DETAIL::DIRECTOR, UPDATED_DETAIL::WRITER})
    EXPECT_TRUE(updated.contains(detail)) << detail;
}
