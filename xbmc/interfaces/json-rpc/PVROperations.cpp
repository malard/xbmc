/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PVROperations.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "XBDateTime.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "pvr/PVRManager.h"
#include "pvr/PVRPlaybackState.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/channels/PVRChannelGroup.h"
#include "pvr/channels/PVRChannelGroupMember.h"
#include "pvr/channels/PVRChannelGroups.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/epg/Epg.h"
#include "pvr/epg/EpgContainer.h"
#include "pvr/epg/EpgInfoTag.h"
#include "pvr/guilib/PVRGUIActionsChannels.h"
#include "pvr/guilib/PVRGUIActionsTimers.h"
#include "pvr/recordings/PVRRecordings.h"
#include "pvr/timers/PVRTimerInfoTag.h"
#include "pvr/timers/PVRTimers.h"
#include "utils/Variant.h"

#include <memory>
#include <string>
#include <vector>

using namespace JSONRPC;
using namespace PVR;
using namespace KODI::MESSAGING;

namespace
{
bool PvrStarted()
{
  return CServiceBroker::GetPVRManager().IsStarted();
}

//! \p container once PVR has started, else nullptr: the containers exist before PVR has
//! loaded them, and are not safe to search until then.
template<typename T>
std::shared_ptr<T> Started(std::shared_ptr<T> container)
{
  return PvrStarted() ? std::move(container) : nullptr;
}

//! The group a channelGroupId names: an id, or "allTv" or "allRadio" for every channel of
//! that kind.
std::shared_ptr<const CPVRChannelGroup> ChannelGroupFrom(const CPVRChannelGroupsContainer& groups,
                                                         const CVariant& id)
{
  if (id.isInteger())
    return groups.GetByIdFromAll(static_cast<int>(id.asInteger()));
  if (id.isString())
    return groups.GetGroupAll(id.asString() == "allRadio");
  return nullptr;
}
} // namespace

JSONRPC_STATUS CPVROperations::FindChannel(const std::string& key,
                                           const CVariant& id,
                                           std::shared_ptr<CPVRChannel>& channel,
                                           CVariant& result)
{
  const std::shared_ptr<const CPVRChannelGroupsContainer> channelGroupContainer{
      Started(CServiceBroker::GetPVRManager().ChannelGroups())};
  if (!channelGroupContainer)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  channel = channelGroupContainer->GetChannelById(static_cast<int>(id.asInteger()));
  if (!channel)
    return Fail(result, NotFound, Reason::NoSuchItem, Target(key, id));

  return OK;
}

JSONRPC_STATUS CPVROperations::FindBroadcast(const std::string& key,
                                             const CVariant& id,
                                             std::shared_ptr<CPVREpgInfoTag>& broadcast,
                                             CVariant& result)
{
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  broadcast = CServiceBroker::GetPVRManager().EpgContainer().GetTagByDatabaseId(
      static_cast<int>(id.asInteger()));
  if (!broadcast)
    return Fail(result, NotFound, Reason::NoSuchItem, Target(key, id));

  return OK;
}

JSONRPC_STATUS CPVROperations::FindRecording(const std::string& key,
                                             const CVariant& id,
                                             std::shared_ptr<CPVRRecording>& recording,
                                             CVariant& result)
{
  const std::shared_ptr<const CPVRRecordings> recordings{
      Started(CServiceBroker::GetPVRManager().Recordings())};
  if (!recordings)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  recording = recordings->GetById(static_cast<int>(id.asInteger()));
  if (!recording)
    return Fail(result, NotFound, Reason::NoSuchItem, Target(key, id));

  return OK;
}

JSONRPC_STATUS CPVROperations::FindTimer(const std::string& key,
                                         const CVariant& id,
                                         std::shared_ptr<CPVRTimerInfoTag>& timer,
                                         CVariant& result)
{
  const std::shared_ptr<const CPVRTimers> timers{Started(CServiceBroker::GetPVRManager().Timers())};
  if (!timers)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  timer = timers->GetById(static_cast<int>(id.asInteger()));
  if (!timer)
    return Fail(result, NotFound, Reason::NoSuchItem, Target(key, id));

  return OK;
}

JSONRPC_STATUS CPVROperations::GetProperties(const CVariant& parameterObject, CVariant& result)
{
  return GetNamedProperties(parameterObject, result, GetPropertyValue);
}

JSONRPC_STATUS CPVROperations::GetChannelGroups(const CVariant& parameterObject, CVariant& result)
{
  const std::shared_ptr<const CPVRChannelGroupsContainer> channelGroupContainer{
      Started(CServiceBroker::GetPVRManager().ChannelGroups())};
  if (!channelGroupContainer)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  const std::shared_ptr<const CPVRChannelGroups> channelGroups{
      channelGroupContainer->Get(parameterObject["channelType"].asString() == "radio")};
  if (!channelGroups)
    return FailedToExecute;

  int start{0};
  int end{0};

  std::vector<std::shared_ptr<CPVRChannelGroup>> groupList{channelGroups->GetMembers(true)};
  HandleLimits(parameterObject, result, static_cast<int>(groupList.size()), start, end);
  for (int index = start; index < end; ++index)
    FillChannelGroupDetails(groupList.at(index), parameterObject, result["channelGroups"], true);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetChannelGroupDetails(const CVariant& parameterObject,
                                                      CVariant& result)
{
  const std::shared_ptr<const CPVRChannelGroupsContainer> channelGroupContainer{
      Started(CServiceBroker::GetPVRManager().ChannelGroups())};
  if (!channelGroupContainer)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  const std::shared_ptr<const CPVRChannelGroup> channelGroup{
      ChannelGroupFrom(*channelGroupContainer, parameterObject["channelGroupId"])};

  if (!channelGroup)
    return Fail(result, NotFound, Reason::NoSuchItem,
                Target("channelGroupId", parameterObject["channelGroupId"]));

  FillChannelGroupDetails(channelGroup, parameterObject, result["channelGroupDetails"], false);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetChannels(const CVariant& parameterObject, CVariant& result)
{
  const std::shared_ptr<const CPVRChannelGroupsContainer> channelGroupContainer{
      Started(CServiceBroker::GetPVRManager().ChannelGroups())};
  if (!channelGroupContainer)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  const std::shared_ptr<const CPVRChannelGroup> channelGroup{
      ChannelGroupFrom(*channelGroupContainer, parameterObject["channelGroupId"])};

  if (!channelGroup)
    return Fail(result, NotFound, Reason::NoSuchItem,
                Target("channelGroupId", parameterObject["channelGroupId"]));

  CFileItemList channels;
  const auto groupMembers = channelGroup->GetMembers(CPVRChannelGroup::Include::ONLY_VISIBLE);
  for (const auto& groupMember : groupMembers)
  {
    channels.Add(std::make_shared<CFileItem>(groupMember));
  }

  HandleFileItemList("channelId", false, "channels", channels, parameterObject, result, true);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetChannelDetails(const CVariant& parameterObject, CVariant& result)
{
  std::shared_ptr<CPVRChannel> channel;
  if (const JSONRPC_STATUS status =
          FindChannel("channelId", parameterObject["channelId"], channel, result);
      status != OK)
    return status;

  const std::shared_ptr<CPVRChannelGroupMember> groupMember{
      CServiceBroker::GetPVRManager().Get<PVR::GUI::Channels>().GetChannelGroupMember(channel)};
  if (!groupMember)
    return Fail(result, NotFound, Reason::NoSuchItem,
                Target("channelId", parameterObject["channelId"]));

  HandleFileItem("channelId", false, "channelDetails", std::make_shared<CFileItem>(groupMember),
                 parameterObject, parameterObject["properties"], result, false);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetClients(const CVariant& parameterObject, CVariant& result)
{
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  int start{0};
  int end{0};
  auto clientInfos = CServiceBroker::GetPVRManager().Clients()->GetEnabledClientInfos();
  HandleLimits(parameterObject, result, static_cast<int>(clientInfos.size()), start, end);

  for (int index = start; index < end; ++index)
  {
    result["clients"].append(clientInfos[index]);
  }

  return OK;
}

JSONRPC_STATUS CPVROperations::GetBroadcasts(const CVariant& parameterObject, CVariant& result)
{
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  CDateTime start;
  CDateTime end;

  const JSONRPC_STATUS rangeStatus{ParseTimeRange(parameterObject, false, start, end)};
  if (rangeStatus != OK)
    return rangeStatus;

  std::shared_ptr<CPVRChannel> channel;
  if (const JSONRPC_STATUS status =
          FindChannel("channelId", parameterObject["channelId"], channel, result);
      status != OK)
    return status;

  const std::shared_ptr<const CPVREpg> channelEpg{channel->GetEPG()};
  if (!channelEpg)
    return InternalError;

  CFileItemList programFull;
  for (const auto& tag : GetBroadcastsInRange(*channelEpg, start, end))
  {
    programFull.Add(std::make_shared<CFileItem>(tag));
  }

  HandleFileItemList("broadcastId", false, "broadcasts", programFull, parameterObject, result,
                     programFull.Size(), true);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetBroadcastsByChannelGroup(const CVariant& parameterObject,
                                                   CVariant& result)
{
  const std::shared_ptr<const CPVRChannelGroupsContainer> channelGroupContainer{
      Started(CServiceBroker::GetPVRManager().ChannelGroups())};
  if (!channelGroupContainer)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  CDateTime start;
  CDateTime end;

  const JSONRPC_STATUS rangeStatus{ParseTimeRange(parameterObject, true, start, end)};
  if (rangeStatus != OK)
    return rangeStatus;

  const std::shared_ptr<const CPVRChannelGroup> channelGroup{
      ChannelGroupFrom(*channelGroupContainer, parameterObject["channelGroupId"])};
  if (!channelGroup)
    return Fail(result, NotFound, Reason::NoSuchItem,
                Target("channelGroupId", parameterObject["channelGroupId"]));

  result["channels"] = CVariant{CVariant::VariantTypeArray};

  const auto groupMembers = channelGroup->GetMembers(CPVRChannelGroup::Include::ONLY_VISIBLE);
  for (const auto& groupMember : groupMembers)
  {
    const std::shared_ptr<const CPVRChannel> channel{groupMember->Channel()};
    if (!channel)
      continue;

    CVariant entry{CVariant::VariantTypeObject};
    entry["channelId"] = channel->ChannelID();
    entry["broadcasts"] = CVariant{CVariant::VariantTypeArray};

    const std::shared_ptr<const CPVREpg> channelEpg{channel->GetEPG()};
    if (channelEpg)
    {
      for (const auto& tag : GetBroadcastsInRange(*channelEpg, start, end))
      {
        HandleFileItem("broadcastId", false, "broadcasts", std::make_shared<CFileItem>(tag),
                       parameterObject, parameterObject["properties"], entry, true);
      }
    }

    result["channels"].append(std::move(entry));
  }

  return OK;
}

JSONRPC_STATUS CPVROperations::GetBroadcastDetails(const CVariant& parameterObject,
                                                   CVariant& result)
{
  std::shared_ptr<CPVREpgInfoTag> epgTag;

  if (const JSONRPC_STATUS status =
          FindBroadcast("broadcastId", parameterObject["broadcastId"], epgTag, result);
      status != OK)
    return status;

  HandleFileItem("broadcastId", false, "broadcastDetails", std::make_shared<CFileItem>(epgTag),
                 parameterObject, parameterObject["properties"], result, false);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetBroadcastIsPlayable(const CVariant& parameterObject,
                                                      CVariant& result)
{
  std::shared_ptr<CPVREpgInfoTag> epgTag;

  if (const JSONRPC_STATUS status =
          FindBroadcast("broadcastId", parameterObject["broadcastId"], epgTag, result);
      status != OK)
    return status;

  result = epgTag->IsPlayable();

  return OK;
}

JSONRPC_STATUS CPVROperations::GetPlayableBroadcasts(const CVariant& parameterObject,
                                                     CVariant& result)
{
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  CDateTime start;
  CDateTime end;
  const JSONRPC_STATUS rangeStatus{ParseTimeRange(parameterObject, true, start, end)};
  if (rangeStatus != OK)
    return rangeStatus;

  std::shared_ptr<CPVRChannel> channel;
  if (const JSONRPC_STATUS status =
          FindChannel("channelId", parameterObject["channelId"], channel, result);
      status != OK)
    return status;

  const std::shared_ptr<const CPVREpg> channelEpg{channel->GetEPG()};
  if (!channelEpg)
    return InternalError;

  const std::vector<std::shared_ptr<CPVREpgInfoTag>> tagsInRange{
      GetBroadcastsInRange(*channelEpg, start, end)};

  // Resolving playability costs a call into the client per tag, so bound how many are
  // examined rather than how many are returned.
  int first{0};
  int last{0};
  HandleLimits(parameterObject, result, static_cast<int>(tagsInRange.size()), first, last);

  result["broadcastIds"] = CVariant{CVariant::VariantTypeArray};

  for (int index = first; index < last; ++index)
  {
    const std::shared_ptr<const CPVREpgInfoTag>& tag{tagsInRange[index]};
    if (tag->IsPlayable())
    {
      result["broadcastIds"].append(tag->DatabaseID());
    }
  }

  return OK;
}

JSONRPC_STATUS CPVROperations::Record(const CVariant& parameterObject, CVariant& result)
{
  auto& pvrManager{CServiceBroker::GetPVRManager()};
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  std::shared_ptr<CPVRChannel> pChannel;
  const CVariant channel{parameterObject["channel"]};
  if (channel.isString() && channel.asString() == "current")
  {
    pChannel = pvrManager.PlaybackState()->GetPlayingChannel();
    if (!pChannel)
    {
      const bool playing{
          CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->IsPlaying()};
      return Fail(result, FailedToExecute,
                  playing ? Reason::NotApplicable : Reason::NothingPlaying);
    }
  }
  else if (channel.isInteger())
  {
    if (const JSONRPC_STATUS status = FindChannel("channel", channel, pChannel, result);
        status != OK)
      return status;
  }
  else
    return InvalidParams;

  if (!pChannel->CanRecord())
    return Fail(result, FailedToExecute, Reason::NotRecordable,
                Target("channel", parameterObject["channel"]));

  const CVariant record{parameterObject["record"]};
  const bool isRecording{pvrManager.Timers()->IsRecordingOnChannel(*pChannel)};
  bool toggle = true;
  if (record.isBoolean() && record.asBoolean() == isRecording)
    toggle = false;

  if (toggle)
  {
    if (!pvrManager.Get<PVR::GUI::Timers>().SetRecordingOnChannel(pChannel, !isRecording))
      return Fail(result, FailedToExecute, Reason::BackendRefused);
  }

  return ACK;
}

JSONRPC_STATUS CPVROperations::Scan(const CVariant& parameterObject, CVariant& result)
{
  if (!PvrStarted())
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  if (parameterObject.isMember("clientId"))
  {
    if (CServiceBroker::GetPVRManager().Get<PVR::GUI::Channels>().StartChannelScan(
            static_cast<int>(parameterObject["clientId"].asInteger())))
      return ACK;
  }
  else
  {
    if (CServiceBroker::GetPVRManager().Get<PVR::GUI::Channels>().StartChannelScan())
      return ACK;
  }

  return Fail(result, FailedToExecute, Reason::BackendRefused);
}

JSONRPC_STATUS CPVROperations::GetPropertyValue(const std::string& property, CVariant& result)
{
  const bool started{PvrStarted()};

  if (property == "available")
    result = started;
  else if (property == "recording")
  {
    if (started)
      result = CServiceBroker::GetPVRManager().PlaybackState()->IsRecording();
    else
      result = false;
  }
  else if (property == "scanning")
  {
    if (started)
      result = CServiceBroker::GetPVRManager().Get<PVR::GUI::Channels>().IsRunningChannelScan();
    else
      result = false;
  }
  else
    return InvalidParams;

  return OK;
}

void CPVROperations::FillChannelGroupDetails(
    const std::shared_ptr<const CPVRChannelGroup>& channelGroup,
    const CVariant& parameterObject,
    CVariant& result,
    bool append /* = false */)
{
  if (!channelGroup)
    return;

  CVariant object{CVariant::VariantTypeObject};
  object["channelGroupId"] = channelGroup->GroupID();
  object["channelType"] = channelGroup->IsRadio() ? "radio" : "tv";
  object["label"] = channelGroup->GroupName();

  if (append)
    result.append(object);
  else
  {
    CFileItemList channels;
    const auto groupMembers{channelGroup->GetMembers(CPVRChannelGroup::Include::ONLY_VISIBLE)};
    for (const auto& groupMember : groupMembers)
    {
      channels.Add(std::make_shared<CFileItem>(groupMember));
    }

    object["channels"] = CVariant(CVariant::VariantTypeArray);
    HandleFileItemList("channelId", false, "channels", channels, parameterObject["channels"],
                       object, false);

    result = object;
  }
}

JSONRPC_STATUS CPVROperations::GetTimers(const CVariant& parameterObject, CVariant& result)
{
  const std::shared_ptr<const CPVRTimers> timers{Started(CServiceBroker::GetPVRManager().Timers())};
  if (!timers)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  CFileItemList timerList;
  const std::vector<std::shared_ptr<CPVRTimerInfoTag>> tags{timers->GetAll()};
  for (const auto& timer : tags)
  {
    timerList.Add(std::make_shared<CFileItem>(timer));
  }

  HandleFileItemList("timerId", false, "timers", timerList, parameterObject, result, true);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetTimerDetails(const CVariant& parameterObject, CVariant& result)
{
  std::shared_ptr<CPVRTimerInfoTag> timer;
  if (const JSONRPC_STATUS status = FindTimer("timerId", parameterObject["timerId"], timer, result);
      status != OK)
    return status;

  HandleFileItem("timerId", false, "timerDetails", std::make_shared<CFileItem>(timer),
                 parameterObject, parameterObject["properties"], result, false);

  return OK;
}

JSONRPC_STATUS CPVROperations::AddTimer(const CVariant& parameterObject, CVariant& result)
{
  auto& pvrManager{CServiceBroker::GetPVRManager()};
  std::shared_ptr<CPVREpgInfoTag> epgTag;

  if (const JSONRPC_STATUS status =
          FindBroadcast("broadcastId", parameterObject["broadcastId"], epgTag, result);
      status != OK)
    return status;

  if (pvrManager.Timers()->GetTimerForEpgTag(epgTag))
    return Fail(result, FailedToExecute, Reason::TimerExists,
                Target("broadcastId", parameterObject["broadcastId"]));

  const std::shared_ptr<CPVRTimerInfoTag> newTimer{
      CPVRTimerInfoTag::CreateFromEpg(epgTag, parameterObject["timerRule"].asBoolean(false),
                                      parameterObject["reminder"].asBoolean(false))};
  if (newTimer)
  {
    if (pvrManager.Get<PVR::GUI::Timers>().AddTimer(newTimer))
      return ACK;
  }
  return Fail(result, FailedToExecute, Reason::BackendRefused);
}

JSONRPC_STATUS CPVROperations::DeleteTimer(const CVariant& parameterObject, CVariant& result)
{
  std::shared_ptr<CPVRTimerInfoTag> timer;
  if (const JSONRPC_STATUS status = FindTimer("timerId", parameterObject["timerId"], timer, result);
      status != OK)
    return status;

  if (CServiceBroker::GetPVRManager().Timers()->DeleteTimer(timer, timer->IsRecording(), false) == TimerOperationResult::OK)
    return ACK;

  return Fail(result, FailedToExecute, Reason::BackendRefused);
}

JSONRPC_STATUS CPVROperations::ToggleTimer(const CVariant& parameterObject, CVariant& result)
{
  auto& pvrManager{CServiceBroker::GetPVRManager()};
  std::shared_ptr<CPVREpgInfoTag> epgTag;

  if (const JSONRPC_STATUS status =
          FindBroadcast("broadcastId", parameterObject["broadcastId"], epgTag, result);
      status != OK)
    return status;

  const std::shared_ptr<CPVRTimers> timers{pvrManager.Timers()};
  if (!timers)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  const bool timerrule{parameterObject["timerRule"].asBoolean(false)};
  bool sentOkay = false;
  std::shared_ptr<CPVRTimerInfoTag> timer{timers->GetTimerForEpgTag(epgTag)};
  if (timer)
  {
    if (timerrule)
      timer = timers->GetTimerRule(timer);

    if (timer)
      sentOkay =
          (timers->DeleteTimer(timer, timer->IsRecording(), false) == TimerOperationResult::OK);
  }
  else
  {
    timer = CPVRTimerInfoTag::CreateFromEpg(epgTag, timerrule);
    if (!timer)
      return InvalidParams;

    sentOkay = pvrManager.Get<PVR::GUI::Timers>().AddTimer(timer);
  }

  if (sentOkay)
    return ACK;

  return Fail(result, FailedToExecute, Reason::BackendRefused);
}

JSONRPC_STATUS CPVROperations::GetRecordings(const CVariant& parameterObject, CVariant& result)
{
  const std::shared_ptr<const CPVRRecordings> recordings{
      Started(CServiceBroker::GetPVRManager().Recordings())};
  if (!recordings)
    return Fail(result, FailedToExecute, Reason::PvrNotStarted);

  CFileItemList recordingsList;
  const std::vector<std::shared_ptr<CPVRRecording>> recs{recordings->GetAll()};
  for (const auto& recording : recs)
  {
    recordingsList.Add(std::make_shared<CFileItem>(recording));
  }

  HandleFileItemList("recordingId", true, "recordings", recordingsList, parameterObject, result,
                     true);

  return OK;
}

JSONRPC_STATUS CPVROperations::GetRecordingDetails(const CVariant& parameterObject,
                                                   CVariant& result)
{
  std::shared_ptr<CPVRRecording> recording;
  if (const JSONRPC_STATUS status =
          FindRecording("recordingId", parameterObject["recordingId"], recording, result);
      status != OK)
    return status;

  HandleFileItem("recordingId", true, "recordingDetails", std::make_shared<CFileItem>(recording),
                 parameterObject, parameterObject["properties"], result, false);

  return OK;
}

std::shared_ptr<CFileItem> CPVROperations::GetRecordingFileItem(int recordingId)
{
  const std::shared_ptr<const CPVRRecordings> recordings{
      Started(CServiceBroker::GetPVRManager().Recordings())};
  if (!recordings)
    return {};

  const std::shared_ptr<CPVRRecording> recording{recordings->GetById(recordingId)};
  if (!recording)
    return {};

  return std::make_shared<CFileItem>(recording);
}

JSONRPC_STATUS CPVROperations::ParseTimeRange(const CVariant& parameterObject,
                                              bool required,
                                              CDateTime& start,
                                              CDateTime& end)
{
  const std::string startTime{parameterObject["startTime"].asString()};
  const std::string endTime{parameterObject["endTime"].asString()};

  if (startTime.empty() && endTime.empty())
  {
    if (required)
      return InvalidParams;

    start.SetValid(false);
    end.SetValid(false);
    return OK;
  }

  if (!start.SetFromDBDateTime(startTime) || !end.SetFromDBDateTime(endTime) || end < start)
    return InvalidParams;

  return OK;
}

std::vector<std::shared_ptr<CPVREpgInfoTag>> CPVROperations::GetBroadcastsInRange(
    const CPVREpg& epg, const CDateTime& start, const CDateTime& end)
{
  if (!start.IsValid() || !end.IsValid())
    return epg.GetTags();

  // The ranged query fills the gaps between broadcasts with placeholder tags, and from the database
  // it also answers the broadcasts that only touch the range
  std::vector<std::shared_ptr<CPVREpgInfoTag>> tags{epg.GetTimeline(start, end, start, end)};
  std::erase_if(
      tags, [&start, &end](const std::shared_ptr<CPVREpgInfoTag>& tag)
      { return tag->IsGapTag() || tag->EndAsUTC() <= start || tag->StartAsUTC() >= end; });
  return tags;
}
