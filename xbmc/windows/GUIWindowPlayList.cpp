/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIWindowPlayList.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIUserMessages.h"
#include "PartyModeManager.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayLists.h"
#include "dialogs/GUIDialogSmartPlaylistEditor.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIKeyboardFactory.h"
#include "guilib/GUIWindowManager.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "music/MusicFileItemClassify.h"
#include "music/tags/MusicInfoTag.h"
#include "music/windows/GUIWindowMusicBase.h"
#include "playlists/PlayList.h"
#include "playlists/PlayListM3U.h"
#include "profiles/ProfileManager.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "video/windows/GUIWindowVideoBase.h"
#include "view/GUIViewState.h"

using namespace KODI;

namespace
{
constexpr int CONTROL_BTNVIEWASICONS = 2;

constexpr int CONTROL_BTNSHUFFLE = 20;
constexpr int CONTROL_BTNSAVE = 21;
constexpr int CONTROL_BTNCLEAR = 22;

constexpr int CONTROL_BTNPLAY = 23;
constexpr int CONTROL_BTNNEXT = 24;
constexpr int CONTROL_BTNPREVIOUS = 25;
constexpr int CONTROL_BTNREPEAT = 26;

const char* DirectoryPath(PLAYLIST::Type type)
{
  return type == PLAYLIST::Audio ? "playlistmusic://" : "playlistvideo://";
}
} // namespace

template<typename Base>
CGUIWindowPlayList<Base>::CGUIWindowPlayList(int id,
                                             const std::string& xmlFile,
                                             PLAYLIST::Type type)
  : Base(id, xmlFile),
    m_type(type),
    m_playLists(CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayLists>())
{
}

template<typename Base>
CGUIWindowPlayList<Base>::~CGUIWindowPlayList() = default;

template<typename Base>
bool CGUIWindowPlayList<Base>::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_PLAYLISTPLAYER_REPEAT:
      UpdateButtons();
      break;

    case GUI_MSG_PLAYLISTPLAYER_RANDOM:
    case GUI_MSG_PLAYLIST_CHANGED:
    {
      UpdateButtons();

      if (this->m_vecItemsUpdating)
      {
        CLog::LogF(LOGWARNING, "updating in progress");
        return true;
      }
      typename Base::CUpdateGuard guard(this->m_vecItemsUpdating);

      this->Refresh(true);

      if (this->m_viewControl.HasControl(this->m_iLastControl) && this->m_vecItems->IsEmpty())
      {
        this->m_iLastControl = CONTROL_BTNVIEWASICONS;
        SET_CONTROL_FOCUS(this->m_iLastControl, 0);
      }
      break;
    }

    case GUI_MSG_WINDOW_DEINIT:
      StopLoadingItems();
      m_movingFrom = -1;
      break;

    case GUI_MSG_WINDOW_INIT:
    {
      this->m_vecItems->SetPath(DirectoryPath(m_type));

      if (!Base::OnMessage(message))
      {
        return false;
      }

      if (this->m_vecItems->IsEmpty())
      {
        this->m_iLastControl = CONTROL_BTNVIEWASICONS;
        SET_CONTROL_FOCUS(this->m_iLastControl, 0);
      }

      if (m_playLists->IsPlaying(m_type))
      {
        if (const int position = m_playLists->GetPlayingPosition(m_type); position >= 0)
        {
          this->m_viewControl.SetSelectedItem(position);
        }
      }
      return true;
    }

    case GUI_MSG_CLICKED:
    {
      const int control = message.GetSenderId();
      if (control == CONTROL_BTNSHUFFLE)
      {
        ToggleShuffle();
      }
      else if (control == CONTROL_BTNSAVE)
      {
        StopLoadingItems();
        SavePlayList();
      }
      else if (control == CONTROL_BTNCLEAR)
      {
        StopLoadingItems();
        ClearPlayList();
      }
      else if (control == CONTROL_BTNPLAY)
      {
        if (this->m_guiState)
        {
          this->m_guiState->SetPlaylistDirectory(DirectoryPath(m_type));
        }
        m_playLists->Play(m_type, this->m_viewControl.GetSelectedItem());
        UpdateButtons();
      }
      else if (control == CONTROL_BTNNEXT)
      {
        m_playLists->PlayNext(m_type);
      }
      else if (control == CONTROL_BTNPREVIOUS)
      {
        m_playLists->PlayPrevious(m_type);
      }
      else if (control == CONTROL_BTNREPEAT)
      {
        CycleRepeat();
      }
      else if (this->m_viewControl.HasControl(control))
      {
        const int action = message.GetParam1();
        if (action == ACTION_DELETE_ITEM || action == ACTION_MOUSE_MIDDLE_CLICK)
        {
          RemovePlayListItem(this->m_viewControl.GetSelectedItem());
        }
      }
      break;
    }
  }
  return Base::OnMessage(message);
}

template<typename Base>
bool CGUIWindowPlayList<Base>::OnAction(const CAction& action)
{
  switch (action.GetID())
  {
    case ACTION_PARENT_DIR:
      return true;

    case ACTION_SHOW_PLAYLIST:
      CServiceBroker::GetGUI()->GetWindowManager().PreviousWindow();
      return true;

    case ACTION_MOVE_ITEM_UP:
    case ACTION_MOVE_ITEM_DOWN:
    {
      const int item = this->m_viewControl.HasControl(this->GetFocusedControlID())
                           ? this->m_viewControl.GetSelectedItem()
                           : -1;
      OnMove(item, action.GetID());
      return true;
    }

    case ACTION_PLAYER_PLAY:
      if (this->m_viewControl.HasControl(this->GetFocusedControlID()))
      {
        return OnPlayMedia(this->m_viewControl.GetSelectedItem());
      }
      break;

    default:
      break;
  }
  return Base::OnAction(action);
}

template<typename Base>
bool CGUIWindowPlayList<Base>::OnBack(int actionID)
{
  // the media window's back goes up a folder, and a playlist has none
  if (actionID == ACTION_NAV_BACK)
  {
    return CGUIWindow::OnBack(actionID);
  }
  return Base::OnBack(actionID);
}

template<typename Base>
bool CGUIWindowPlayList<Base>::OnPlayMedia(int iItem, const std::string& player)
{
  if (iItem < 0 || iItem >= this->m_vecItems->Size())
  {
    return false;
  }

  if (g_partyModeManager.IsEnabled())
  {
    g_partyModeManager.Play(iItem);
  }
  else
  {
    PlayEntry(iItem, player);
  }
  return true;
}

template<typename Base>
void CGUIWindowPlayList<Base>::PlayEntry(int iItem, const std::string& player)
{
  if (this->m_guiState)
  {
    this->m_guiState->SetPlaylistDirectory(DirectoryPath(m_type));
  }
  m_playLists->Play(m_type, iItem, player);
}

template<typename Base>
void CGUIWindowPlayList<Base>::UpdateButtons()
{
  Base::UpdateButtons();

  // party mode owns the list
  if (!this->m_vecItems->IsEmpty() && !g_partyModeManager.IsEnabled())
  {
    CONTROL_ENABLE(CONTROL_BTNSHUFFLE);
    CONTROL_ENABLE(CONTROL_BTNSAVE);
    CONTROL_ENABLE(CONTROL_BTNCLEAR);
    CONTROL_ENABLE(CONTROL_BTNREPEAT);
    CONTROL_ENABLE(CONTROL_BTNPLAY);

    if (m_playLists->IsPlaying(m_type))
    {
      CONTROL_ENABLE(CONTROL_BTNNEXT);
      CONTROL_ENABLE(CONTROL_BTNPREVIOUS);
    }
    else
    {
      CONTROL_DISABLE(CONTROL_BTNNEXT);
      CONTROL_DISABLE(CONTROL_BTNPREVIOUS);
    }
  }
  else
  {
    CONTROL_DISABLE(CONTROL_BTNSHUFFLE);
    CONTROL_DISABLE(CONTROL_BTNSAVE);
    CONTROL_DISABLE(CONTROL_BTNCLEAR);
    CONTROL_DISABLE(CONTROL_BTNREPEAT);
    CONTROL_DISABLE(CONTROL_BTNPLAY);
    CONTROL_DISABLE(CONTROL_BTNNEXT);
    CONTROL_DISABLE(CONTROL_BTNPREVIOUS);
  }

  CONTROL_DESELECT(CONTROL_BTNSHUFFLE);
  if (m_playLists->IsShuffled(m_type))
  {
    CONTROL_SELECT(CONTROL_BTNSHUFFLE);
  }

  using enum CApplicationPlayLists::Repeat;
  uint32_t repeatLabel = 597; // Repeat: All
  switch (m_playLists->GetRepeat(m_type))
  {
    case Off:
      repeatLabel = 595;
      break;
    case One:
      repeatLabel = 596;
      break;
    case All:
      break;
  }
  SET_CONTROL_LABEL(CONTROL_BTNREPEAT,
                    CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(repeatLabel));
}

template<typename Base>
void CGUIWindowPlayList<Base>::GetContextButtons(int itemNumber, CContextButtons& buttons)
{
  if (itemNumber >= 0 && itemNumber < this->m_vecItems->Size())
  {
    const int itemPlaying = m_playLists->GetPlayingPosition(m_type);
    const bool partyMode = g_partyModeManager.IsEnabled();

    if (m_movingFrom >= 0)
    {
      // party mode keeps what has played above what is playing
      if (itemNumber != m_movingFrom && (!partyMode || itemNumber > itemPlaying))
      {
        buttons.Add(CONTEXT_BUTTON_MOVE_HERE, 13252);
      }
      buttons.Add(CONTEXT_BUTTON_CANCEL_MOVE, 13253);
    }
    else
    {
      if (itemNumber > (partyMode ? itemPlaying + 1 : 0))
      {
        buttons.Add(CONTEXT_BUTTON_MOVE_ITEM_UP, 13332);
      }
      if (itemNumber + 1 < this->m_vecItems->Size())
      {
        buttons.Add(CONTEXT_BUTTON_MOVE_ITEM_DOWN, 13333);
      }
      if (!partyMode || itemNumber != itemPlaying)
      {
        buttons.Add(CONTEXT_BUTTON_MOVE_ITEM, 13251);
      }
      if (itemNumber != itemPlaying)
      {
        buttons.Add(CONTEXT_BUTTON_DELETE, 1210); // Remove
      }
    }
  }

  if (g_partyModeManager.IsEnabled())
  {
    buttons.Add(CONTEXT_BUTTON_EDIT_PARTYMODE, 21439);
    buttons.Add(CONTEXT_BUTTON_CANCEL_PARTYMODE, 588);
  }
}

template<typename Base>
bool CGUIWindowPlayList<Base>::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  switch (button)
  {
    case CONTEXT_BUTTON_MOVE_ITEM:
      m_movingFrom = itemNumber;
      return true;

    case CONTEXT_BUTTON_MOVE_HERE:
      if (m_movingFrom >= 0)
      {
        MoveItem(m_movingFrom, itemNumber);
      }
      m_movingFrom = -1;
      return true;

    case CONTEXT_BUTTON_CANCEL_MOVE:
      m_movingFrom = -1;
      return true;

    case CONTEXT_BUTTON_MOVE_ITEM_UP:
      OnMove(itemNumber, ACTION_MOVE_ITEM_UP);
      return true;

    case CONTEXT_BUTTON_MOVE_ITEM_DOWN:
      OnMove(itemNumber, ACTION_MOVE_ITEM_DOWN);
      return true;

    case CONTEXT_BUTTON_DELETE:
      RemovePlayListItem(itemNumber);
      return true;

    case CONTEXT_BUTTON_CANCEL_PARTYMODE:
      g_partyModeManager.Disable();
      return true;

    case CONTEXT_BUTTON_EDIT_PARTYMODE:
    {
      std::string rules = CServiceBroker::GetSettingsComponent()->GetProfileManager()->GetUserDataItem(
          m_type == PLAYLIST::Audio ? "PartyMode.xsp" : "PartyMode-Video.xsp");
      if (CGUIDialogSmartPlaylistEditor::EditPlaylist(rules))
      {
        g_partyModeManager.Disable();
        g_partyModeManager.Enable(m_type == PLAYLIST::Audio ? PartyModeContext::MUSIC
                                                            : PartyModeContext::VIDEO);
      }
      return true;
    }

    default:
      break;
  }
  return Base::OnContextButton(itemNumber, button);
}

template<typename Base>
void CGUIWindowPlayList<Base>::OnMove(int iItem, int iAction)
{
  if (iItem < 0 || iItem >= this->m_vecItems->Size())
  {
    return;
  }

  const bool restart = StopLoadingItems();
  MoveCurrentPlayListItem(iItem, iAction);
  if (restart)
  {
    StartLoadingItems();
  }
}

template<typename Base>
void CGUIWindowPlayList<Base>::MoveItem(int iStart, int iDest)
{
  if (iStart < 0 || iStart >= this->m_vecItems->Size() || iDest < 0 ||
      iDest >= this->m_vecItems->Size())
  {
    return;
  }

  const int action = iStart < iDest ? ACTION_MOVE_ITEM_DOWN : ACTION_MOVE_ITEM_UP;
  const int direction = iStart < iDest ? 1 : -1;

  const bool restart = StopLoadingItems();

  // a swap refused, such as past what party mode is playing, ends the move there
  for (int i = iStart; i != iDest && MoveCurrentPlayListItem(i, action, false); i += direction)
  {
  }
  this->Refresh();

  if (restart)
  {
    StartLoadingItems();
  }
}

template<typename Base>
bool CGUIWindowPlayList<Base>::MoveCurrentPlayListItem(int iItem, int iAction, bool bUpdate)
{
  const int destination = iAction == ACTION_MOVE_ITEM_UP ? iItem - 1 : iItem + 1;
  if (!m_playLists->GetPlayList(m_type).Swap(iItem, destination))
  {
    return false;
  }
  if (bUpdate)
  {
    this->Refresh();
  }
  return true;
}

template<typename Base>
void CGUIWindowPlayList<Base>::RemovePlayListItem(int iItem)
{
  if (iItem < 0 || iItem >= this->m_vecItems->Size())
  {
    return;
  }

  // what is playing stays
  if (m_playLists->IsPlaying(m_type) && m_playLists->GetPlayingPosition(m_type) == iItem)
  {
    return;
  }

  m_playLists->GetPlayList(m_type).Remove(iItem);
  this->Refresh();

  if (this->m_vecItems->IsEmpty())
  {
    SET_CONTROL_FOCUS(CONTROL_BTNVIEWASICONS, 0);
  }
  else
  {
    this->m_viewControl.SetSelectedItem(iItem);
  }

  g_partyModeManager.OnSongChange();
}

template<typename Base>
void CGUIWindowPlayList<Base>::ClearPlayList()
{
  this->ClearFileItems();
  m_playLists->GetPlayList(m_type).Clear();
  if (m_playLists->GetPlayingType() == m_type)
  {
    m_playLists->SetPlayingType(std::nullopt);
  }
  this->Refresh();
  SET_CONTROL_FOCUS(CONTROL_BTNVIEWASICONS, 0);
}

template<typename Base>
void CGUIWindowPlayList<Base>::SavePlayList()
{
  std::string name;
  if (!CGUIKeyboardFactory::ShowAndGetInput(
          name, CVariant{CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(16012)},
          false))
  {
    return;
  }

  const std::string path = URIUtils::AddFileToFolder(
      CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(
          CSettings::SETTING_SYSTEM_PLAYLISTSPATH),
      m_type == PLAYLIST::Audio ? "music" : "video",
      CUtil::MakeLegalFileName(std::move(name)) + ".m3u8");

  PLAYLIST::CPlayListM3U playlist;
  for (const auto& item : *this->m_vecItems)
  {
    // a saved playlist outlives the music database, so it names the file itself
    if (MUSIC::IsMusicDb(*item))
    {
      auto file = std::make_shared<CFileItem>(*item);
      file->SetPath(item->GetMusicInfoTag()->GetURL());
      playlist.Add(file);
    }
    else
    {
      playlist.Add(item);
    }
  }
  CLog::LogF(LOGDEBUG, "saving [{}]", path);
  playlist.Save(path);
}

template<typename Base>
void CGUIWindowPlayList<Base>::ToggleShuffle()
{
  if (g_partyModeManager.IsEnabled())
  {
    return;
  }

  m_playLists->SetShuffle(m_type, !m_playLists->IsShuffled(m_type));
  if (m_type == PLAYLIST::Audio)
  {
    CMediaSettings::GetInstance().SetMusicPlaylistShuffled(m_playLists->IsShuffled(m_type));
  }
  else
  {
    CMediaSettings::GetInstance().SetVideoPlaylistShuffled(m_playLists->IsShuffled(m_type));
  }
  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
  UpdateButtons();
  this->Refresh();
}

template<typename Base>
void CGUIWindowPlayList<Base>::CycleRepeat()
{
  using enum CApplicationPlayLists::Repeat;
  switch (m_playLists->GetRepeat(m_type))
  {
    case Off:
      m_playLists->SetRepeat(m_type, All);
      break;
    case All:
      m_playLists->SetRepeat(m_type, One);
      break;
    case One:
      m_playLists->SetRepeat(m_type, Off);
      break;
  }

  const bool repeatAll = m_playLists->GetRepeat(m_type) == All;
  if (m_type == PLAYLIST::Audio)
  {
    CMediaSettings::GetInstance().SetMusicPlaylistRepeat(repeatAll);
  }
  else
  {
    CMediaSettings::GetInstance().SetVideoPlaylistRepeat(repeatAll);
  }
  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();
  UpdateButtons();
}

template class CGUIWindowPlayList<CGUIWindowMusicBase>;
template class CGUIWindowPlayList<CGUIWindowVideoBase>;
