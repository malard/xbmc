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
#include "ServiceBroker.h"
#include "application/ApplicationComponents.h"
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

using namespace JSONRPC;
using namespace KODI;

namespace
{
std::shared_ptr<CApplicationPlayLists> PlayLists()
{
  return CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>();
}

void GetPlayListItems(PLAYLIST::Side side, CFileItemList& list)
{
  const PLAYLIST::CPlayList& playList = PlayLists()->GetPlayList(side);
  for (int i = 0; i < playList.size(); i++)
  {
    if (const std::shared_ptr<CFileItem> item = playList[i]; item)
      list.Add(std::make_shared<CFileItem>(*item));
  }
}
} // namespace

JSONRPC_STATUS CPlaylistOperations::GetPlaylists(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  result = CVariant(CVariant::VariantTypeArray);
  CVariant playlist = CVariant(CVariant::VariantTypeObject);

  playlist["playlistid"] = static_cast<int>(PLAYLIST::Id::TYPE_MUSIC);
  playlist["type"] = "audio";
  result.append(playlist);

  playlist["playlistid"] = static_cast<int>(PLAYLIST::Id::TYPE_VIDEO);
  playlist["type"] = "video";
  result.append(playlist);

  playlist["playlistid"] = static_cast<int>(PLAYLIST::Id::TYPE_PICTURE);
  playlist["type"] = "picture";
  result.append(playlist);

  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetProperties(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);
  for (unsigned int index = 0; index < parameterObject["properties"].size(); index++)
  {
    std::string propertyName = parameterObject["properties"][index].asString();
    CVariant property;
    JSONRPC_STATUS ret;
    if ((ret = GetPropertyValue(playlistId, propertyName, property)) != OK)
      return ret;

    result[propertyName] = property;
  }

  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetItems(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CFileItemList list;
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);

  switch (playlistId)
  {
    case PLAYLIST::Id::TYPE_VIDEO:
    case PLAYLIST::Id::TYPE_MUSIC:
      GetPlayListItems(*PLAYLIST::SideFromId(playlistId), list);
      break;

    case PLAYLIST::Id::TYPE_PICTURE:
    {
      CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      slideShow.GetSlideShowContents(list);
      break;
    }
    default:
      break;
  }

  HandleFileItemList("id", true, "items", list, parameterObject, result);

  return OK;
}

bool CPlaylistOperations::CheckMediaParameter(PLAYLIST::Id playlistId, const CVariant& itemObject)
{
  if (itemObject.isMember("media") && itemObject["media"].asString().compare("files") != 0)
  {
    if (playlistId == PLAYLIST::Id::TYPE_VIDEO &&
        itemObject["media"].asString().compare("video") != 0)
      return false;
    if (playlistId == PLAYLIST::Id::TYPE_MUSIC &&
        itemObject["media"].asString().compare("music") != 0)
      return false;
    if (playlistId == PLAYLIST::Id::TYPE_PICTURE &&
        itemObject["media"].asString().compare("video") != 0 &&
        itemObject["media"].asString().compare("pictures") != 0)
      return false;
  }
  return true;
}

JSONRPC_STATUS CPlaylistOperations::Add(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);

  CFileItemList list;
  if (!HandleItemsParameter(playlistId, parameterObject["item"], list))
    return InvalidParams;

  switch (playlistId)
  {
    case PLAYLIST::Id::TYPE_VIDEO:
    case PLAYLIST::Id::TYPE_MUSIC:
      PlayLists()->GetPlayList(*PLAYLIST::SideFromId(playlistId)).Add(list);
      break;
    case PLAYLIST::Id::TYPE_PICTURE:
    {
      CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      for (int index = 0; index < list.Size(); index++)
      {
        CPictureInfoTag picture = CPictureInfoTag();
        if (!picture.Load(list[index]->GetPath()))
          continue;

        *list[index]->GetPictureInfoTag() = picture;
        slideShow.Add(list[index].get());
      }
      break;
    }
    default:
      return InvalidParams;
  }

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Insert(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);
  if (playlistId != PLAYLIST::Id::TYPE_MUSIC && playlistId != PLAYLIST::Id::TYPE_VIDEO)
    return FailedToExecute;

  CFileItemList list;
  if (!HandleItemsParameter(playlistId, parameterObject["item"], list))
    return InvalidParams;

  PlayLists()
      ->GetPlayList(*PLAYLIST::SideFromId(playlistId))
      .Insert(list, static_cast<int>(parameterObject["position"].asInteger()));

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Remove(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);
  if (playlistId != PLAYLIST::Id::TYPE_MUSIC && playlistId != PLAYLIST::Id::TYPE_VIDEO)
    return FailedToExecute;

  const PLAYLIST::Side side = *PLAYLIST::SideFromId(playlistId);
  PLAYLIST::CPlayList& playList = PlayLists()->GetPlayList(side);
  int position = (int)parameterObject["position"].asInteger();
  if (PlayLists()->GetPlayingSide() == side && playList.GetCurrentPosition() == position)
    return InvalidParams;

  playList.Remove(position);

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Clear(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);
  switch (playlistId)
  {
    case PLAYLIST::Id::TYPE_MUSIC:
    case PLAYLIST::Id::TYPE_VIDEO:
      PlayLists()->GetPlayList(*PLAYLIST::SideFromId(playlistId)).Clear();
      break;

    case PLAYLIST::Id::TYPE_PICTURE:
    {
      CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
      //! @todo: Stop should be a delegator method to void GUI coupling! Same goes for other player controls.
      CServiceBroker::GetAppMessenger()->PostMsg(TMSG_GUI_ACTION, WINDOW_SLIDESHOW, -1,
                                                 static_cast<void*>(new CAction(ACTION_STOP)));
      slideShow.Reset();
      break;
    }
    default:
      break;
  }

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Swap(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  PLAYLIST::Id playlistId = GetPlaylist(parameterObject["playlistid"]);
  if (playlistId != PLAYLIST::Id::TYPE_MUSIC && playlistId != PLAYLIST::Id::TYPE_VIDEO)
    return FailedToExecute;

  PlayLists()
      ->GetPlayList(*PLAYLIST::SideFromId(playlistId))
      .Swap(static_cast<int>(parameterObject["position1"].asInteger()),
            static_cast<int>(parameterObject["position2"].asInteger()));

  return ACK;
}

PLAYLIST::Id CPlaylistOperations::GetPlaylist(const CVariant& playlist)
{
  PLAYLIST::Id playlistId =
      PLAYLIST::Id{playlist.asInteger32(static_cast<int>(PLAYLIST::Id::TYPE_NONE))};
  if (playlistId != PLAYLIST::Id::TYPE_NONE)
    return playlistId;

  return PLAYLIST::Id::TYPE_NONE;
}

JSONRPC_STATUS CPlaylistOperations::GetPropertyValue(PLAYLIST::Id playlistId,
                                                     const std::string& property,
                                                     CVariant& result)
{
  if (property == "type")
  {
    switch (playlistId)
    {
      case PLAYLIST::Id::TYPE_MUSIC:
        result = "audio";
        break;

      case PLAYLIST::Id::TYPE_VIDEO:
        result = "video";
        break;

      case PLAYLIST::Id::TYPE_PICTURE:
        result = "pictures";
        break;

      default:
        result = "unknown";
        break;
    }
  }
  else if (property == "size")
  {
    switch (playlistId)
    {
      case PLAYLIST::Id::TYPE_MUSIC:
      case PLAYLIST::Id::TYPE_VIDEO:
        result = PlayLists()->GetPlayList(*PLAYLIST::SideFromId(playlistId)).size();
        break;
      case PLAYLIST::Id::TYPE_PICTURE:
      {
        CSlideShowDelegator& slideShow = CServiceBroker::GetSlideShowDelegator();
        const int numSlides = slideShow.NumSlides();
        if (numSlides < 0)
          result = 0;
        else
          result = numSlides;
        break;
      }
      default:
      {
        result = 0;
        break;
      }
    }
  }
  else
    return InvalidParams;

  return OK;
}

bool CPlaylistOperations::HandleItemsParameter(PLAYLIST::Id playlistId,
                                               const CVariant& itemParam,
                                               CFileItemList& items)
{
  std::vector<CVariant> vecItems;
  if (itemParam.isArray())
    vecItems.assign(itemParam.begin_array(), itemParam.end_array());
  else
    vecItems.push_back(itemParam);

  bool success = false;
  for (auto& itemIt : vecItems)
  {
    if (!CheckMediaParameter(playlistId, itemIt))
      continue;

    switch (playlistId)
    {
      case PLAYLIST::Id::TYPE_VIDEO:
        itemIt["media"] = "video";
        break;
      case PLAYLIST::Id::TYPE_MUSIC:
        itemIt["media"] = "music";
        break;
      case PLAYLIST::Id::TYPE_PICTURE:
        itemIt["media"] = "pictures";
        break;
      default:
        break;
    }

    success |= FillFileItemList(itemIt, items);
  }

  return success;
}
