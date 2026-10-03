/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "URL.h"
#include "addons/Scraper.h"
#include "interfaces/json-rpc/AudioLibrary.h"

#include <string>

#include <gtest/gtest.h>

using namespace JSONRPC;

namespace
{

class TestableAudioLibrary : public CAudioLibrary
{
public:
  using CAudioLibrary::ResolveInfoProviderView;
};

// The listing without its options, so a test can say which listing was resolved without
// depending on the order the options come back in.
std::string ListingOf(const std::string& viewPath)
{
  const CURL url{viewPath};
  return url.GetWithoutOptions();
}

bool HasOption(const std::string& viewPath, const std::string& key, const std::string& value)
{
  const CURL url{viewPath};
  const auto& options = url.GetOptions();
  return options.find("?" + key + "=" + value) != std::string::npos ||
         options.find("&" + key + "=" + value) != std::string::npos;
}

bool HasNoOption(const std::string& viewPath, const std::string& key)
{
  const CURL url{viewPath};
  return url.GetOptions().find(key + "=") == std::string::npos;
}

} // namespace

TEST(TestAudioLibraryInfoProviderView, AListingDropsTheItemItNamesAndKeepsItsFilters)
{
  struct ViewCase
  {
    std::string path;
    ADDON::ContentType content;
    //! Empty to leave the listing unchecked
    std::string listing;
    //! Empty for no option that must be gone
    std::string dropped;
    //! Empty for no option that must remain
    std::string keptKey;
    std::string keptValue;
  };

  const ViewCase cases[] = {
      // The id that names one item in the listing is what the view scope drops. On an albums
      // listing that is albumid: a path naming a single album is not a view of one album.
      {"musicdb://albums/?albumid=3", ADDON::ContentType::ALBUMS, "musicdb://albums/", "albumid"},
      {"musicdb://artists/?artistid=5", ADDON::ContentType::ARTISTS, "musicdb://artists/",
       "artistid"},
      // artistid on an albums listing is a filter, not the name of a single album, and
      // CMusicDatabase::GetFilter applies it. Dropping it turns "this artist's albums" into every
      // album in the library, which is what SetScraperAll would then rewrite.
      {"musicdb://albums/?artistid=5", ADDON::ContentType::ALBUMS, "musicdb://albums/", "",
       "artistid", "5"},
      {"musicdb://artists/?albumid=3", ADDON::ContentType::ARTISTS, "", "", "albumid", "3"},
      // The same filter spelled as a path segment has to survive too - this is the form the GUI
      // navigates with. An artist's node lists that artist's albums, so the listing is albums and
      // the artist is the filter on it.
      {"musicdb://artists/5/", ADDON::ContentType::ALBUMS, "musicdb://albums/", "", "artistid",
       "5"},
      {"musicdb://genres/7/albums/", ADDON::ContentType::ALBUMS, "musicdb://albums/", "", "genreid",
       "7"},
  };

  for (const ViewCase& c : cases)
  {
    SCOPED_TRACE(c.path);
    ADDON::ContentType content{ADDON::ContentType::NONE};
    std::string viewPath;

    ASSERT_TRUE(TestableAudioLibrary::ResolveInfoProviderView(c.path, content, viewPath));

    EXPECT_EQ(c.content, content);
    if (!c.listing.empty())
      EXPECT_EQ(c.listing, ListingOf(viewPath));
    if (!c.dropped.empty())
      EXPECT_TRUE(HasNoOption(viewPath, c.dropped)) << viewPath;
    if (!c.keptKey.empty())
      EXPECT_TRUE(HasOption(viewPath, c.keptKey, c.keptValue)) << viewPath;
  }
}

// Only artists and albums carry an information provider.
TEST(TestAudioLibraryInfoProviderView, ASongsListingIsRefused)
{
  ADDON::ContentType content{ADDON::ContentType::NONE};
  std::string viewPath;

  EXPECT_FALSE(
      TestableAudioLibrary::ResolveInfoProviderView("musicdb://songs/", content, viewPath));
}

TEST(TestAudioLibraryInfoProviderView, APathThatIsNotAMusicListingIsRefused)
{
  ADDON::ContentType content{ADDON::ContentType::NONE};
  std::string viewPath;

  EXPECT_FALSE(
      TestableAudioLibrary::ResolveInfoProviderView("videodb://movies/titles/", content, viewPath));
}
