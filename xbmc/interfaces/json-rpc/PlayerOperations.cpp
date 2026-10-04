/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayerOperations.h"

#include "AudioLibrary.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "GUIInfoManager.h"
#include "GUIUserMessages.h"
#include "InputOperations.h"
#include "PVROperations.h"
#include "PartyMode.h"
#include "PlaybackModes.h"
#include "SeekHandler.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "Util.h"
#include "VideoLibrary.h"
#include "application/Application.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationContentGeometry.h"
#include "application/ApplicationPlayLists.h"
#include "application/ApplicationPlayer.h"
#include "application/ApplicationPowerHandling.h"
#include "application/ApplicationVolumeHandling.h"
#include "cores/playercorefactory/PlayerCoreFactory.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "interfaces/PlaybackValues.h"
#include "interfaces/builtins/Builtins.h"
#include "messaging/ApplicationMessenger.h"
#include "messaging/MessengerPayload.h"
#include "music/MusicDatabase.h"
#include "music/MusicFileItemClassify.h"
#include "music/tags/MusicInfoTag.h"
#include "pictures/SlideShowDelegator.h"
#include "playlists/PlayList.h"
#include "pvr/PVRManager.h"
#include "pvr/PVRPlaybackState.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/channels/PVRChannelGroupMember.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/epg/EpgContainer.h"
#include "pvr/epg/EpgInfoTag.h"
#include "pvr/guilib/PVRGUIActionsChannels.h"
#include "pvr/guilib/PVRGUIActionsPlayback.h"
#include "pvr/recordings/PVRRecordings.h"
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/AspectRatioVocabulary.h"
#include "utils/DatabaseUtils.h"
#include "utils/FileExtensionProvider.h"
#include "utils/ItemProperties.h"
#include "utils/MathUtils.h"
#include "utils/PlayerUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "video/VideoDatabase.h"
#include "video/VideoFileItemClassify.h"
#include "video/geometry/GeometryPublication.h"

#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>

using namespace KODI;
using namespace JSONRPC;
using namespace PVR;
using KODI::MESSAGING::TransferToMessenger;

namespace
{

std::shared_ptr<CApplicationPlayer> AppPlayer()
{
  return CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
}

std::shared_ptr<CApplicationVolumeHandling> VolumeHandling()
{
  return CServiceBroker::GetAppComponents().GetComponent<CApplicationVolumeHandling>();
}

std::shared_ptr<CApplicationContentGeometry> ContentGeometryComponent()
{
  return CServiceBroker::GetAppComponents().GetComponent<CApplicationContentGeometry>();
}

CApplicationPlayLists::Repeat ParseRepeat(const CVariant& repeat)
{
  return CApplicationPlayLists::ParseRepeat(repeat.asString())
      .value_or(CApplicationPlayLists::Repeat::Off);
}

bool IsReachable(const CFileItem& item)
{
  const std::string& path = item.GetDynPath();

  // Only plain files can be cheaply verified. Anything resolved by a plugin, served
  // over the network as a stream, or addressed as a container is left to the player.
  // Stacks in particular have no CFileFactory loader, so CFile::Exists() would always
  // report them missing.
  if (item.IsFolder() || path.empty() || URIUtils::IsPlugin(path) || URIUtils::IsUPnP(path) ||
      URIUtils::IsInternetStream(path) || URIUtils::IsBlurayPath(path) || URIUtils::IsStack(path))
    return true;

  // Bypass the directory cache; a cached hit would mask a share that has gone away.
  return XFILE::CFile::Exists(path, false);
}

//! \brief Sets \p index to the stream \p selection names among \p count: "previous" and "next"
//! step from \p current and wrap, and an integer is taken as given. Fails for other text and
//! for a stream that is not there.
JSONRPC_STATUS SelectStream(
    const CVariant& selection, int current, int count, int& index, CVariant& result)
{
  if (!selection.isString())
    index = selection.isInteger() ? static_cast<int>(selection.asInteger()) : -1;
  else if (selection.asString() == "previous")
    index = current > 0 ? current - 1 : count - 1;
  else if (selection.asString() == "next")
    index = current + 1 < count ? current + 1 : 0;
  else
    return InvalidParams;

  if (index < 0 || count <= index)
    return Fail(result, InvalidParams, Reason::NoSuchStream);

  return OK;
}

//! Every stream of one kind \p player holds, as answered
template<typename Info>
CVariant StreamList(const CApplicationPlayer& player,
                    int (CApplicationPlayer::*count)() const,
                    void (CApplicationPlayer::*info)(int, Info&) const)
{
  CVariant streams(CVariant::VariantTypeArray);
  for (int index = 0; index < (player.*count)(); ++index)
  {
    Info streamInfo;
    (player.*info)(index, streamInfo);
    streams.append(INTERFACES::StreamToObject(index, streamInfo));
  }
  return streams;
}

//! The speed \p player plays at, the "speed" property
int PlaybackSpeed(PlayerType player)
{
  if (player == PlayerType::Picture)
  {
    const CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
    return slideShow.IsPlaying() && !slideShow.IsPaused() ? slideShow.GetDirection() : 0;
  }
  const auto appPlayer = AppPlayer();
  return appPlayer->IsPausedPlayback() ? 0 : static_cast<int>(lrint(appPlayer->GetPlaySpeed()));
}

void OverlayCurrentSongTag(CFileItem& item)
{
  const MUSIC_INFO::CMusicInfoTag* current{
      CServiceBroker::GetGUI()->GetInfoManager().GetCurrentSongTag()};
  if (!current)
    return;

  // Only copy what the source actually supplied, so anything the item already carries in its
  // own right survives.
  MUSIC_INFO::CMusicInfoTag& tag{*item.GetMusicInfoTag()};
  if (!current->GetTitle().empty())
    tag.SetTitle(current->GetTitle());
  if (!current->GetArtist().empty())
    tag.SetArtist(current->GetArtist());
  if (!current->GetAlbum().empty())
    tag.SetAlbum(current->GetAlbum());
  if (!current->GetGenre().empty())
    tag.SetGenre(current->GetGenre());
  if (!current->GetStationName().empty())
    tag.SetStationName(current->GetStationName());
}

} // namespace

JSONRPC_STATUS CPlayerOperations::GetPlayers(const CVariant &parameterObject, CVariant &result)
{
  const CPlayerCoreFactory &playerCoreFactory = CServiceBroker::GetPlayerCoreFactory();

  std::string media = parameterObject["media"].asString();
  result = CVariant(CVariant::VariantTypeArray);
  std::vector<std::string> players;

  if (media == "all")
  {
    playerCoreFactory.GetPlayers(players);
  }
  else
  {
    bool video = false;
    if (media == "video")
      video = true;
    playerCoreFactory.GetPlayers(players, true, video);
  }

  for (const auto& playername : players)
  {
    CVariant player(CVariant::VariantTypeObject);
    player["name"] = playername;

    player["playsVideo"] = playerCoreFactory.PlaysVideo(playername);
    player["playsAudio"] = playerCoreFactory.PlaysAudio(playername);
    player["type"] = playerCoreFactory.GetPlayerType(playername);

    result.push_back(player);
  }

  return OK;
}

JSONRPC_STATUS CPlayerOperations::GetProperties(const CVariant& parameterObject, CVariant& result)
{
  const CVariant& playlist = parameterObject["playlist"];
  const PlayerType player = GetTarget(playlist);
  return GetNamedProperties(parameterObject, result,
                            [player, &playlist](const std::string & property, CVariant& value)
                            { return GetPropertyValue(player, property, value, playlist); });
}

JSONRPC_STATUS CPlayerOperations::GetItem(const CVariant &parameterObject, CVariant &result)
{
  PlayerType player = GetTarget(parameterObject["playlist"]);
  CFileItemPtr fileItem;

  switch (player)
  {
    case Video:
    case Audio:
    {
      fileItem = std::make_shared<CFileItem>(*g_application.CurrentFileItemPtr());
      if (IsPVRChannel())
      {
        // Metadata that arrives mid-stream reaches only the item held by the GUI, so overlay
        // it here. The channel item stays authoritative for identity, path and artwork.
        OverlayCurrentSongTag(*fileItem);
        break;
      }

      if (player == Video)
      {
        if (!CVideoLibrary::FillFileItem(fileItem->GetPath(), fileItem, parameterObject))
        {
          // Fallback to item details held by GUI but ensure path unchanged
          //! @todo  remove this once there is no route to playback that updates
          // GUI item without also updating app item e.g. start playback of a
          // non-library item via JSON
          const CVideoInfoTag *currentVideoTag = CServiceBroker::GetGUI()->GetInfoManager().GetCurrentMovieTag();
          if (currentVideoTag != nullptr)
          {
            std::string originalLabel = fileItem->GetLabel();
            std::string originalPath = fileItem->GetPath();
            fileItem->SetFromVideoInfoTag(*currentVideoTag);
            if (fileItem->GetLabel().empty())
              fileItem->SetLabel(originalLabel);
            fileItem->SetPath(originalPath);   // Ensure path unchanged
          }
        }

        bool additionalInfo = false;
        for (CVariant::const_iterator_array itr = parameterObject["properties"].begin_array();
             itr != parameterObject["properties"].end_array(); ++itr)
        {
          std::string fieldValue = itr->asString();
          if (fieldValue == "cast" || fieldValue == "set" || fieldValue == "setId" || fieldValue == "showLink" || fieldValue == "resume" ||
             (fieldValue == "streamDetails" && !fileItem->GetVideoInfoTag()->m_streamDetails.HasItems()))
            additionalInfo = true;
        }

        CVideoDatabase videodatabase;
        if ((additionalInfo) &&
            videodatabase.Open())
        {
          CVideoInfoTag& tag = *fileItem->GetVideoInfoTag();
          videodatabase.TryGetDetailsByTypeAndId(
              DatabaseUtils::MediaTypeFromVideoContentType(fileItem->GetVideoContentType()),
              tag.m_iDbId, tag, nullptr, VideoDbDetailsAll, tag.GetAssetInfo().GetId(),
              tag.m_iFileId);
        }
      }
      else // Audio
      {
        if (!CAudioLibrary::FillFileItem(fileItem->GetPath(), fileItem, parameterObject))
        {
          // Fallback to item details held by GUI but ensure path unchanged
          //! @todo  remove this once there is no route to playback that updates
          // GUI item without also updating app item e.g. start playback of a
          // non-library item via JSON
          const MUSIC_INFO::CMusicInfoTag* currentMusicTag =
              CServiceBroker::GetGUI()->GetInfoManager().GetCurrentSongTag();
          if (currentMusicTag != nullptr)
          {
            std::string originalLabel = fileItem->GetLabel();
            std::string originalPath = fileItem->GetPath();
            fileItem->SetFromMusicInfoTag(*currentMusicTag);
            if (fileItem->GetLabel().empty())
              fileItem->SetLabel(originalLabel);
            fileItem->SetPath(originalPath); // Ensure path unchanged
          }
        }

        if (MUSIC::IsMusicDb(*fileItem))
        {
          CMusicDatabase musicdb;
          CFileItemList items;
          items.Add(fileItem);
          CAudioLibrary::GetAdditionalSongDetails(parameterObject, items, musicdb);
        }
      }
      break;
    }

    case Picture:
    {
      CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      CFileItemList slides;
      slideShow.GetSlideShowContents(slides);
      fileItem = slides[slideShow.CurrentSlide() - 1];
      break;
    }

    case None:
    default:
      return FailedToExecute;
  }

  HandleFileItem("id", !IsPVRChannel(), "item", fileItem, parameterObject, parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CPlayerOperations::SetProperties(const CVariant &parameterObject, CVariant &result)
{
  const CVariant& properties = parameterObject["properties"];
  const auto volume = VolumeHandling();

  if (!properties["volume"].isNull())
  {
    const int wanted = static_cast<int>(properties["volume"].asInteger());
    const bool up = wanted > static_cast<int>(volume->GetVolumePercent());
    volume->SetVolume(static_cast<float>(wanted), true);
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_VOLUME_SHOW,
                                               up ? ACTION_VOLUME_UP : ACTION_VOLUME_DOWN);
  }
  if (!properties["muted"].isNull() && properties["muted"].asBoolean() != volume->IsMuted())
    CServiceBroker::GetAppMessenger()->SendMsg(
        TMSG_GUI_ACTION, WINDOW_INVALID, -1,
        TransferToMessenger(std::make_unique<CAction>(ACTION_MUTE)));

  CVariant named(CVariant::VariantTypeObject);
  named["properties"] = CVariant(CVariant::VariantTypeArray);
  for (auto property = properties.begin_map(); property != properties.end_map(); ++property)
  {
    if (!property->second.isNull())
      named["properties"].push_back(property->first);
  }
  return GetNamedProperties(named, result, [](const std::string& property, CVariant& value)
                            { return GetPropertyValue(None, property, value); });
}

JSONRPC_STATUS CPlayerOperations::VolumeUp(const CVariant& parameterObject, CVariant& result)
{
  return StepVolume(ACTION_VOLUME_UP, result);
}

JSONRPC_STATUS CPlayerOperations::VolumeDown(const CVariant& parameterObject, CVariant& result)
{
  return StepVolume(ACTION_VOLUME_DOWN, result);
}

JSONRPC_STATUS CPlayerOperations::StepVolume(int action, CVariant& result)
{
  if (const JSONRPC_STATUS status = CInputOperations::SendAction(action, false, true);
      status != ACK && status != OK)
    return status;

  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_VOLUME_SHOW, action);
  return GetPropertyValue(None, "volume", result["volume"]);
}

JSONRPC_STATUS CPlayerOperations::PlayPause(const CVariant& parameterObject, CVariant& result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           case Audio:
                           {
                             const auto appPlayer = AppPlayer();
                             if (!appPlayer->CanPause())
                               return Fail(result, FailedToExecute, Reason::NotPausable);

                             if (parameterObject["play"].isString())
                               CBuiltins::GetInstance().Execute("playercontrol(play)");
                             else
                             {
                               if (parameterObject["play"].asBoolean())
                               {
                                 if (appPlayer->IsPausedPlayback())
                                   CServiceBroker::GetAppMessenger()->SendMsg(TMSG_MEDIA_PAUSE);
                                 else if (appPlayer->GetPlaySpeed() != 1)
                                   appPlayer->SetPlaySpeed(1);
                               }
                               else if (!appPlayer->IsPausedPlayback())
                                 CServiceBroker::GetAppMessenger()->SendMsg(TMSG_MEDIA_PAUSE);
                             }
                             result["speed"] = PlaybackSpeed(player);
                             return OK;
                           }
                           case Picture:
                           {
                             CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
                             if (slideShow.IsPlaying() && (parameterObject["play"].isString() ||
                                  (parameterObject["play"].isBoolean() &&
                                   parameterObject["play"].asBoolean() == slideShow.IsPaused())))
                               SendSlideshowAction(ACTION_PAUSE);

                             result["speed"] = PlaybackSpeed(player);
                             return OK;
                           }
                           case None:
                           default:
                             return FailedToExecute;
                         }
                       });
}

JSONRPC_STATUS CPlayerOperations::Stop(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(
      parameterObject, result,
      [&](PlayerType player) -> JSONRPC_STATUS
      {
        switch (player)
        {
          case Video:
          case Audio:
            CServiceBroker::GetAppMessenger()->PostMsg(
                TMSG_MEDIA_STOP, static_cast<int>(player == Video ? CApplication::PlaybackWindow::Video
                                                 : CApplication::PlaybackWindow::Visualisation));
            return ACK;

          case Picture:
            SendSlideshowAction(ACTION_STOP);
            return ACK;

          case None:
          default:
            return FailedToExecute;
        }
      });
}

JSONRPC_STATUS CPlayerOperations::GetAudioDelay(const CVariant& parameterObject,
                                                CVariant& result)
{
  const auto appPlayer = AppPlayer();
  result["offset"] = appPlayer->GetVideoSettings().m_AudioDelay;
  return OK;
}

JSONRPC_STATUS CPlayerOperations::NotifyAudioChainReady(const CVariant& parameterObject,
                                                CVariant& result)
{
  AppPlayer()->NotifyAudioChainReady();
  return ACK;
}

JSONRPC_STATUS CPlayerOperations::SetAudioDelay(const CVariant& parameterObject, CVariant& result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           {
                             const auto appPlayer = AppPlayer();
                             float videoAudioDelayRange = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_videoAudioDelayRange;
                             float videoAudioDelayStep = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_videoAudioDelayStep;

                             if (parameterObject["offset"].isDouble())
                             {
                               float offset = static_cast<float>(parameterObject["offset"].asDouble());
                               offset = MathUtils::RoundF(offset, videoAudioDelayStep);
                               if (offset > videoAudioDelayRange)
                                 offset = videoAudioDelayRange;
                               else if (offset < -videoAudioDelayRange)
                                 offset = -videoAudioDelayRange;

                               appPlayer->SetAVDelay(offset);
                             }
                             else if (parameterObject["offset"].isString())
                             {
                               CVideoSettings vs = appPlayer->GetVideoSettings();
                               if (parameterObject["offset"].asString().compare("increment") == 0)
                               {
                                 vs.m_AudioDelay += videoAudioDelayStep;
                                 if (vs.m_AudioDelay > videoAudioDelayRange)
                                   vs.m_AudioDelay = videoAudioDelayRange;
                                 appPlayer->SetAVDelay(vs.m_AudioDelay);
                               }
                               else
                               {
                                 vs.m_AudioDelay -= videoAudioDelayStep;
                                 if (vs.m_AudioDelay < -videoAudioDelayRange)
                                   vs.m_AudioDelay = -videoAudioDelayRange;
                                 appPlayer->SetAVDelay(vs.m_AudioDelay);
                               }
                             }
                             else
                               return InvalidParams;

                             result["offset"] = appPlayer->GetVideoSettings().m_AudioDelay;
                             return OK;
                           }
                           case Audio:
                           case Picture:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           case None:
                           default:
                             return FailedToExecute;
                         }
                       });
}

JSONRPC_STATUS CPlayerOperations::SetSpeed(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           case Audio:
                           {
                             const auto appPlayer = AppPlayer();
                             if (parameterObject["speed"].isInteger())
                             {
                               int speed = static_cast<int>(parameterObject["speed"].asInteger());
                               if (speed != 0)
                               {
                                 // If the player is paused we first need to unpause
                                 if (appPlayer->IsPausedPlayback())
                                   appPlayer->Pause();
                                 appPlayer->SetPlaySpeed(speed);
                               }
                               else
                                 appPlayer->Pause();
                             }
                             else if (parameterObject["speed"].isString())
                             {
                               if (parameterObject["speed"].asString().compare("increment") == 0)
                                 CBuiltins::GetInstance().Execute("playercontrol(forward)");
                               else
                                 CBuiltins::GetInstance().Execute("playercontrol(rewind)");
                             }
                             else
                               return InvalidParams;

                             result["speed"] = PlaybackSpeed(player);
                             return OK;
                           }

                           case Picture:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           case None:
                           default:
                             return FailedToExecute;
                         }
                       });
}

JSONRPC_STATUS CPlayerOperations::SetTempo(const CVariant& parameterObject,
                                           CVariant& result)
{
  return ForEachTarget(
      parameterObject, result,
      [&](PlayerType player) -> JSONRPC_STATUS
      {
        switch (player)
        {
          case Video:
          case Audio:
          {
            const auto appPlayer = AppPlayer();
            if (!appPlayer->SupportsTempo())
              return Fail(result, FailedToExecute, Reason::TempoUnsupported);
            if (appPlayer->IsPausedPlayback())
              return Fail(result, FailedToExecute, Reason::Paused);

            if (parameterObject["tempo"].isDouble())
              appPlayer->SetTempo(parameterObject["tempo"].asFloat());
            else if (parameterObject["tempo"].isString())
            {
              if (parameterObject["tempo"].asString().compare("increment") == 0)
                CPlayerUtils::AdvanceTempoStep(*appPlayer, TempoStepChange::INCREASE);
              else
                CPlayerUtils::AdvanceTempoStep(*appPlayer, TempoStepChange::DECREASE);
            }
            else
              return InvalidParams;

            result["tempo"] = appPlayer->GetPlayTempo();
            return OK;
          }

          case Picture:
            return Fail(result, FailedToExecute, Reason::NotApplicable);
          case None:
          default:
            return FailedToExecute;
        }
      });
}

namespace
{
double ParseTimeInSeconds(const CVariant& time)
{
  double seconds = 0.0;
  if (time.isMember("hours"))
    seconds += time["hours"].asInteger() * 60 * 60;
  if (time.isMember("minutes"))
    seconds += time["minutes"].asInteger() * 60;
  if (time.isMember("seconds"))
    seconds += time["seconds"].asInteger();
  if (time.isMember("milliseconds"))
    seconds += time["milliseconds"].asDouble() / 1000.0;

  return seconds;
}

void HandleResumeOption(const CVariant& optionResume, CFileItem& item)
{
  if (optionResume.isBoolean() && optionResume.asBoolean())
    item.SetStartOffset(STARTOFFSET_RESUME);
  else if (optionResume.isDouble())
    item.SetProperty(ITEM::PROPERTY::START_PERCENT, optionResume);
  else if (optionResume.isObject())
    item.SetStartOffset(CUtil::ConvertSecsToMilliSecs(ParseTimeInSeconds(optionResume)));
}

JSONRPC_STATUS PlayRecording(const std::shared_ptr<CPVRRecording>& recording,
                             const CVariant& optionResume,
                             CVariant& result)
{
  CFileItem item{recording};
  HandleResumeOption(optionResume, item);
  if (!CServiceBroker::GetPVRManager().Get<PVR::GUI::Playback>().PlayMedia(item))
    return Fail(result, FailedToExecute, Reason::PlaybackRefused);

  return ACK;
}
} // unnamed namespace

JSONRPC_STATUS CPlayerOperations::Seek(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(
      parameterObject, result,
      [&](PlayerType player) -> JSONRPC_STATUS
      {
        switch (player)
        {
          case Video:
          case Audio:
          {
            const auto appPlayer = AppPlayer();
            if (!appPlayer->CanSeek())
              return Fail(result, FailedToExecute, Reason::NotSeekable);

            const CVariant& value = parameterObject["value"];
            if (value.isMember("percentage"))
              g_application.SeekPercentage(value["percentage"].asFloat());
            else if (value.isMember("step"))
            {
              std::string step = value["step"].asString();
              if (step == "smallForward")
                CBuiltins::GetInstance().Execute("playercontrol(smallskipforward)");
              else if (step == "smallBackward")
                CBuiltins::GetInstance().Execute("playercontrol(smallskipbackward)");
              else if (step == "bigForward")
                CBuiltins::GetInstance().Execute("playercontrol(bigskipforward)");
              else if (step == "bigBackward")
                CBuiltins::GetInstance().Execute("playercontrol(bigskipbackward)");
              else
                return InvalidParams;
            }
            else if (value.isMember("seconds"))
              appPlayer->GetSeekHandler().SeekSeconds(static_cast<int>(value["seconds"].asInteger()));
            else if (value.isMember("time"))
              g_application.SeekTime(ParseTimeInSeconds(value["time"]));
            else
              return InvalidParams;

            GetPropertyValue(player, "percentage", result["percentage"]);
            GetPropertyValue(player, "time", result["time"]);
            GetPropertyValue(player, "totalTime", result["totalTime"]);
            return OK;
          }

          case Picture:
            return Fail(result, FailedToExecute, Reason::NotApplicable);
          case None:
          default:
            return FailedToExecute;
        }
      });
}

JSONRPC_STATUS CPlayerOperations::Move(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(
      parameterObject, result,
      [&](PlayerType player) -> JSONRPC_STATUS
      {
        std::string direction = parameterObject["direction"].asString();
        switch (player)
        {
          case Picture:
            if (direction == "left")
              SendSlideshowAction(ACTION_MOVE_LEFT);
            else if (direction == "right")
              SendSlideshowAction(ACTION_MOVE_RIGHT);
            else if (direction == "up")
              SendSlideshowAction(ACTION_MOVE_UP);
            else if (direction == "down")
              SendSlideshowAction(ACTION_MOVE_DOWN);
            else
              return InvalidParams;

            return ACK;

          case Video:
          case Audio:
            if (direction == "left" || direction == "up")
              CServiceBroker::GetAppMessenger()->SendMsg(
                  TMSG_GUI_ACTION, WINDOW_INVALID, -1,
                  TransferToMessenger(std::make_unique<CAction>(ACTION_PREV_ITEM)));
            else if (direction == "right" || direction == "down")
              CServiceBroker::GetAppMessenger()->SendMsg(
                  TMSG_GUI_ACTION, WINDOW_INVALID, -1,
                  TransferToMessenger(std::make_unique<CAction>(ACTION_NEXT_ITEM)));
            else
              return InvalidParams;

            return ACK;

          case None:
          default:
            return FailedToExecute;
        }
      });
}

JSONRPC_STATUS CPlayerOperations::Zoom(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         CVariant zoom = parameterObject["zoom"];
                         switch (player)
                         {
                           case Picture:
                             if (zoom.isInteger())
                               SendSlideshowAction(ACTION_ZOOM_LEVEL_NORMAL + (static_cast<int>(zoom.asInteger()) - 1));
                             else if (zoom.isString())
                             {
                               std::string strZoom = zoom.asString();
                               if (strZoom == "in")
                                 SendSlideshowAction(ACTION_ZOOM_IN);
                               else if (strZoom == "out")
                                 SendSlideshowAction(ACTION_ZOOM_OUT);
                               else
                                 return InvalidParams;
                             }
                             else
                               return InvalidParams;

                             return ACK;

                           case Video:
                           case Audio:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           case None:
                           default:
                             return FailedToExecute;
                         }
                       });
}

// Matching pairs of values from JSON type "Player.ViewMode" and C++ enum ViewMode
// Additions to enum ViewMode need to be added here and in the JSON type
std::map<std::string, ViewMode> viewModes =
{
  {"normal", ViewModeNormal},
  {"zoom", ViewModeZoom},
  {"stretch4x3", ViewModeStretch4x3},
  {"widezoom", ViewModeWideZoom, },
  {"stretch16x9", ViewModeStretch16x9},
  {"original", ViewModeOriginal},
  {"stretch16x9nonlin", ViewModeStretch16x9Nonlin},
  {"zoom120width", ViewModeZoom120Width},
  {"zoom110width", ViewModeZoom110Width}
};

std::string GetStringFromViewMode(ViewMode viewMode)
{
  std::string result = "custom";

  auto it = find_if(viewModes.begin(), viewModes.end(), [viewMode](const std::pair<std::string, ViewMode> & p)
  {
    return p.second == viewMode;
  });

  if (it != viewModes.end())
  {
    std::pair<std::string, ViewMode> value = *it;
    result = value.first;
  }

  return result;
}

void GetNewValueForViewModeParameter(const CVariant &parameter, float stepSize, float minValue, float maxValue, float &result)
{
  if (parameter.isDouble())
  {
    result = parameter.asDouble();
  }
  else if (parameter.isString())
  {
    if (parameter == "decrease")
    {
      stepSize *= -1;
    }

    result += stepSize;
  }

  result = std::max(minValue, std::min(result, maxValue));
}

JSONRPC_STATUS CPlayerOperations::SetViewMode(const CVariant &parameterObject, CVariant &result)
{
  JSONRPC_STATUS jsonStatus = InvalidParams;
  // init with current values from settings
  const auto appPlayer = AppPlayer();

  CVideoSettings vs = appPlayer->GetVideoSettings();
  ViewMode mode = ViewModeNormal;

  CVariant viewMode = parameterObject["viewMode"];
  if (viewMode.isString())
  {
    std::string modestr = viewMode.asString();
    if (viewModes.contains(modestr))
    {
      mode = viewModes[modestr];
      jsonStatus = ACK;
    }
  }
  else if (viewMode.isObject())
  {
    mode = ViewModeCustom;
    CVariant zoom = viewMode["zoom"];
    CVariant pixelRatio = viewMode["pixelRatio"];
    CVariant verticalShift = viewMode["verticalShift"];
    CVariant stretch = viewMode["nonlinearStretch"];

    if (!zoom.isNull())
    {
      GetNewValueForViewModeParameter(zoom, 0.01f, 0.5f, 2.f, vs.m_CustomZoomAmount);
      jsonStatus = ACK;
    }

    if (!pixelRatio.isNull())
    {
      GetNewValueForViewModeParameter(pixelRatio, 0.01f, 0.5f, 2.f, vs.m_CustomPixelRatio);
      jsonStatus = ACK;
    }

    if (!verticalShift.isNull())
    {
      GetNewValueForViewModeParameter(verticalShift, -0.01f, -2.f, 2.f, vs.m_CustomVerticalShift);
      jsonStatus = ACK;
    }

    if (stretch.isBoolean())
    {
      vs.m_CustomNonLinStretch = stretch.asBoolean();
      jsonStatus = ACK;
    }
  }

  if (jsonStatus == ACK)
  {
    appPlayer->SetRenderViewMode(static_cast<int>(mode), vs.m_CustomZoomAmount,
                                 vs.m_CustomPixelRatio, vs.m_CustomVerticalShift,
                                 vs.m_CustomNonLinStretch);
  }

  return jsonStatus;
}

JSONRPC_STATUS CPlayerOperations::SetGeometry(const CVariant &parameterObject, CVariant &result)
{
  const auto contentGeometry = ContentGeometryComponent();
  if (!contentGeometry)
    return FailedToExecute;

  KODI::VIDEO::GEOMETRY::GeometryOverrides overrides = contentGeometry->GetOverrides();
  KODI::VIDEO::GEOMETRY::ParseGeometryOverrides(parameterObject["geometry"], overrides);
  contentGeometry->SetOverrides(overrides);

  return ACK;
}

JSONRPC_STATUS CPlayerOperations::GetGeometry(const CVariant& parameterObject, CVariant& result)
{
  const auto contentGeometry = ContentGeometryComponent();
  if (!contentGeometry)
    return FailedToExecute;

  KODI::VIDEO::GEOMETRY::SerializeGeometryOverrides(contentGeometry->GetOverrides(),
                                                    result["stated"]);

  result["raster"] = KODI::VIDEO::GEOMETRY::PublishedAspect(contentGeometry->RasterAspect());
  result["osdPlacement"] =
      KODI::VIDEO::GEOMETRY::OsdPlacementName(contentGeometry->OsdPlacementInForce());

  // Absent while nothing is drawn.
  const KODI::VIDEO::GEOMETRY::DrawnGeometry drawn = contentGeometry->Drawn();
  if (drawn.Drawn())
    KODI::VIDEO::GEOMETRY::SerializeDrawnGeometry(drawn, result["screen"]);

  return OK;
}

JSONRPC_STATUS CPlayerOperations::GetViewMode(const CVariant& parameterObject, CVariant& result)
{
  const auto appPlayer = AppPlayer();

  int mode = appPlayer->GetVideoSettings().m_ViewMode;

  result["viewMode"] = GetStringFromViewMode(static_cast<ViewMode>(mode));

  result["zoom"] = CDisplaySettings::GetInstance().GetZoomAmount();
  result["pixelRatio"] = CDisplaySettings::GetInstance().GetPixelRatio();
  result["verticalShift"] = CDisplaySettings::GetInstance().GetVerticalShift();
  result["nonlinearStretch"] = CDisplaySettings::GetInstance().IsNonLinearStretched();
  return OK;
}

JSONRPC_STATUS CPlayerOperations::SetDeclaredAspectRatio(const CVariant &parameterObject, CVariant &result)
{
  const auto appPlayer = AppPlayer();

  if (!appPlayer || !appPlayer->IsPlayingVideo())
    return Fail(result, FailedToExecute,
                appPlayer && appPlayer->IsPlaying() ? Reason::NotApplicable
                                                    : Reason::NothingPlaying);

  const CVariant& ratio = parameterObject["aspectRatio"];

  if (ratio.isString())
  {
    if (ratio.asString() != "auto")
      return InvalidParams;

    ContentGeometryComponent()->ApplyDeclaredAspect(*appPlayer, 0.0f);
    return ACK;
  }

  // Anything further than the tolerance from a ratio Kodi knows is a bad request.
  const std::optional<KODI::UTILS::AspectRatioEntry> entry =
      KODI::UTILS::CAspectRatioVocabulary::Match(static_cast<float>(ratio.asDouble()),
                                                 KODI::UTILS::AspectRatioUse::Declare);
  if (!entry)
    return InvalidParams;

  ContentGeometryComponent()->ApplyDeclaredAspect(*appPlayer, entry->ratio);
  return ACK;
}

JSONRPC_STATUS CPlayerOperations::GetDeclaredAspectRatio(const CVariant& parameterObject,
                                                         CVariant& result)
{
  const auto appPlayer = AppPlayer();

  const CVideoSettings vs = appPlayer->GetVideoSettings();

  if (vs.m_declaredAspect <= 0.0f)
  {
    // "Auto" is the absence of a declaration, so there is no ratio to report.
    result["declared"] = false;
    return OK;
  }

  const std::optional<KODI::UTILS::AspectRatioEntry> entry =
      KODI::UTILS::CAspectRatioVocabulary::Nearest(vs.m_declaredAspect);

  result["declared"] = true;
  result["aspectRatio"] = KODI::VIDEO::GEOMETRY::PublishedAspect(vs.m_declaredAspect);
  result["label"] = entry ? entry->label : "";
  result["name"] = entry ? entry->name : "";
  result["declaredOn"] = vs.m_declaredOn;
  return OK;
}

JSONRPC_STATUS CPlayerOperations::Rotate(const CVariant& parameterObject, CVariant& result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Picture:
                             if (parameterObject["value"].asString().compare("clockwise") == 0)
                               SendSlideshowAction(ACTION_ROTATE_PICTURE_CW);
                             else
                               SendSlideshowAction(ACTION_ROTATE_PICTURE_CCW);
                             return ACK;

                           case Video:
                           case Audio:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           case None:
                           default:
                             return FailedToExecute;
                         }
                       });
}

JSONRPC_STATUS CPlayerOperations::Open(const CVariant &parameterObject, CVariant &result)
{
  auto& pvrManager{CServiceBroker::GetPVRManager()};
  CVariant options = parameterObject["options"];
  CVariant optionShuffled = options["shuffled"];
  CVariant optionRepeat = options["repeat"];
  CVariant optionResume = options["resume"];

  if (const auto contentGeometry = ContentGeometryComponent(); contentGeometry)
  {
    KODI::VIDEO::GEOMETRY::GeometryOverrides overrides;
    if (options.isMember("geometry"))
      KODI::VIDEO::GEOMETRY::ParseGeometryOverrides(options["geometry"], overrides);
    contentGeometry->SetPendingOverrides(overrides);
  }

  if (parameterObject["item"].isMember("playlist"))
  {
    const CVariant& named = parameterObject["item"]["playlist"];
    const std::optional<PLAYLIST::Type> type = PLAYLIST::TypeFromName(named.asString());
    const int playlistStartPosition =
        static_cast<int>(parameterObject["item"]["position"].asInteger());

    if (type)
    {
      if (optionShuffled.isBoolean())
        CServiceBroker::GetPlayLists()->SetShuffle(*type, optionShuffled.asBoolean(),
                                                   CApplicationPlayLists::Persist::No);
      const std::string repeat =
          optionRepeat.isNull()
              ? ""
              : std::string(CApplicationPlayLists::RepeatName(ParseRepeat(optionRepeat)));
      CServiceBroker::GetAppMessenger()->PostMsg(TMSG_MEDIA_PLAY_PLAYLIST, static_cast<int>(*type),
                                                   playlistStartPosition, nullptr, repeat);
      return ACK;
    }

    if (named.asString() != "picture")
      return InvalidParams;

    std::string firstPicturePath;
    if (playlistStartPosition > 0)
    {
      CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      CFileItemList list;
      slideShow.GetSlideShowContents(list);
      if (playlistStartPosition < list.Size())
        firstPicturePath = list.Get(playlistStartPosition)->GetPath();
    }
    return StartSlideshow("", false, optionShuffled.isBoolean() && optionShuffled.asBoolean(), firstPicturePath);
  }
  else if (parameterObject["item"].isMember("path"))
  {
    const std::string path{parameterObject["item"]["path"].asString()};
    const bool recursive{parameterObject["item"]["recursive"].asBoolean()};

    // Only a directory holding pictures is a slideshow; one holding none plays as a playlist.
    CFileItemList pictures;
    CFileItemList media;
    if (ListSlideshowDirectory(path, recursive, pictures, media) && pictures.IsEmpty() &&
        !media.IsEmpty())
      return PlayFileItemList(media, options, result);

    bool random = (optionShuffled.isBoolean() && optionShuffled.asBoolean()) ||
                  (!optionShuffled.isBoolean() && parameterObject["item"]["random"].asBoolean());
    return StartSlideshow(path, recursive, random);
  }
  else if (parameterObject["item"].isObject() && parameterObject["item"].isMember("partyMode"))
  {
    const std::string partymode = parameterObject["item"]["partyMode"].asString();
    const std::optional<PLAYLIST::Type> type = PLAYLIST::TypeFromName(partymode);
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_PLAYLISTPLAYER_PARTYMODE, 1,
                                               type ? static_cast<int>(*type) : -1, nullptr,
                                               type ? "" : partymode);
    return ACK;
  }
  else if (parameterObject["item"].isMember("broadcastId"))
  {
    std::shared_ptr<CPVREpgInfoTag> epgTag;
    if (const JSONRPC_STATUS status = CPVROperations::FindBroadcast(
            "broadcastId", parameterObject["item"]["broadcastId"], epgTag, result);
        status != OK)
      return status;

    if (!epgTag->IsPlayable())
      return InvalidParams;

    if (!pvrManager.Get<PVR::GUI::Playback>().PlayEpgTag(CFileItem(epgTag)))
      return Fail(result, FailedToExecute, Reason::PlaybackRefused);

    return ACK;
  }
  else if (parameterObject["item"].isMember("channelId"))
  {
    std::shared_ptr<CPVRChannel> channel;
    if (const JSONRPC_STATUS status = CPVROperations::FindChannel(
            "channelId", parameterObject["item"]["channelId"], channel, result);
        status != OK)
      return status;

    const std::shared_ptr<CPVRChannelGroupMember> groupMember =
        pvrManager.Get<PVR::GUI::Channels>().GetChannelGroupMember(channel);
    if (!groupMember)
      return InvalidParams;

    if (!pvrManager.Get<PVR::GUI::Playback>().PlayMedia(
            CFileItem(groupMember)))
      return Fail(result, FailedToExecute, Reason::PlaybackRefused);

    return ACK;
  }
  else if (parameterObject["item"].isMember("recordingId"))
  {
    std::shared_ptr<CPVRRecording> recording;
    if (const JSONRPC_STATUS status = CPVROperations::FindRecording(
            "recordingId", parameterObject["item"]["recordingId"], recording, result);
        status != OK)
      return status;

    return PlayRecording(recording, optionResume, result);
  }

  CFileItemList list;
  if (!FillFileItemList(parameterObject["item"], list) || list.IsEmpty())
    return DiagnoseUnresolvedItem(parameterObject["item"], result);

  bool slideshow = true;
  for (int index = 0; index < list.Size(); index++)
  {
    if (!list[index]->IsPicture())
    {
      slideshow = false;
      break;
    }
  }

  if (slideshow)
  {
    //! @todo: This should be a delegator method instead of going via GUI!
    //! look into triggering stop from Reset() itself!
    SendSlideshowAction(ACTION_STOP);
    CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
    slideShow.Reset();
    for (int index = 0; index < list.Size(); index++)
      slideShow.Add(list[index].get());

    return StartSlideshow("", false, optionShuffled.isBoolean() && optionShuffled.asBoolean());
  }

  if (list.Size() == 1 && URIUtils::IsPVRChannel(list[0]->GetPath()))
  {
    if (!pvrManager.IsStarted())
      return Fail(result, FailedToExecute, Reason::PvrNotStarted);
    if (!pvrManager.Get<PVR::GUI::Playback>().PlayMedia(*list[0]))
      return Fail(result, FailedToExecute, Reason::PlaybackRefused);

    return ACK;
  }

  if (list.Size() == 1 && URIUtils::IsPVRRecording(list[0]->GetPath()))
  {
    const std::shared_ptr<const CPVRRecordings> recordingsContainer{pvrManager.Recordings()};
    if (!pvrManager.IsStarted() || !recordingsContainer)
      return Fail(result, FailedToExecute, Reason::PvrNotStarted);

    std::shared_ptr<CPVRRecording> recording{list[0]->GetPVRRecordingInfoTag()};
    if (!recording)
      recording = recordingsContainer->GetByPath(list[0]->GetPath());

    if (!recording)
      return Fail(result, NotFound, Reason::NoSuchPath,
                  Target("file", parameterObject["item"]["file"]));

    return PlayRecording(recording, optionResume, result);
  }

  return PlayFileItemList(list, options, result);
}

JSONRPC_STATUS CPlayerOperations::PlayFileItemList(CFileItemList& list,
                                                   const CVariant& options,
                                                   CVariant& result)
{
  const CVariant& optionShuffled = options["shuffled"];
  const CVariant& optionRepeat = options["repeat"];
  const CVariant& optionResume = options["resume"];
  const CVariant& optionPlayer = options["playerName"];

  std::string playername;
  if (!optionPlayer.isNull())
  {
    if (optionPlayer.isString())
    {
      playername = optionPlayer.asString();

      if (playername != "default")
      {
        const CPlayerCoreFactory &playerCoreFactory = CServiceBroker::GetPlayerCoreFactory();

        // check if the there's actually a player with the given name
        if (playerCoreFactory.GetPlayerType(playername).empty())
          return InvalidParams;

        // check if the player can handle at least the first item in the list
        std::vector<std::string> possiblePlayers;
        playerCoreFactory.GetPlayers(*list.Get(0).get(), possiblePlayers);

        bool match = false;
        for (const auto& entry : possiblePlayers)
        {
          if (StringUtils::EqualsNoCase(entry, playername))
          {
            match = true;
            break;
          }
        }
        if (!match)
          return InvalidParams;
      }
    }
    else
      return InvalidParams;
  }

  // Handle "shuffled" option
  if (optionShuffled.isBoolean())
    list.SetProperty("shuffled", optionShuffled);
  // Handle "repeat" option
  if (!optionRepeat.isNull())
    list.SetProperty("repeat", optionRepeat);
  // Handle "resume" option
  if (list.Size() == 1)
    HandleResumeOption(optionResume, *list[0]);

  // Playback is posted asynchronously; nothing after this point can be reported to the caller.
  if (list.Size() == 1 && !IsReachable(*list[0]))
    return Fail(result, Unavailable, Reason::Unreachable,
                Target("file", CURL(list[0]->GetDynPath()).GetWithoutUserDetails()));

  auto items = std::make_unique<CFileItemList>();
  items->Copy(list);
  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_MEDIA_PLAY_ITEMS, -1, -1,
                                             TransferToMessenger(std::move(items)), playername);

  return ACK;
}

bool CPlayerOperations::ListSlideshowDirectory(const std::string& path,
                                               bool recursive,
                                               CFileItemList& pictures,
                                               CFileItemList& media)
{
  const CFileExtensionProvider& extensions = CServiceBroker::GetFileExtensionProvider();
  const std::string mask{extensions.GetPictureExtensions() + "|" + extensions.GetVideoExtensions() +
                         "|" + extensions.GetMusicExtensions()};

  CFileItemList items;
  if (!XFILE::CDirectory::GetDirectory(path, items, mask, XFILE::DIR_FLAG_NO_FILE_DIRS))
    return false;

  items.Sort(SortBy::FILE, SortOrder::ASCENDING);

  // one picture makes the directory a slideshow, and the slideshow window lists the rest itself
  for (const auto& item : items)
  {
    if (item->IsFolder())
    {
      if (recursive)
        ListSlideshowDirectory(item->GetPath(), true, pictures, media);
    }
    else if (item->IsPicture())
      pictures.Add(item);
    else if (VIDEO::IsVideo(*item) || MUSIC::IsAudio(*item))
      media.Add(item);

    if (!pictures.IsEmpty())
      return true;
  }

  return true;
}

JSONRPC_STATUS CPlayerOperations::GoTo(const CVariant &parameterObject, CVariant &result)
{
  const auto appMessenger{CServiceBroker::GetAppMessenger()};
  return ForEachOnList(
      parameterObject, result,
      [&](PlayerType player) -> JSONRPC_STATUS
      {
        CVariant to = parameterObject["to"];
        switch (player)
        {
          case Video:
          case Audio:
          {
            // the playing playlist moves through the whole previous and next order, another
            // named one is moved directly
            const std::optional<PLAYLIST::Type> named =
                PLAYLIST::TypeFromName(parameterObject["playlist"].asString());
            const bool elsewhere =
                named && CServiceBroker::GetPlayLists()->GetPlayingType() != named;
            if (to.isString())
            {
              const std::string strTo = to.asString();
              if (strTo != "previous" && strTo != "next")
                return InvalidParams;

              if (elsewhere)
                appMessenger->SendMsg(strTo == "next" ? TMSG_PLAYLISTPLAYER_NEXT
                                                      : TMSG_PLAYLISTPLAYER_PREV,
                                      static_cast<int>(*named), -1, nullptr);
              else
                appMessenger->SendMsg(TMSG_GUI_ACTION, WINDOW_INVALID, -1,
                                      TransferToMessenger(std::make_unique<CAction>(
                                          strTo == "next" ? ACTION_NEXT_ITEM : ACTION_PREV_ITEM)));
            }
            else if (to.isInteger())
            {
              if (IsPVRChannel() && !elsewhere)
                appMessenger->SendMsg(
                    TMSG_GUI_ACTION, WINDOW_INVALID, -1,
                    TransferToMessenger(std::make_unique<CAction>(ACTION_CHANNEL_SWITCH, static_cast<float>(to.asInteger()))));
              else
                appMessenger->SendMsg(TMSG_PLAYLISTPLAYER_PLAY, static_cast<int>(to.asInteger()),
                                      named ? static_cast<int>(*named) : -1, nullptr);
            }
            else
              return InvalidParams;
            break;
          }

          case Picture:
            if (to.isString())
            {
              std::string strTo = to.asString();
              int actionID;
              if (strTo == "previous")
                actionID = ACTION_PREV_PICTURE;
              else if (strTo == "next")
                actionID = ACTION_NEXT_PICTURE;
              else
                return InvalidParams;

              SendSlideshowAction(actionID);
            }
            else
              return Fail(result, FailedToExecute, Reason::NotApplicable);
            break;

          case None:
          default:
            return FailedToExecute;
        }

        return ACK;
      });
}

JSONRPC_STATUS CPlayerOperations::SetPartymode(const CVariant &parameterObject, CVariant &result)
{
  return ForEachOnList(parameterObject, result,
      [&](PlayerType player)-> JSONRPC_STATUS
      {
        switch (player)
        {
          case Video:
          case Audio:
          {
            if (IsPVRChannel())
              return Fail(result, FailedToExecute, Reason::NotApplicable);

            const PLAYLIST::Type type = player == Video ? PLAYLIST::Video : PLAYLIST::Audio;
            const bool enabled = PARTYMODE::IsRunning();
            if (enabled && !PARTYMODE::IsRunning(type))
              return Fail(result, FailedToExecute, Reason::PartyModeElsewhere);

            const bool toggle = parameterObject["partyMode"].isString();
            const bool wanted = toggle ? !enabled : parameterObject["partyMode"].asBoolean();
            if (wanted != enabled)
              CServiceBroker::GetAppMessenger()->PostMsg(TMSG_PLAYLISTPLAYER_PARTYMODE,
                                                         wanted ? 1 : 0, static_cast<int>(type));
            break;
          }

          case Picture:
            return Fail(result, FailedToExecute, Reason::NotApplicable);
          default:
            return FailedToExecute;
        }

        return ACK;
      });
}

JSONRPC_STATUS CPlayerOperations::SetAudioStream(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           case Audio:
                           {
                             const auto appPlayer = AppPlayer();
                             if (appPlayer->HasPlayer())
                             {
                               int index = -1;
                               if (const JSONRPC_STATUS status = SelectStream(
                                       parameterObject["stream"], appPlayer->GetAudioStream(),
                                       appPlayer->GetAudioStreamCount(), index, result);
                                   status != OK)
                                 return status;

                               appPlayer->SetAudioStream(index);
                             }
                             else
                               return Fail(result, FailedToExecute, Reason::NothingPlaying);
                             break;
                           }

                           case Picture:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           default:
                             return FailedToExecute;
                         }

                         return ACK;
                       });
}

JSONRPC_STATUS CPlayerOperations::AddSubtitle(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         if (player != Video)
                           return Fail(result, FailedToExecute, Reason::NotApplicable);

                         const auto appPlayer = AppPlayer();

                         if (!appPlayer->HasPlayer())
                           return Fail(result, FailedToExecute, Reason::NothingPlaying);

                         std::string sub = parameterObject["subtitle"].asString();
                         appPlayer->AddSubtitle(sub);
                         return ACK;
                       });
}

JSONRPC_STATUS CPlayerOperations::SetSubtitle(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           {
                             const auto appPlayer = AppPlayer();
                             if (appPlayer->HasPlayer())
                             {
                               const CVariant& subtitle = parameterObject["subtitle"];
                               if (subtitle.isString() &&
                                   (subtitle.asString() == "off" || subtitle.asString() == "on"))
                               {
                                 appPlayer->SetSubtitleVisible(subtitle.asString() == "on");
                                 return ACK;
                               }

                               int index = -1;
                               if (const JSONRPC_STATUS status =
                                       SelectStream(subtitle, appPlayer->GetSubtitle(),
                                                    appPlayer->GetSubtitleCount(), index, result);
                                   status != OK)
                                 return status;

                               appPlayer->SetSubtitle(index);

                               // Check if we need to enable subtitles to be displayed
                               if (parameterObject["enable"].asBoolean() && !appPlayer->GetSubtitleVisible())
                                 appPlayer->SetSubtitleVisible(true);
                             }
                             else
                               return Fail(result, FailedToExecute, Reason::NothingPlaying);
                             break;
                           }

                           case Audio:
                           case Picture:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           default:
                             return FailedToExecute;
                         }

                         return ACK;
                       });
}

JSONRPC_STATUS CPlayerOperations::SetVideoStream(const CVariant &parameterObject, CVariant &result)
{
  return ForEachTarget(parameterObject, result,
                       [&](PlayerType player) -> JSONRPC_STATUS
                       {
                         switch (player)
                         {
                           case Video:
                           {
                             const auto appPlayer = AppPlayer();
                             int streamCount = appPlayer->GetVideoStreamCount();
                             if (streamCount > 0)
                             {
                               int index = -1;
                               if (const JSONRPC_STATUS status = SelectStream(
                                       parameterObject["stream"], appPlayer->GetVideoStream(),
                                       streamCount, index, result);
                                   status != OK)
                                 return status;

                               appPlayer->SetVideoStream(index);
                             }
                             else
                               return Fail(result, FailedToExecute, Reason::NotApplicable);
                             break;
                           }
                           case Audio:
                           case Picture:
                             return Fail(result, FailedToExecute, Reason::NotApplicable);
                           default:
                             return FailedToExecute;
                         }

                         return ACK;
                       });
}

std::vector<PlayerType> CPlayerOperations::GetTargets(const CVariant& playlist)
{
  const std::optional<PLAYLIST::Type> named = PLAYLIST::TypeFromName(playlist.asString());
  if (named == PLAYLIST::Audio)
    return {Audio};
  if (playlist.asString() == "picture")
    return {Picture};

  const auto playLists = CServiceBroker::GetPlayLists();
  const bool slideShow = playLists->IsSlideShowRunning();
  // the slideshow shows on the video side
  if (named == PLAYLIST::Video)
    return {slideShow && playLists->GetPlayingType() != PLAYLIST::Video ? Picture : Video};

  std::vector<PlayerType> targets;
  if (playLists->GetPlayingType())
    targets.push_back(playLists->IsPlayingAsAudio() ? Audio : Video);
  if (slideShow)
    targets.push_back(Picture);
  if (targets.empty())
    targets.push_back(GetPlayList(None, playlist) == PLAYLIST::Audio ? Audio : Video);
  return targets;
}

PlayerType CPlayerOperations::GetTarget(const CVariant& playlist)
{
  return GetTargets(playlist).front();
}

JSONRPC_STATUS CPlayerOperations::ForEachTarget(
    const CVariant& parameterObject,
    CVariant& result,
    const std::function<JSONRPC_STATUS(PlayerType)>& verb)
{
  // a named playlist that is not playing has no player to act on
  const auto playLists = CServiceBroker::GetPlayLists();
  const std::string& name = parameterObject["playlist"].asString();
  const std::optional<PLAYLIST::Type> named = PLAYLIST::TypeFromName(name);
  const bool idle = named ? playLists->GetPlayingType() != named &&
                                !(*named == PLAYLIST::Video && playLists->IsSlideShowRunning())
                          : name == "picture" && !playLists->IsSlideShowRunning();
  if (idle)
    return Fail(result, FailedToExecute, Reason::NothingPlaying,
                Target("playlist", parameterObject["playlist"]));

  const JSONRPC_STATUS status = ForEachOnList(parameterObject, result, verb);
  // with nothing playing, the player acted on was only a guess
  if (status == FailedToExecute && !IsAnythingPlaying())
    return Fail(result, FailedToExecute, Reason::NothingPlaying);
  return status;
}

JSONRPC_STATUS CPlayerOperations::ForEachOnList(
    const CVariant& parameterObject,
    CVariant& result,
    const std::function<JSONRPC_STATUS(PlayerType)>& verb)
{
  std::optional<JSONRPC_STATUS> succeeded;
  std::optional<JSONRPC_STATUS> failed;
  CVariant failure;
  for (const PlayerType player : GetTargets(parameterObject["playlist"]))
  {
    // a failure's error data must not reach a success, nor overwrite what another verb answered
    CVariant answered = result;
    const JSONRPC_STATUS status = verb(player);
    if (status == OK || status == ACK)
    {
      succeeded = succeeded.value_or(status);
      continue;
    }

    if (!failed)
    {
      failed = status;
      failure = std::move(result);
      if (failure.isMember("reason") && !failure.isMember("target"))
      {
        const std::optional<PLAYLIST::Type> playList =
            GetPlayList(player, parameterObject["playlist"]);
        failure["target"] =
            Target("playlist", playList ? std::string{PLAYLIST::NameOf(*playList)} : "picture");
      }
    }
    result = std::move(answered);
  }

  if (succeeded)
    return *succeeded;
  result = std::move(failure);
  return *failed;
}

bool CPlayerOperations::IsAnythingPlaying()
{
  return AppPlayer()->IsPlaying() ||
      CServiceBroker::GetPlayLists()->IsSlideShowRunning();
}

std::optional<PLAYLIST::Type> CPlayerOperations::GetPlayList(PlayerType player,
                                                             const CVariant& named)
{
  if (player == Picture)
    return std::nullopt;
  if (const std::optional<PLAYLIST::Type> type = PLAYLIST::TypeFromName(named.asString()); type)
    return type;

  return CServiceBroker::GetPlayLists()->GetPlayingType().value_or(
      player == Audio ? PLAYLIST::Audio: PLAYLIST::Video);
}

JSONRPC_STATUS CPlayerOperations::StartSlideshow(const std::string& path, bool recursive, bool random, const std::string &firstPicturePath /* = "" */)
{
  int flags = 0;
  if (recursive)
    flags |= 1;
  if (random)
    flags |= 2;
  else
    flags |= 4;

  std::vector<std::string> params;
  params.push_back(path);
  if (!firstPicturePath.empty())
    params.push_back(firstPicturePath);

  // Reset screensaver when started from JSON only to avoid potential conflict with slideshow screensavers
  auto& components = CServiceBroker::GetAppComponents();
  const auto appPower = components.GetComponent<CApplicationPowerHandling>();
  appPower->ResetScreenSaver();
  appPower->WakeUpScreenSaverAndDPMS();
  CGUIMessage msg(GUI_MSG_START_SLIDESHOW, 0, 0, flags);
  msg.SetStringParams(params);
  CServiceBroker::GetAppMessenger()->SendGUIMessage(msg, WINDOW_SLIDESHOW);

  return ACK;
}

void CPlayerOperations::SendSlideshowAction(int actionID)
{
  CServiceBroker::GetAppMessenger()->SendMsg(TMSG_GUI_ACTION, WINDOW_SLIDESHOW, -1,
      TransferToMessenger(std::make_unique<CAction>(actionID)));
}

JSONRPC_STATUS CPlayerOperations::GetPropertyValue(PlayerType player, const std::string &property, CVariant &result,
                                                   const CVariant& named /* = CVariant() */)
{
  using PlayList = std::optional<PLAYLIST::Type>;
  //! A property's value, or nothing when it cannot be read
  using Value = std::optional<CVariant>;
  using Getter = Value (*)(PlayerType player, const PlayList& playList);

  // what is seen and heard whether or not anything plays
  static const std::unordered_map<std::string_view, Value (*)()> output{
      {"volume", []() -> Value
       { return static_cast<int>(std::lroundf(VolumeHandling()->GetVolumePercent())); }},
      {"muted", []() -> Value { return VolumeHandling()->IsMuted(); }},
      {"contentRect",
       []() -> Value
       {
         const auto contentGeometry = ContentGeometryComponent();
         if (!contentGeometry)
           return std::nullopt;
         CVariant rect;
         KODI::VIDEO::GEOMETRY::SerializeEffectiveGeometry(contentGeometry->Get(), rect);
         return rect;
       }},
  };
  if (const auto getter = output.find(property); getter != output.end())
  {
    Value value = getter->second();
    if (!value)
      return FailedToExecute;
    result = std::move(*value);
    return OK;
  }

  if (player == None)
    return FailedToExecute;

  static const auto timeObject = [](int milliseconds)
  {
    CVariant time;
    INTERFACES::MillisecondsToTimeObject(milliseconds, time);
    return time;
  };

  static const auto position = [](PlayerType player, const PlayList& playList) -> Value
  {
    if (player == Picture)
    {
      const CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      return slideShow.IsPlaying() ? slideShow.CurrentSlide() - 1 : -1;
    }
    if (IsPVRChannel() || !playList)
      return -1;
    return CServiceBroker::GetPlayLists()->GetPlayingPosition(*playList);
  };

  static const std::unordered_map<std::string_view, Getter> getters{
      {"type",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player == Video)
           return "video";
         if (player == Audio)
           return "audio";
         return "picture";
       }},
      {"partyMode", [](PlayerType player, const PlayList&) -> Value
       { return player != Picture && !IsPVRChannel() && PARTYMODE::IsRunning(); }},
      {"speed", [](PlayerType player, const PlayList&) -> Value { return PlaybackSpeed(player); }},
      {"time",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player == Picture)
           return timeObject(0);
        if (!IsPVRChannel())
           return timeObject(static_cast<int>(g_application.GetTime() * 1000.0));
         const std::shared_ptr<CPVREpgInfoTag> epg = GetCurrentEpg();
         return timeObject(epg ? static_cast<int>(epg->Progress() * 1000) : 0);
       }},
      {"totalTime",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player == Picture)
           return timeObject(0);
         if (!IsPVRChannel())
           return timeObject(static_cast<int>(g_application.GetTotalTime() * 1000.0));
         const std::shared_ptr<CPVREpgInfoTag> epg = GetCurrentEpg();
         return timeObject(epg ? static_cast<int>(epg->GetDuration() * 1000) : 0);
       }},
      {"percentage",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player == Picture)
         {
           const CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
           if (slideShow.NumSlides() > 0)
             return static_cast<double>(slideShow.CurrentSlide()) / slideShow.NumSlides();
           return 0.0;
         }
         if (!IsPVRChannel())
           return g_application.GetPercentage();
         if (const std::shared_ptr<CPVREpgInfoTag> epg = GetCurrentEpg())
           return epg->ProgressPercentage();
         return 0;
       }},
      {"playerType",
       [](PlayerType player, const PlayList&) -> Value
       {
         const auto appPlayer = AppPlayer();
        if (player != Picture && appPlayer->IsExternalPlaying())
           return "external";
         if (player != Picture && appPlayer->IsRemotePlaying())
           return "remote";
         return "internal";
      }
    },
      {"cachePercentage",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player == Picture)
           return 0.0;
         return g_application.GetCachePercentage();
       }},
      {"playlist", [](PlayerType, const PlayList& playList) -> Value
       { return playList ? std::string{PLAYLIST::NameOf(*playList)} : "picture"; }},
      {"position", position},
      {"displayOrder",
       [](PlayerType player, const PlayList& playList) -> Value
       {
         if (player == Picture)
           return position(player, playList);
         if (IsPVRChannel() || !playList)
           return -1;
         return CServiceBroker::GetPlayLists()->GetPlayingDisplayPosition(*playList);
       }},
      {"canSeek", [](PlayerType player, const PlayList&) -> Value
       { return player != Picture && AppPlayer()->CanSeek(); }},
      {"canChangeSpeed", [](PlayerType player, const PlayList&) -> Value
       { return player != Picture && !IsPVRChannel(); }},
      {"canMove", [](PlayerType player, const PlayList&) -> Value { return player == Picture; }},
      {"canZoom", [](PlayerType player, const PlayList&) -> Value { return player == Picture; }},
      {"canRotate", [](PlayerType player, const PlayList&) -> Value { return player == Picture; }},
      {"canShuffle", [](PlayerType, const PlayList&) -> Value { return !IsPVRChannel(); }},
      {"canRepeat", [](PlayerType player, const PlayList&) -> Value
       { return player != Picture && !IsPVRChannel(); }},
      {"currentAudioStream",
       [](PlayerType player, const PlayList&) -> Value
       {
         const auto appPlayer = AppPlayer();
        if (player == Picture || !appPlayer->HasPlayer())
           return CVariant{};
         const int index = appPlayer->GetAudioStream();
         if (index < 0)
           return CVariant{};
         AudioStreamInfo info;
         appPlayer->GetAudioStreamInfo(index, info);
         return INTERFACES::StreamToObject(index, info);
       }},
      {"audioStreams",
       [](PlayerType player, const PlayList&) -> Value
       {
         const auto appPlayer = AppPlayer();
         if (player == Picture || !appPlayer->HasPlayer())
           return CVariant(CVariant::VariantTypeArray);
         return StreamList(*appPlayer, &CApplicationPlayer::GetAudioStreamCount,
                           &CApplicationPlayer::GetAudioStreamInfo);
       }},
      {"currentVideoStream",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player != Video)
           return CVariant{};
        const auto appPlayer = AppPlayer();
      const int index = appPlayer->GetVideoStream();
      if (index < 0)
           return CVariant{};
         VideoStreamInfo info;
         appPlayer->GetVideoStreamInfo(index, info);
         return INTERFACES::StreamToObject(index, info);
       }},
      {"videoStreams",
       [](PlayerType player, const PlayList&) -> Value
       {
         if (player != Video)
           return CVariant(CVariant::VariantTypeArray);
         return StreamList(*AppPlayer(), &CApplicationPlayer::GetVideoStreamCount,
                           &CApplicationPlayer::GetVideoStreamInfo);
       }},
      {"subtitleEnabled", [](PlayerType player, const PlayList&) -> Value
       { return player == Video && AppPlayer()->GetSubtitleVisible(); }},
      {"currentSubtitle",
       [](PlayerType player, const PlayList&) -> Value
       {
         const auto appPlayer = AppPlayer();
      if (player != Video || !appPlayer->HasPlayer())
           return CVariant{};
         const int index = appPlayer->GetSubtitle();
         if (index < 0)
           return CVariant{};
         SubtitleStreamInfo info;
         appPlayer->GetSubtitleStreamInfo(index, info);
         return INTERFACES::StreamToObject(index, info);
       }},
      {"subtitles",
       [](PlayerType player, const PlayList&) -> Value
       {
         const auto appPlayer = AppPlayer();
         if (player != Video || !appPlayer->HasPlayer())
           return CVariant(CVariant::VariantTypeArray);
         return StreamList(*appPlayer, &CApplicationPlayer::GetSubtitleCount,
                           &CApplicationPlayer::GetSubtitleStreamInfo);
       }},
      {"live", [](PlayerType, const PlayList&) -> Value { return IsPVRChannel(); }},
  };

  const auto getter = getters.find(property);
  if (getter == getters.end())
    return InvalidParams;

  Value value = getter->second(player, GetPlayList(player, named));

  if (!value)
    return FailedToExecute;

  result = std::move(*value);

  return OK;
}

bool CPlayerOperations::IsPVRChannel()
{
  const std::shared_ptr<const CPVRPlaybackState> state =
      CServiceBroker::GetPVRManager().PlaybackState();
  return state->IsPlayingTV() || state->IsPlayingRadio();
}

std::shared_ptr<CPVREpgInfoTag> CPlayerOperations::GetCurrentEpg()
{
  const std::shared_ptr<const CPVRPlaybackState> state =
      CServiceBroker::GetPVRManager().PlaybackState();
  if (!state->IsPlayingTV() && !state->IsPlayingRadio())
    return {};

  const std::shared_ptr<const CPVRChannel> currentChannel = state->GetPlayingChannel();
  if (!currentChannel)
    return {};

  return currentChannel->GetEPGNow();
}

JSONRPC_STATUS CPlayerOperations::GetChapters(const CVariant& parameterObject,
                                              CVariant& result)
{
  const auto appPlayer = AppPlayer();

  if (!IsAnythingPlaying())
    return Fail(result, FailedToExecute, Reason::NothingPlaying);
  if (GetTarget(parameterObject["playlist"]) != Video || !appPlayer->IsPlayingVideo())
    return Fail(result, FailedToExecute, Reason::NotApplicable);

  // Extract chapters from CApplicationPlayer
  const int chapterCount = appPlayer->GetChapterCount();
  CVariant chapters(CVariant::VariantTypeArray);

  for (int i = 1; i <= chapterCount; ++i)
  {
    CVariant chapter(CVariant::VariantTypeObject);
    chapter["index"] = i;
    // Chapter name
    std::string name;
    appPlayer->GetChapterName(name, i);
    if (!name.empty())
      chapter["name"] = name;
    // Chapter position in seconds
    int64_t position = appPlayer->GetChapterPos(i);
    if (position < 0)
      continue;
    chapter["time"] = position;
    chapters.push_back(chapter);
  }

  result["chapters"] = std::move(chapters);
  return OK;
}
