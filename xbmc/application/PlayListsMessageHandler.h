/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "application/IApplicationComponent.h"
#include "messaging/IMessageTarget.h"
#include "playlists/PlayListTypes.h"

#include <optional>

class CApplicationPlayLists;

/*!
 * \brief Turns the playlist messages into calls on the playlists, and answers the user for them.
 */
class CPlayListsMessageHandler : public IApplicationComponent,
                                 public KODI::MESSAGING::IMessageTarget
{
public:
  explicit CPlayListsMessageHandler(CApplicationPlayLists& playLists);

  int GetMessageMask() override;
  void OnApplicationMessage(KODI::MESSAGING::ThreadMessage* pMsg) override;

private:
  //! A message's playlist: the one named, or with -1 the playing one.
  std::optional<KODI::PLAYLIST::Type> NamedOrPlaying(int type) const;
  void OnPlayItem(KODI::MESSAGING::ThreadMessage* pMsg);
  void OnPlayItems(KODI::MESSAGING::ThreadMessage* pMsg);

  CApplicationPlayLists& m_playLists;
};
