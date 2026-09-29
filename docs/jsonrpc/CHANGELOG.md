# JSON-RPC API changelog

The version `JSONRPC.Version` reports, and `openrpc.json` and `asyncapi.json` carry. It moves
independently of Kodi's version. Versions before 15 are recorded only in the commit history of
`xbmc/interfaces/json-rpc/`.

## 15.0.0

Breaking. Changes since 13.5.0 (Kodi 21). [MIGRATING-v13-v14-to-v15.md](MIGRATING-v13-v14-to-v15.md)
covers every break. 13.200.0 (22.0b2) already has the library-id `NotFound`, `Player.Open`
`Unavailable` and BCP 47 stream languages.

### Breaking

- Names are camelCase: every property, parameter and result name, and every value of an
  enum the API defines for itself (`movieId`, `playCount`, `canChangeSpeed`). Shared Kodi
  vocabularies keep their spelling: media and content types, sort methods, smart playlist
  fields, window and action names, add-on types.
- `WriteSetting` is a real permission, required by `Settings.SetSettingValue`,
  `Settings.ResetSettingValue`, `Settings.SetSkinSettingValue` and `Settings.SetLevel`. HTTP `GET`
  does not hold it. An unknown permission name is an error.
- `JSONRPC.Introspect` answers in JSON Schema 2020-12, not draft-03.
- PVR channel `uniqueid` is `channeluid`.
- `Playlist.Add` and `Playlist.Insert` return `Playlist.AddResult`, not `"OK"`.
- `currentaudiostream`, `currentvideostream` and `currentsubtitle` are `null`, not `{}`, when
  nothing is selected.
- `Files.GetDirectory` keeps a folder that matches a library item as a `directory` at its own path;
  answers `properties` under `"media": "files"`; resolves tv show folders.
- `NotFound` (-32098), `Unavailable` (-32097), `AccessDenied` (-32096) and `InternalError` (-32603)
  replace `InvalidParams` where the request was valid but could not be served: library and PVR ids,
  `Files.*`, `Player.Open`, `VideoLibrary.Scan`, `VideoLibrary.Clean`,
  `AudioLibrary.GetArtistDetails` and `Settings.*SettingValue`.
- Playlists are named `video`, `audio` or `picture`, not numbered, in every `Playlist` method and
  notification and in `Player.Open`.
- `Player` methods take an optional `playlist` in place of the required `playerid`. With none they
  act on everything playing; an idle named playlist is `FailedToExecute`.
- `Player.GetActivePlayers` is removed. `Player.GetProperties` reports `playlist` and `playertype`.
- `Player.On*` notifications carry `players` in place of `playerid`.
- `Playlist.GetItems` returns `Playlist.Entry` with `position` and `displayorder`.
  `Player.GetProperties` reports `displayorder`.
- A hidden setting is read and written like any other.
- Stream languages are BCP 47 tags (`en`, `en-AU`), not ISO 639-2/B (`eng`).
- `XBMC.GetInfoLabels` and `XBMC.GetInfoBooleans` are removed; `GUI.GetInfoLabels` and
  `GUI.GetInfoBooleans` are the same methods.
- `Textures.GetTextures` and `Textures.RemoveTexture` are `Application.GetTextures` and
  `Application.RemoveTexture`: a namespace names what it acts on, and the texture cache is Kodi's
  internals.
- `Player.SetShuffle` and `Player.SetRepeat` are removed, and `Player.GetProperties` and
  `Player.OnPropertiesChanged` no longer carry `shuffled` or `repeat`. Shuffle and repeat belong to
  the playlist: `Playlist.SetShuffle`, `Playlist.SetRepeat`, `Playlist.GetProperties` and
  `Playlist.OnPropertiesChanged`.
- `Application.SetVolume` and `Application.SetMute` are removed, and `Application.GetProperties`
  no longer reports `volume`, `muted` or `contentrect`. `Player` is what is seen and heard: its
  `GetProperties` answers `volume`, `muted` and `contentrect` whether or not anything plays,
  `Player.SetProperties` sets `volume` and `muted`, and `Player.VolumeUp` and `Player.VolumeDown`
  step the volume.
- `Player.OnPropertyChanged` and `Playlist.OnPropertyChanged` are `Player.OnPropertiesChanged` and
  `Playlist.OnPropertiesChanged`, carrying what changed under `data.properties` in place of
  `data.property`. `Application.OnVolumeChanged` is removed: a volume or mute change is
  `Player.OnPropertiesChanged` carrying `volume` or `muted`, whichever changed, and no `player`.
- `Player.OnPause`, `Player.OnResume`, `Player.OnSpeedChanged` and `Player.OnSeek` are removed:
  each is `Player.OnPropertiesChanged`, carrying `speed` for the first three and `time` for a seek,
  with `player`. A pause is `speed` 0.
- Six failures answer with the status that fits, each with its reason in `error.data`: an
  unknown add-on id in `Addons.GetAddonDetails`, `Addons.SetAddonEnabled` and
  `Addons.ExecuteAddon` is `NotFound` (`no-such-addon`), not `InvalidParams`; `Player.GetChapters`
  with no video playing is `FailedToExecute` (`nothing-playing` or `not-applicable`), not
  `InvalidParams`; `Player.Open` with an unknown `broadcastId`, `channelId` or `recordingId` is
  `NotFound` (`no-such-item`), and with a PVR recording path nothing has, `NotFound`
  (`no-such-path`), not `InvalidParams`; `PVR.Record` on the `current` channel with no channel
  playing is `FailedToExecute` (`nothing-playing` or `not-applicable`), not `InternalError`;
  `Files.GetDirectory` on a directory that does not exist is `NotFound` (`no-such-path`), not
  `Unavailable`, which now means only that the source cannot be reached.

### Deprecated

Marked `"deprecated": true`; each names its replacement.

- `VideoLibrary.RefreshMovie`, `RefreshTVShow`, `RefreshEpisode`, `RefreshMusicVideo`: use
  `VideoLibrary.Refresh`.
- `PVR.Details.Broadcast` `seasonnum`, `episodenum`, `isplayable`: use `season`, `episode`,
  `PVR.GetBroadcastIsPlayable`.
- `Player.Open` `item.random`: use `options.shuffled`.
- `AudioLibrary.GetArtists` filters `genreid`, `genre`: use `songgenreid`, `songgenre`.

### Added

Methods:

- `Application.SetLogLevel`
- `AudioLibrary.RefreshAlbum`, `AudioLibrary.RefreshArtist`, `AudioLibrary.SetInfoProvider`
- `Application.GetDatabaseName`
- `GUI.GetInfoLabels`, `GUI.GetInfoBooleans`
- `GUI.TakeScreenshot`, `GUI.DeleteScreenshots` (off unless `allowscreenshotdeletion` is set)
- `GUI.SetScreenAlignment`, `GUI.GetScreenAlignment`
- `Player.GetChapters`
- `Player.SetGeometry`, `Player.GetGeometry`, and `geometry` on `Player.Open`
- `Player.SetDeclaredAspectRatio`, `Player.GetDeclaredAspectRatio`
- `Player.NotifyAudioChainReady`
- `Player.SetProperties`, `Player.VolumeUp`, `Player.VolumeDown`
- `Playlist.SetShuffle`, `Playlist.SetRepeat`
- `PVR.GetBroadcastsByChannelGroup`, `PVR.GetPlayableBroadcasts`
- `Settings.GetLevel`, `Settings.SetLevel`
- `VideoLibrary.Refresh`, `VideoLibrary.RefreshContentGeometry`, `VideoLibrary.SetSourceContent`
- `confirmed` on `Settings.SetSettingValue`
- `starttime` and `endtime` on `PVR.GetBroadcasts`

Notifications:

- `GUI.OnSkinLoaded`, `GUI.OnSkinLoadFailed`, `GUI.OnSkinUnloading`
- `Player.OnPlaybackFailed`
- `Player.OnContentGeometryChange`
- `Playlist.OnPropertiesChanged`
- `Settings.OnLevelChanged`

Failure reasons:

- A failure a client can act on carries `error.data` as `{"reason": ..., "target": {...}}`: a
  stable kebab-case reason, and what the failure concerns as the call addresses it
  (`{"playlist": "audio"}`), when there is one.
- Each method declares its `reasons` under the errors they come with, beside its `errors`:
  `{"FailedToExecute": ["nothing-playing", "not-seekable"]}`. A reason may come with more than
  one error. `JSONRPC.Introspect` serves them, with the reason descriptions under `reasons`;
  `openrpc.json` carries them as `x-kodi-reasons`.
- `Player`: `nothing-playing`, `not-applicable`, `not-seekable`, `not-pausable`,
  `tempo-unsupported`, `paused`, `no-such-stream`, and `unreachable` from `Player.Open`.
- `VideoLibrary`, `AudioLibrary` and `PVR`: `no-such-item` for an id nothing has, with the id as
  its target (`{"movieId": 3}`); `no-such-source` from `VideoLibrary.Scan`, `not-in-library` from
  `VideoLibrary.Clean`, and `no-such-addon` for a scraper that does not exist.
- `Files`: `outside-sources`, `no-such-path`, and `unreachable` for a directory that cannot be
  listed, each naming the path it was given.
- `Playlist`: `not-applicable` for an edit the `picture` playlist cannot take, `nothing-playing`
  for shuffling a slideshow that is not running, and, when `Add` or `Insert` adds nothing, the
  reason the first missing item gives. `Player.Open` gives the same for an item it cannot resolve:
  `no-such-item`, `no-such-path` or `not-a-file`.
- `Settings`: `no-such-setting`, `setting-disabled`, `change-declined` and `level-locked`, naming
  the setting or level.
- `PVR`: `pvr-not-started`, `not-recordable`, and `backend-refused` when the PVR add-on refuses a
  recording, timer or channel scan. `Player.Open`: `playback-refused` when PVR playback does not
  start, and `pvr-not-started`.
- `GUI.TakeScreenshot`: `nothing-playing` or `not-applicable` for the video frame with no video,
  `no-screenshot-folder`, `capture-failed`. `GUI.DeleteScreenshots`: `disabled`, `no-such-path`,
  `delete-failed`.
- `System.Shutdown`, `Suspend`, `Hibernate`, `Reboot`: `not-supported`.
  `Application.GetDatabaseName`: `database-not-open`. `VideoLibrary.RefreshContentGeometry`:
  `disabled`, `measure-failed`. `Playlist.SetShuffle`: `not-applicable` for unshuffling a
  slideshow. `Files.PrepareDownload` and `Files.Download`: `no-such-path`.

Properties and types:

- The error taxonomy in `JSONRPC.Introspect` (`errors`, and `"error"` as a filter type), and each
  method's `errors`
- `AccessDenied` (-32096)
- `Application.GetProperties`: `loglevel`
- `GUI.GetProperties`: `ready`
- `Player.GetProperties`: `contentrect`; video streams: `contentrect`
- `Player.Audio.Stream`: `bitspersample`; `Player.Subtitle`: `codec`
- `Playlist.GetProperties`: `shuffled`, `repeat`
- `List.Item.Base`: `stationname`; `List.Fields.All`: `episodename`, `episodepart`
- `PVR.Details.Broadcast`: `season`, `episode`, `parentalratingcode`, `parentalratingicon`,
  `parentalratingsource`; `PVR.Details.Recording`: those and `parentalrating`
- `Video.Details.TVShow`: `status`, `trailer`
- `Textures.Details.Texture`: `lastlibrarycheck`
- `Files.Media`: `games`

### Changed

- `Settings.GetSections`, `GetCategories` and `GetSettings` answer the `level` they filtered at.
- `Configuration.Notifications` declares `Info`, `Sources` and `Settings`.
- `Player.OnPropertiesChanged` carries every member `Player.Property.Value` declares.
- PVR images are `/image/` URLs, not local paths.
- A PVR channel's `icon` and `thumbnail` are its own logo; programme art is under `broadcastnow`.
- PVR cast members carry `role` and `order`.
- `GetInfoBooleans` is declared to return booleans.
- `JSONRPC.Introspect` names Kodi, not XBMC.
- `Player.Open` plays a directory with no pictures as a playlist.
- `VideoLibrary.SetTVShowDetails` accepts `trailer`.

### Fixed

- A `Player` method called without `playlist` acts on everything playing. The validator had filled
  the omitted parameter with `video`, so a call naming nothing failed while only audio played.
- Announcements are not held up by a busy TCP server, and each connection's requests run on its
  own thread, so a modal dialog stalls no other client.
- A failing send gives up instead of spinning.
- Methods are registered before anything can call them.
- `Settings.SetSettingValue` reports an invalid value as `InvalidParams` and a declined change as
  `Unavailable`, not `false`.
- `JSONRPC.SetConfiguration` keeps the namespaces it is not given.
- `Playlist.Clear` resets the position.
- `Playlist.Add` keeps each album's tracks together.
- `Player.GetItem` reports AirPlay cover art and PVR radio stream metadata, and keeps the metadata
  of a played item the library does not hold, as `Files.GetFileDetails` does.
- `file` agrees with `filetype` for a movie with versions or extras.
- `Files.GetFileDetails` reports `file` and `filetype`.
- `Files.PrepareDownload` reports `https` when the client used it.
- `VideoLibrary.SetTVShowDetails` applies `playcount` and `lastplayed` to the episodes.
- `VideoLibrary.Clean` honours `directory`.
- A hidden subtitle keeps its selection when its stream closes.
- `broadcastnow` and `broadcastnext` carry `label` and exactly the `PVR.Fields.Broadcast` fields.
- Broadcast times from 2038-01-19 on are no longer in the past.
- The schema declares what the serializers send: stream `source`, `version`, `flags`,
  `stereomode`, `language`, `hdrdetail`; broadcast `imdbnumber` as a string; `genre` as an array;
  `textureid` required; `volume` listed once.
