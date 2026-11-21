/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DatabaseManager.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "filesystem/File.h"
#include "filesystem/SmartPlaylistDirectory.h"
#include "filesystem/SpecialProtocol.h"
#include "playlists/SmartPlayList.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/ContentNames.h"
#include "utils/ItemProperties.h"
#include "video/VideoDatabase.h"

#include <optional>
#include <string>

#include <gtest/gtest.h>

using namespace KODI;

namespace
{
constexpr const char* DB_NAME = "TestSmartPlaylistDirectory";

class TestDatabase : public CVideoDatabase
{
public:
  using CDatabase::GetSchemaVersion;
};

//! Points the video database the settings name, which the directory opens, at a test database.
class TestSmartPlaylistDirectory : public ::testing::Test
{
protected:
  void SetUp() override
  {
    if (!CServiceBroker::GetDatabaseManager().CanOpen("MyVideos"))
    {
      ASSERT_TRUE(CServiceBroker::GetDatabaseManager().Initialize());
    }
    m_settings.type = "sqlite3";
    m_settings.name = DB_NAME;
    m_settings.host = CSpecialProtocol::TranslatePath("special://temp/");
    auto& videoDatabase{
        CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseVideo};
    m_videoDatabase = videoDatabase;
    videoDatabase = m_settings;
    m_settings.name += std::to_string(m_db.GetSchemaVersion());
    ASSERT_EQ(CDatabase::ConnectionState::STATE_CONNECTED,
              m_db.Connect(m_settings.name, m_settings, true));
  }

  void TearDown() override
  {
    if (m_videoDatabase)
    {
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_databaseVideo =
          *m_videoDatabase;
    }
    m_db.Close();
    XFILE::CFile::Delete(m_settings.host + m_settings.name + ".db");
  }

  TestDatabase m_db;
  DatabaseSettings m_settings;
  std::optional<DatabaseSettings> m_videoDatabase;
};
} // namespace

TEST_F(TestSmartPlaylistDirectory, AnUngroupedEpisodePlaylistRecordsTheEpisodesPath)
{
  PLAYLIST::CSmartPlaylist playlist;
  playlist.SetType(MEDIA::CONTENT::EPISODES);

  CFileItemList items;
  ASSERT_TRUE(XFILE::CSmartPlaylistDirectory::GetDirectory(playlist, items));
  EXPECT_EQ("videodb://tvshows/titles/-1/-1/",
            items.GetProperty(ITEM::PROPERTY::PATH_DB).asString());
}
