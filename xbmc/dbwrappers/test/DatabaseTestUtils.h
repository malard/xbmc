/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DatabaseManager.h"
#include "ServiceBroker.h"
#include "dbwrappers/Database.h"
#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"

#include <string>

#include <gtest/gtest.h>

//! \brief Where a test database lives: an sqlite file in special://temp/.
inline DatabaseSettings TestDatabaseSettings()
{
  DatabaseSettings settings;
  settings.type = "sqlite3";
  settings.host = CSpecialProtocol::TranslatePath("special://temp/");
  return settings;
}

/*!
 * \brief Points a database slot of the advanced settings, which Open() connects to, at an empty
 * test database for the life of the guard, so code that opens the library itself can be tested.
 */
template<typename DATABASE>
class CScopedTestDatabase
{
public:
  CScopedTestDatabase(DatabaseSettings CAdvancedSettings::* slot, const std::string& baseName)
    : m_slot(slot),
      m_settings(TestDatabaseSettings()),
      m_restore(Target())
  {
    CDatabaseManager& manager{CServiceBroker::GetDatabaseManager()};
    if (!manager.CanOpen(m_db.GetBaseDBName()) && !manager.Initialize())
    {
      ADD_FAILURE() << "the database manager did not initialise";
      return;
    }

    m_settings.name = baseName;
    Target() = m_settings;
    m_settings.name += std::to_string(m_db.GetSchemaVersion());
    EXPECT_EQ(CDatabase::ConnectionState::STATE_CONNECTED,
              m_db.Connect(m_settings.name, m_settings, true));
  }

  ~CScopedTestDatabase()
  {
    Target() = m_restore;
    m_db.Close();
    XFILE::CFile::Delete(m_settings.host + m_settings.name + ".db");
  }

  CScopedTestDatabase(const CScopedTestDatabase&) = delete;
  CScopedTestDatabase& operator=(const CScopedTestDatabase&) = delete;

  DATABASE& Get() { return m_db; }

private:
  struct Database : DATABASE
  {
    using DATABASE::CDatabase::GetBaseDBName;
    using DATABASE::CDatabase::GetSchemaVersion;
  };

  DatabaseSettings& Target()
  {
    return (*CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()).*m_slot;
  }

  DatabaseSettings CAdvancedSettings::* m_slot;
  DatabaseSettings m_settings;
  DatabaseSettings m_restore;
  Database m_db;
};

/*!
 * \brief Upgrades the test database named \p baseName as Kodi does at startup: points \p slot of
 * the advanced settings at it and reinitialises CDatabaseManager, then puts both back.
 * \return whether the database manager initialised
 */
inline bool UpgradeThroughManager(DatabaseSettings CAdvancedSettings::* slot,
                                  const std::string& baseName)
{
  const auto advancedSettings{CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()};
  DatabaseSettings& target{(*advancedSettings).*slot};
  const DatabaseSettings restore{target};

  const DatabaseSettings test{TestDatabaseSettings()};
  target.type = test.type;
  target.host = test.host;
  target.name = baseName;

  CDatabaseManager& manager{CServiceBroker::GetDatabaseManager()};
  manager.Deinitialize();
  const bool initialized{manager.Initialize()};

  target = restore;
  manager.Deinitialize();

  return initialized;
}

/*!
 * \brief Restates the tables the 155 video upgrade changes as 154 defined them, so that upgrade can
 * run over a database Connect() built at the current schema.
 *
 * Restated rather than derived from the current tables, so a column added to the 155 upgrade and
 * not taken back out here cannot abort the upgrade on a duplicate.
 */
inline void RestateVideo154(CDatabase& db)
{
  ASSERT_TRUE(db.ExecuteQuery("DROP TABLE contentgeometry"));
  ASSERT_TRUE(db.ExecuteQuery("DROP TABLE settings"));
  ASSERT_TRUE(db.ExecuteQuery(
      "CREATE TABLE settings ( idFile integer, Deinterlace bool,"
      "ViewMode integer,ZoomAmount float, PixelRatio float, VerticalShift float, AudioStream "
      "integer, SubtitleStream integer,"
      "SubtitleDelay float, SubtitlesOn bool, Brightness float, Contrast float, Gamma float,"
      "VolumeAmplification float, AudioDelay float, ResumeTime integer,"
      "Sharpness float, NoiseReduction float, NonLinStretch bool, PostProcess bool,"
      "ScalingMethod integer, DeinterlaceMode integer, StereoMode integer, StereoInvert bool, "
      "VideoStream integer,"
      "TonemapMethod integer, TonemapParam float, Orientation integer, CenterMixLevel integer, "
      "Hue float, Saturation float)"));
}
