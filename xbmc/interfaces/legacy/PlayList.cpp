/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayList.h"

#include "FileItemList.h"
#include "ServiceBroker.h"
#include "application/ApplicationPlayLists.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListEntryRules.h"
#include "playlists/PlayListFileItemClassify.h"
#include "utils/URIUtils.h"

using namespace KODI;

namespace
{
PLAYLIST::Type TypeOf(int playList)
{
  return *XBMCAddon::xbmc::PlayListFromId(playList);
}
} // namespace

namespace XBMCAddon
{
  namespace xbmc
  {
  std::optional<PLAYLIST::Type> PlayListFromId(int playList)
  {
    if (playList == PLAYLIST_MUSIC_ID)
      return PLAYLIST::Audio;
    if (playList == PLAYLIST_VIDEO_ID)
      return PLAYLIST::Video;
    return std::nullopt;
  }

    PlayList::PlayList(int playList) :
      iPlayList(playList), pPlayList(NULL)
    {
      const std::optional<PLAYLIST::Type> type = PlayListFromId(playList);
      if (!type)
        throw PlayListException("PlayList does not exist");

      // a Python playlist wraps the Video or Audio playlist rather than owning one
      pPlayList = &CServiceBroker::GetPlayLists()->GetPlayList(*type);
      iPlayList = playList;
    }

    PlayList::~PlayList() = default;

    void PlayList::add(const String& url, XBMCAddon::xbmcgui::ListItem* listitem, int index)
    {
      CFileItemList items;

      if (listitem != NULL)
      {
        // an optional listitem was passed
        // set m_strPath to the passed url
        listitem->item->SetPath(url);

        items.Add(listitem->item);
      }
      else
      {
        CFileItemPtr item(new CFileItem(url, false));
        item->SetLabel(url);

        items.Add(item);
      }

      CServiceBroker::GetPlayLists()->Insert(TypeOf(iPlayList), items, index);
    }

    bool PlayList::load(const char* cFileName)
    {
      const auto item = std::make_shared<CFileItem>(cFileName, false);

      if (PLAYLIST::HoldsEntries(*item))
      {
        // replace this playlist's contents with the file's entries
        PLAYLIST::CEntriesAsListed asListed;
        CFileItemList items;
        CApplicationPlayLists::ExpandToEntries(item, asListed, nullptr, items);
        if (items.IsEmpty())
          return false;

        for (const auto& entry : items)
        {
          if (entry->GetLabel().empty())
            entry->SetLabel(URIUtils::GetFileName(entry->GetPath()));
        }
        CServiceBroker::GetPlayLists()->Replace(TypeOf(iPlayList), items);
      }
      else
        // filename is not a valid playlist
        throw PlayListException("Not a valid playlist");

      return true;
    }

    void PlayList::remove(const char* filename)
    {
      CServiceBroker::GetPlayLists()->Remove(TypeOf(iPlayList), std::string{filename});
    }

    void PlayList::clear()
    {
      CServiceBroker::GetPlayLists()->Clear(TypeOf(iPlayList));
    }

    int PlayList::size()
    {
      return pPlayList->Size();
    }

    void PlayList::shuffle()
    {
      CServiceBroker::GetPlayLists()->SetShuffle(TypeOf(iPlayList), true,
                                                 CApplicationPlayLists::Persist::No);
    }

    void PlayList::unshuffle()
    {
      CServiceBroker::GetPlayLists()->SetShuffle(TypeOf(iPlayList), false,
                                                 CApplicationPlayLists::Persist::No);
    }

    int PlayList::getposition()
    {
      return pPlayList->GetCurrentPosition();
    }

    XBMCAddon::xbmcgui::ListItem* PlayList::operator [](long i)
    {
      int iPlayListSize = size();

      long pos = i;
      if (pos < 0) pos += iPlayListSize;

      if (pos < 0 || pos >= iPlayListSize)
        throw PlayListException("array out of bound");

      // a copy: the playlist owns its items, and nothing outside it changes one in place
      const std::shared_ptr<CFileItem> item = (*pPlayList)[pos];
      if (!item)
        throw PlayListException("array out of bound");
      return new XBMCAddon::xbmcgui::ListItem(std::make_shared<CFileItem>(*item));
    }
  }
}

