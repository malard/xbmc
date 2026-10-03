/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/IAnnouncer.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/channels/PVRChannelGroupMember.h"
#include "utils/Variant.h"
#include "video/VideoInfoTag.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace ANNOUNCEMENT;
using KODI::MEDIA::MediaType;

namespace
{
//! Announces synchronously, bypassing the queue thread.
class CTestAnnouncementManager : public CAnnouncementManager
{
public:
  using CAnnouncementManager::DoAnnounce;
};

class CCapturingAnnouncer : public IAnnouncer
{
public:
  void Announce(AnnouncementFlag flag,
                const std::string& sender,
                const std::string& message,
                const CVariant& data) override
  {
    m_data = data;
  }

  CVariant m_data;
};

CVariant ItemDataFor(const std::shared_ptr<CFileItem>& item)
{
  CTestAnnouncementManager manager;
  CCapturingAnnouncer announcer;
  manager.AddAnnouncer(&announcer);
  manager.DoAnnounce(Player, CAnnouncementManager::ANNOUNCEMENT_SENDER, "OnPlay", item,
                     CVariant::VariantTypeObject);
  manager.RemoveAnnouncer(&announcer);
  return announcer.m_data["item"];
}
} // unnamed namespace

TEST(TestAnnouncementItemData, AnEpisodeCarriesItsShowTitleInCamelCase)
{
  // No database id, so the details travel with the item rather than being left to a lookup.
  CVideoInfoTag tag;
  tag.SetMediaType(MediaType::EPISODE);
  tag.m_strTitle = "Pilot";
  tag.m_strShowTitle = "The Show";
  tag.m_iSeason = 1;
  tag.m_iEpisode = 1;

  const auto episode{std::make_shared<CFileItem>(tag)};
  episode->SetProperty("database-lookup", false);

  const CVariant item{ItemDataFor(episode)};

  EXPECT_EQ("The Show", item["showTitle"].asString());
  EXPECT_FALSE(item.isMember("showtitle"));
}

TEST(TestAnnouncementItemData, AChannelCarriesItsChannelTypeInCamelCase)
{
  // TV rather than radio, and no group name: either would reach PVR services tests do not have.
  const auto member{std::make_shared<PVR::CPVRChannelGroupMember>()};
  member->SetChannel(std::make_shared<PVR::CPVRChannel>(false));

  const CVariant item{ItemDataFor(std::make_shared<CFileItem>(member))};

  EXPECT_EQ("tv", item["channelType"].asString());
  EXPECT_FALSE(item.isMember("channeltype"));
}
