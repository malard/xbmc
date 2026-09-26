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
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayLists.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListFactory.h"
#include "playlists/PlayListFileItemClassify.h"
#include "playlists/PlayListShuffle.h"
#include "utils/URIUtils.h"

using namespace KODI;

namespace XBMCAddon
{
  namespace xbmc
  {
    //! @todo need a means to check for a valid construction
    //!  either by throwing an exception or by an "isValid" check
    PlayList::PlayList(int playList) :
      iPlayList(playList), pPlayList(NULL)
    {
      // we do not create our own playlist, just using the Video and Audio ones
      if (PLAYLIST::Id{iPlayList} != PLAYLIST::Id::TYPE_MUSIC &&
          PLAYLIST::Id{iPlayList} != PLAYLIST::Id::TYPE_VIDEO)
        throw PlayListException("PlayList does not exist");

      pPlayList = &CServiceBroker::GetAppComponents()
                       .GetComponent<CApplicationPlayLists>()
                       ->GetPlayList(*PLAYLIST::SideFromId(PLAYLIST::Id{playList}));
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

      pPlayList->Insert(items, index);
    }

    bool PlayList::load(const char* cFileName)
    {
      CFileItem item(cFileName);
      item.SetPath(cFileName);

      if (PLAYLIST::IsPlayList(item))
      {
        // load playlist and copy al items to existing playlist

        // load a playlist like .m3u, .pls
        // first get correct factory to load playlist
        std::unique_ptr<PLAYLIST::CPlayList> pPlayList(PLAYLIST::CPlayListFactory::Create(item));
        if (nullptr != pPlayList)
        {
          // load it
          if (!pPlayList->Load(item.GetPath()))
            //hmmm unable to load playlist?
            return false;

          // clear current playlist
          this->pPlayList->Clear();

          // add each item of the playlist to ours
          for (int i=0; i < pPlayList->size(); ++i)
          {
            CFileItemPtr playListItem =(*pPlayList)[i];
            if (playListItem->GetLabel().empty())
              playListItem->SetLabel(URIUtils::GetFileName(playListItem->GetPath()));

            this->pPlayList->Add(playListItem);
          }
        }
      }
      else
        // filename is not a valid playlist
        throw PlayListException("Not a valid playlist");

      return true;
    }

    void PlayList::remove(const char* filename)
    {
      pPlayList->Remove(filename);
    }

    void PlayList::clear()
    {
      pPlayList->Clear();
    }

    int PlayList::size()
    {
      return pPlayList->size();
    }

    void PlayList::shuffle()
    {
      pPlayList->SetShuffle(std::make_unique<PLAYLIST::CPlayListRandomShuffle>());
    }

    void PlayList::unshuffle()
    {
      pPlayList->SetShuffled(false);
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

      CFileItemPtr ptr((*pPlayList)[pos]);

      return new XBMCAddon::xbmcgui::ListItem(ptr);
    }
  }
}

