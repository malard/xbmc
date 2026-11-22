/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "interfaces/json-rpc/VideoLibrarySetSourceContent.h"
#include "utils/Variant.h"

#include <climits>
#include <optional>
#include <ostream>
#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{
CVariant MakeParams(const std::string& path,
                    const std::string& content,
                    const std::string& scraperId = "")
{
  CVariant params(CVariant::VariantTypeObject);
  params["path"] = path;
  params["content"] = content;
  if (!scraperId.empty())
  {
    params["scraperId"] = scraperId;
  }

  return params;
}

CVariant MovieParams()
{
  return MakeParams("smb://nas/Movies/", "movies", "metadata.themoviedb.org.python");
}
} // unnamed namespace

TEST(TestSetSourceContentParams, RejectsParamsItCannotApply)
{
  CVariant missingPath(CVariant::VariantTypeObject);
  missingPath["content"] = "movies";
  missingPath["scraperId"] = "metadata.themoviedb.org.python";
  CVariant emptyScraper{MakeParams("smb://nas/Movies/", "movies")};
  emptyScraper["scraperId"] = "";
  CVariant unknownClearMode{MakeParams("smb://nas/Movies/", "none")};
  unknownClearMode["clearMode"] = "purge";

  for (const auto& [rejected, params] : {
           std::pair{"a missing path", missingPath},
           std::pair{"an empty path", MakeParams("", "movies", "metadata.themoviedb.org.python")},
           std::pair{"unknown content", MakeParams("smb://nas/Media/", "audiobooks", "metadata.x")},
           // A content type, but not one a video source can be bound to
           std::pair{"music content", MakeParams("smb://nas/Music/", "albums", "metadata.x")},
           // Unless the content is none
           std::pair{"a missing scraper", MakeParams("smb://nas/Movies/", "movies")},
           std::pair{"an empty scraper", emptyScraper},
           std::pair{"an unknown clear mode", unknownClearMode},
       })
  {
    ParsedSetSourceContent parsed;
    EXPECT_EQ(InvalidParams, ParseSetSourceContentParams(params, parsed)) << rejected;
  }
}

TEST(TestSetSourceContentParams, PassesThroughScraperAndFlags)
{
  CVariant params = MovieParams();
  params["scraperSettings"] = "<settings><setting id=\"language\">en</setting></settings>";
  params["noUpdate"] = true;
  params["refresh"] = true;

  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(params, parsed));
  EXPECT_EQ("smb://nas/Movies/", parsed.path);
  EXPECT_EQ(ADDON::ContentType::MOVIES, parsed.content);
  EXPECT_EQ("metadata.themoviedb.org.python", parsed.scraperId);
  EXPECT_EQ("<settings><setting id=\"language\">en</setting></settings>", parsed.scraperSettings);
  EXPECT_TRUE(parsed.settings.noupdate);
  EXPECT_TRUE(parsed.refresh);
  EXPECT_FALSE(parsed.settings.exclude);
}

TEST(TestSetSourceContentParams, LeavesAllExternalAudioToTheCaller)
{
  // It has no parameter; the handler carries the path's stored value over.
  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(MovieParams(), parsed));
  EXPECT_FALSE(parsed.settings.m_allExtAudio);
}

namespace
{
struct ContentMapping
{
  const char* content;
  std::optional<bool> useDirectoryNames;
  std::optional<bool> scanRecursive;
  std::optional<bool> containsSingleItem;
  bool parentName;
  bool parentNameRoot;
  int recurse;
};

void PrintTo(const ContentMapping& mapping, std::ostream* os)
{
  const auto flag{[](const std::optional<bool>& value)
                  { return value ? (*value ? "true" : "false") : "unset"; }};
  *os << mapping.content << " useDirectoryNames=" << flag(mapping.useDirectoryNames)
      << " scanRecursive=" << flag(mapping.scanRecursive)
      << " containsSingleItem=" << flag(mapping.containsSingleItem);
}

// clang-format off
const ContentMapping CONTENT_MAPPINGS[] = {
    // Movies and music videos: the mapping CGUIDialogContentSettings::Show() applies
    {"movies", std::nullopt, std::nullopt, std::nullopt, false, false, INT_MAX},
    {"movies", std::nullopt, false, std::nullopt, false, false, 0},
    {"movies", true, std::nullopt, std::nullopt, true, false, INT_MAX},
    // One level, not none: a folder per movie is still a level below the source
    {"movies", true, false, std::nullopt, true, false, 1},
    {"movies", true, std::nullopt, true, true, true, 0},
    // A single item is one only in a folder named after it
    {"movies", std::nullopt, std::nullopt, true, false, false, INT_MAX},
    {"musicvideos", true, false, std::nullopt, true, false, 1},
    // TV shows: a fixed mapping, driven only by whether the path holds one show
    {"tvshows", std::nullopt, std::nullopt, std::nullopt, false, false, 0},
    {"tvshows", std::nullopt, std::nullopt, true, true, true, 0},
    {"tvshows", true, true, std::nullopt, false, false, 0},
};
// clang-format on

class TestSetSourceContentMapping : public ::testing::TestWithParam<ContentMapping>
{
};
} // unnamed namespace

TEST_P(TestSetSourceContentMapping, TheFlagsGiveTheScanSettings)
{
  const ContentMapping& mapping{GetParam()};
  CVariant params{MakeParams("smb://nas/Media/", mapping.content, "metadata.local")};
  if (mapping.useDirectoryNames)
    params["useDirectoryNames"] = *mapping.useDirectoryNames;
  if (mapping.scanRecursive)
    params["scanRecursive"] = *mapping.scanRecursive;
  if (mapping.containsSingleItem)
    params["containsSingleItem"] = *mapping.containsSingleItem;

  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(params, parsed));
  EXPECT_EQ(mapping.content, ADDON::TranslateContent(parsed.content));
  EXPECT_EQ(mapping.parentName, parsed.settings.parent_name);
  EXPECT_EQ(mapping.parentNameRoot, parsed.settings.parent_name_root);
  EXPECT_EQ(mapping.recurse, parsed.settings.recurse);
}

INSTANTIATE_TEST_SUITE_P(Contents,
                         TestSetSourceContentMapping,
                         ::testing::ValuesIn(CONTENT_MAPPINGS));

TEST(TestSetSourceContentParams, ClearsWithoutExcludingByDefault)
{
  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(MakeParams("smb://nas/Movies/", "none"), parsed));
  EXPECT_EQ(ADDON::ContentType::NONE, parsed.content);
  EXPECT_EQ(SourceContentClearMode::CLEAR, parsed.clearMode);
  EXPECT_FALSE(parsed.settings.exclude);
  EXPECT_TRUE(parsed.scraperId.empty());
}

TEST(TestSetSourceContentParams, ExcludeClearModeExcludesThePath)
{
  CVariant params = MakeParams("smb://nas/Movies/", "none");
  params["clearMode"] = "exclude";

  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(params, parsed));
  EXPECT_EQ(SourceContentClearMode::EXCLUDE, parsed.clearMode);
  EXPECT_TRUE(parsed.settings.exclude);
}

TEST(TestSetSourceContentParams, RemoveClearModeDoesNotExcludeThePath)
{
  CVariant params = MakeParams("smb://nas/Movies/", "none");
  params["clearMode"] = "remove";

  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(params, parsed));
  EXPECT_EQ(SourceContentClearMode::REMOVE, parsed.clearMode);
  EXPECT_FALSE(parsed.settings.exclude);
}

TEST(TestSetSourceContentParams, ClearingStillHonoursRefresh)
{
  CVariant params = MakeParams("smb://nas/Movies/", "none");
  params["refresh"] = true;

  ParsedSetSourceContent parsed;
  ASSERT_EQ(OK, ParseSetSourceContentParams(params, parsed));
  EXPECT_TRUE(parsed.refresh);
}
