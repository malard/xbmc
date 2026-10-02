/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIContentUtils.h"

#include "FileItem.h"
#include "ServiceBroker.h"
#include "addons/gui/GUIDialogAddonInfo.h"
#include "music/dialogs/GUIDialogMusicInfo.h"
#include "pvr/PVRManager.h"
#include "pvr/guilib/PVRGUIActionsUtils.h"
#include "video/VideoInfoTag.h"
#include "video/dialogs/GUIDialogVideoInfo.h"

using namespace KODI::UTILS::GUILIB;
using KODI::MEDIA::MediaType;

bool CGUIContentUtils::HasInfoForItem(const CFileItem& item)
{
  if (item.HasVideoInfoTag() && !item.HasPVRRecordingInfoTag())
  {
    const MediaType mediaType = item.GetVideoInfoTag()->GetMediaType();
    return (mediaType == MediaType::MOVIE || mediaType == MediaType::TV_SHOW ||
            mediaType == MediaType::SEASON || mediaType == MediaType::EPISODE ||
            mediaType == MediaType::VIDEO || mediaType == MediaType::VIDEO_COLLECTION ||
            mediaType == MediaType::MUSIC_VIDEO);
  }

  return (item.HasMusicInfoTag() || item.HasAddonInfo() ||
          CServiceBroker::GetPVRManager().Get<PVR::GUI::Utils>().HasInfoForItem(item));
}

bool CGUIContentUtils::ShowInfoForItem(const CFileItem& item)
{
  if (item.HasAddonInfo())
  {
    return CGUIDialogAddonInfo::ShowForItem(std::make_shared<CFileItem>(item));
  }
  else if (CServiceBroker::GetPVRManager().Get<PVR::GUI::Utils>().HasInfoForItem(item))
  {
    return CServiceBroker::GetPVRManager().Get<PVR::GUI::Utils>().OnInfo(item);
  }
  else if (item.HasVideoInfoTag())
  {
    CGUIDialogVideoInfo::ShowFor(item);
    return true;
  }
  else if (item.HasMusicInfoTag())
  {
    CGUIDialogMusicInfo::ShowFor(std::make_shared<CFileItem>(item).get());
    return true;
  }
  return false;
}
