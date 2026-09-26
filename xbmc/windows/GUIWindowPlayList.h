/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dialogs/GUIDialogContextMenu.h"
#include "playlists/PlayListTypes.h"

#include <memory>
#include <string>

class CAction;
class CApplicationPlayLists;
class CGUIMessage;

/*!
 * \brief The window showing what one playlist holds, and moving, removing, saving and playing its
 * entries. Base is the media window of the kind of media the playlist plays.
 */
template<typename Base>
class CGUIWindowPlayList : public Base
{
public:
  CGUIWindowPlayList(int id, const std::string& xmlFile, KODI::PLAYLIST::Type type);
  ~CGUIWindowPlayList() override;

  bool OnMessage(CGUIMessage& message) override;
  bool OnAction(const CAction& action) override;
  bool OnBack(int actionID) override;

protected:
  using Base::GetID;

  bool GoParentFolder() override { return false; }
  bool OnPlayMedia(int iItem, const std::string& player = "") override;
  void UpdateButtons() override;
  void GetContextButtons(int itemNumber, CContextButtons& buttons) override;
  bool OnContextButton(int itemNumber, CONTEXT_BUTTON button) override;

  /*!
   * \brief Start the playlist at this position; party mode is handled before this is called.
   */
  virtual void PlayEntry(int iItem, const std::string& player);

  /*!
   * \brief Stop filling in details of the listed items before the list is changed.
   * \return Whether it was running, and so should be started again afterwards.
   */
  virtual bool StopLoadingItems() { return false; }
  virtual void StartLoadingItems() {}

  const KODI::PLAYLIST::Type m_type;
  const std::shared_ptr<CApplicationPlayLists> m_playLists;

private:
  void OnMove(int iItem, int iAction);
  void MoveItem(int iStart, int iDest);
  bool MoveCurrentPlayListItem(int iItem, int iAction, bool bUpdate = true);
  void RemovePlayListItem(int iItem);
  void ClearPlayList();
  void SavePlayList();
  void ToggleShuffle();
  void CycleRepeat();

  int m_movingFrom{-1};
};
