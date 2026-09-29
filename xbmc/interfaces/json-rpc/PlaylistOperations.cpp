/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaylistOperations.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIUserMessages.h"
#include "MessengerPayload.h"
#include "PlaybackModes.h"
#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "messaging/ApplicationMessenger.h"
#include "pictures/PictureInfoTag.h"
#include "pictures/SlideShowDelegator.h"
#include "playlists/PlayList.h"
#include "utils/Variant.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

using namespace JSONRPC;
using namespace KODI;

namespace
{
// The playlists the interface publishes, by name. Pictures is the slideshow's list, not a CPlayList.
struct PublishedPlayList
{
  std::string_view name; // Playlist.Name, which is also its Playlist.Type
  std::optional<PLAYLIST::Type> type;
  std::string_view media; // the "media" an item added to it is read as
};

constexpr std::array<PublishedPlayList, 3> PUBLISHED_PLAYLISTS{{
    {"audio", PLAYLIST::Audio, "music"},
    {"video", PLAYLIST::Video, "video"},
    {"picture", std::nullopt, "pictures"},
}};

const PublishedPlayList* FindPublished(const CVariant& parameterObject)
{
  const std::string name = parameterObject["playlist"].asString();
  const auto it = std::ranges::find(PUBLISHED_PLAYLISTS, name, &PublishedPlayList::name);
  return it == PUBLISHED_PLAYLISTS.end() ? nullptr : &*it;
}

bool IsMediaAccepted(std::string_view media, const CVariant& item)
{
  if (!item.isMember("media"))
    return true;
  const std::string requested = item["media"].asString();
  // the slideshow shows video as well as pictures
  return requested == "files" || requested == media ||
         (media == "pictures" && requested == "video");
}

const char* ReasonOf(JSONRPC_STATUS status)
{
  switch (status)
  {
    case NotFound:
      return "notFound";
    case Unavailable:
      return "unavailable";
    default:
      return "invalid";
  }
}

CVariant UnresolvedEntry(const CVariant& item, const std::string& reason)
{
  CVariant entry{CVariant::VariantTypeObject};
  entry["item"] = item;
  entry["reason"] = reason;
  return entry;
}

CVariant PlayListTarget(const PublishedPlayList& playList)
{
  return Target("playlist", std::string{playList.name});
}
} // namespace

void CPlaylistOperations::ReadItems(std::string_view media,
                                    const CVariant& itemParam,
                                    CFileItemList& items,
                                    CVariant& unresolved)
{
  std::vector<CVariant> requested;
  if (itemParam.isArray())
    requested.assign(itemParam.begin_array(), itemParam.end_array());
  else
    requested.push_back(itemParam);

  for (CVariant& item : requested)
  {
    // kept as the client wrote it; "media" below is added here, not requested
    const CVariant asked{item};
    bool resolved = false;
    if (IsMediaAccepted(media, item))
    {
      item["media"] = std::string{media};
      // FillFileItemList reports a non-empty list, not whether this item resolved
      const int before = items.Size();
      FillFileItemList(item, items);
      resolved = items.Size() > before;
    }
    if (!resolved)
    {
      CVariant diagnosis;
      unresolved.push_back(
          UnresolvedEntry(asked, ReasonOf(DiagnoseUnresolvedItem(asked, diagnosis))));
    }
  }
}

JSONRPC_STATUS CPlaylistOperations::NothingAdded(const CVariant& unresolved, CVariant& result)
{
  for (auto entry = unresolved.begin_array(); entry != unresolved.end_array(); ++entry)
  {
    if ((*entry)["reason"].asString() != "invalid")
      return DiagnoseUnresolvedItem((*entry)["item"], result);
  }
  return InvalidParams;
}

JSONRPC_STATUS CPlaylistOperations::GetPlaylists(const CVariant& parameterObject, CVariant& result)
{
  result = CVariant(CVariant::VariantTypeArray);
  for (const PublishedPlayList& playList : PUBLISHED_PLAYLISTS)
  {
    CVariant entry(CVariant::VariantTypeObject);
    entry["playlist"] = std::string{playList.name};
    entry["type"] = std::string{playList.name};
    result.append(entry);
  }
  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetProperties(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);

  for (auto it = parameterObject["properties"].begin_array();
       it != parameterObject["properties"].end_array(); ++it)
  {
    const std::string property = it->asString();
    if (property == "type")
    {
      result[property] = playList ? std::string{playList->name} : "unknown";
    }
    else if (property == "size")
    {
      if (playList && playList->type)
        result[property] = CServiceBroker::GetPlayLists()->GetPlayList(*playList->type).Size();
      else if (playList)
        result[property] = std::max(CServiceBroker::GetSlideShowDelegator().NumSlides(), 0);
      else
        result[property] = 0;
    }
    else if (property == "shuffled")
    {
      if (playList && playList->type)
        result[property] = CServiceBroker::GetPlayLists()->IsShuffled(*playList->type);
      else
        result[property] = playList && CServiceBroker::GetSlideShowDelegator().IsShuffled();
    }
    else if (property == "repeat")
    {
      result[property] =
          std::string{playList && playList->type
                          ? CApplicationPlayLists::RepeatName(
                                CServiceBroker::GetPlayLists()->GetRepeat(*playList->type))
                          : CApplicationPlayLists::RepeatName(CApplicationPlayLists::Repeat::Off)};
    }
    else
    {
      return InvalidParams;
    }
  }
  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetItems(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);

  CFileItemList list;
  if (playList && playList->type)
  {
    // copies, so that filling in details for the reply leaves the playlist's own items alone
    const PLAYLIST::CPlayList& source =
        CServiceBroker::GetPlayLists()->GetPlayList(*playList->type);
    const std::vector<PLAYLIST::PlayListEntry> entries = source.GetEntries();
    for (int position = 0; position < static_cast<int>(entries.size()); ++position)
    {
      auto item = std::make_shared<CFileItem>(*entries[position].item);
      const int displayOrder = source.GetPlayOrderPosition(entries[position].id);
      item->SetProperty("playlistposition", position);
      item->SetProperty("playlistdisplayorder", displayOrder < 0 ? position : displayOrder);
      list.Add(std::move(item));
    }
  }
  else if (playList)
  {
    CServiceBroker::GetSlideShowDelegator().GetSlideShowContents(list);
    for (int position = 0; position < list.Size(); ++position)
    {
      list[position]->SetProperty("playlistposition", position);
      list[position]->SetProperty("playlistdisplayorder", position);
    }
  }

  HandleFileItemList("id", true, "items", list, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CPlaylistOperations::Add(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return InvalidParams;

  CFileItemList list;
  CVariant unresolved{CVariant::VariantTypeArray};
  ReadItems(playList->media, parameterObject["item"], list, unresolved);

  int added = 0;
  if (playList->type)
  {
    CServiceBroker::GetPlayLists()->Queue(*playList->type, list,
                                          CApplicationPlayLists::Placement::End);
    added = list.Size();
  }
  else
  {
    CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
    for (const auto& item : list)
    {
      CPictureInfoTag picture;
      if (!picture.Load(item->GetPath()))
      {
        // the file resolved but holds no picture, which the item parameter cannot express
        CVariant asked{CVariant::VariantTypeObject};
        asked["file"] = item->GetPath();
        unresolved.push_back(UnresolvedEntry(asked, "invalid"));
        continue;
      }
      *item->GetPictureInfoTag() = picture;
      slideShow.Add(item.get());
      ++added;
    }
  }

  if (added == 0)
    return NothingAdded(unresolved, result);

  result["added"] = added;
  result["unresolved"] = unresolved;
  return OK;
}

JSONRPC_STATUS CPlaylistOperations::Insert(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return FailedToExecute;
  if (!playList->type)
    return Fail(result, FailedToExecute, Reason::NotApplicable, PlayListTarget(*playList));

  CFileItemList list;
  CVariant unresolved{CVariant::VariantTypeArray};
  ReadItems(playList->media, parameterObject["item"], list, unresolved);
  if (list.IsEmpty())
    return NothingAdded(unresolved, result);

  CServiceBroker::GetPlayLists()->Insert(*playList->type, list,
                                         static_cast<int>(parameterObject["position"].asInteger()));
  result["added"] = list.Size();
  result["unresolved"] = unresolved;
  return OK;
}

JSONRPC_STATUS CPlaylistOperations::SetShuffle(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return InvalidParams;

  const CVariant& shuffle = parameterObject["shuffle"];
  if (playList->type)
    return ApplyShuffle(*playList->type, shuffle);

  if (!CServiceBroker::GetSlideShowDelegator().IsPlaying())
    return Fail(result, FailedToExecute, Reason::NothingPlaying, PlayListTarget(*playList));
  return ShuffleSlideshow(shuffle);
}

JSONRPC_STATUS CPlaylistOperations::SetRepeat(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return FailedToExecute;
  if (!playList->type)
    return Fail(result, FailedToExecute, Reason::NotApplicable, PlayListTarget(*playList));

  return ApplyRepeat(*playList->type, parameterObject["repeat"]);
}

JSONRPC_STATUS CPlaylistOperations::Remove(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return FailedToExecute;
  if (!playList->type)
    return Fail(result, FailedToExecute, Reason::NotApplicable, PlayListTarget(*playList));

  const int position = static_cast<int>(parameterObject["position"].asInteger());
  return CServiceBroker::GetPlayLists()->Remove(*playList->type, position) ? ACK : InvalidParams;
}

JSONRPC_STATUS CPlaylistOperations::Clear(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (playList && playList->type)
  {
    CServiceBroker::GetPlayLists()->Clear(*playList->type);
  }
  else if (playList)
  {
    //! @todo Stop should be a delegator method to avoid GUI coupling! Same goes for other player controls.
    CServiceBroker::GetAppMessenger()->PostMsg(
        TMSG_GUI_ACTION, WINDOW_SLIDESHOW, -1,
        TransferToMessenger(std::make_unique<CAction>(ACTION_STOP)));
    CServiceBroker::GetSlideShowDelegator().Reset();
  }
  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Swap(const CVariant& parameterObject, CVariant& result)
{
  const PublishedPlayList* playList = FindPublished(parameterObject);
  if (!playList)
    return FailedToExecute;
  if (!playList->type)
    return Fail(result, FailedToExecute, Reason::NotApplicable, PlayListTarget(*playList));

  CServiceBroker::GetPlayLists()->Swap(*playList->type,
                                       static_cast<int>(parameterObject["position1"].asInteger()),
                                       static_cast<int>(parameterObject["position2"].asInteger()));
  return ACK;
}
