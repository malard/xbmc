# JSON-RPC API changelog

The version reported by `JSONRPC.Version` and carried in `openrpc.json` and
`asyncapi.json`. It moves independently of Kodi's own version.

This changelog starts at version 14. Earlier versions were not tracked here;
for those, the commit history of `xbmc/interfaces/json-rpc/` is the record.

## 14.0.0

**A breaking release.** A client written against version 13 is not guaranteed
to work unchanged. [MIGRATING-v13-to-v14.md](MIGRATING-v13-to-v14.md) covers
every break below, with what to do about each.

Everything here is new since **13.5.0, the version Kodi 21 (Omega) shipped**,
which is the last version delivered in a stable release. Intermediate
versions went out in the Kodi 22 pre-releases — 13.8.0 in 22.0a1 and 13.11.0
in 22.0b1 — so a client tested only against a beta will already have some of
the additions below and none of the breaks.

### Breaking

- `JSONRPC.Introspect` answers in JSON Schema 2020-12 instead of draft-03.
  `extends` becomes `allOf`, a `$ref` is a JSON pointer, `enums` becomes
  `enum`, requiredness moves from a boolean on each property to an array on
  the containing object, and a parameter is a content descriptor wrapping its
  schema. Tuple-form `items`, `additionalItems`, `divisibleBy` and the boolean
  forms of `exclusiveMinimum`/`exclusiveMaximum` are gone.
- A PVR channel reports its client-side identifier as `channeluid`, not
  `uniqueid`. A broadcast and a recording already used that name. `uniqueid`
  on a library item is unrelated and unchanged.
- `Playlist.Add` and `Playlist.Insert` return an object naming what they could
  not add, in place of the `"OK"` string.
- `Player.GetProperties` reports `currentaudiostream`, `currentvideostream`
  and `currentsubtitle` as `null` when nothing is selected, in place of an
  empty object.
- `Files.GetDirectory` browses the directory it was given. A folder that
  matches a library item keeps its own path and comes back as
  `"filetype": "directory"`, where version 13 replaced it with the item and
  returned `"filetype": "file"` naming the video inside it. Entries for files
  are unchanged.
- `Files.GetDirectory` honours the requested `properties` under
  `"media": "files"`, which previously returned none of them. A request naming
  no properties still gets a plain listing and consults no library.
- `Files.GetDirectory` fills in tv show details for a show's folder. Version 13
  resolved movies, episodes and music videos but never shows, so a show folder
  came back bare even under `"media": "video"`.
- Calls that answered `InvalidParams` (-32602) for something that was named
  correctly but could not be provided now answer `NotFound` (-32098),
  `Unavailable` (-32097), `AccessDenied` (-32096) or `InternalError` (-32603).
  This affects `Files.GetDirectory`, `Files.GetFileDetails`,
  `Files.SetFileDetails`, `Files.PrepareDownload`, `Files.Download`,
  `Player.Open`, `VideoLibrary.Scan`, `VideoLibrary.Clean` and
  `AudioLibrary.GetArtistDetails`.
- `Settings.GetSettingValue`, `Settings.SetSettingValue` and
  `Settings.ResetSettingValue` answer `NotFound` for a setting that does not
  exist, and the two writers answer `Unavailable` for one that exists but is
  disabled by its dependencies. All three used to answer `InvalidParams` for
  both, and for a setting that was merely hidden - the same answer a typo
  gets. A hidden setting is now read and written like any other: visibility
  is what the settings window shows, not whether the value is real, and
  `advancedsettings.xml` hides the debug toggle whenever it pins a
  `<loglevel>`, which left the log level unreachable on exactly the
  installations that had set one. `Settings.GetSettings` and the listings
  above it still filter on visibility, since they describe that window.
- Stream languages are BCP 47 language tags, not ISO 639-2/B. `language` on
  `Player.Audio.Stream`, `Player.Video.Stream` and `Player.Subtitle` reads
  `en` where version 13 sent `eng`, and carries a region when the player knows
  one, `en-AU`. The same field inside a library item's `streamdetails` changes
  the same way but never carries a region: it is read from a column that holds
  ISO 639-2/B and is widened on the way out, so it can only ever name the
  language. An empty value still means the stream declared none, and a value
  Kodi does not recognise is passed through unchanged rather than blanked.

### Deprecated

**Everything deprecated in 14 is removed in 15.** Nothing on this list will
outlive this major version, so treat it as work to do before the next one
rather than something to revisit later.

Anything deprecated carries `"deprecated": true` — the annotation JSON Schema
2020-12 defines and OpenRPC uses — on its method or its schema. It is reported
by `JSONRPC.Introspect` even when a client asks for no descriptions, and
appears in `openrpc.json`. The description says what to use instead. The
schema never says *when* something goes; that is here.

- `XBMC.GetInfoLabels` and `XBMC.GetInfoBooleans`, superseded by
  `GUI.GetInfoLabels` and `GUI.GetInfoBooleans`. The old names still work and
  are served by the same implementation.
- `seasonnum` and `episodenum` on `PVR.Details.Broadcast`, superseded by
  `season` and `episode`. These were deprecated in 13.6.0 in prose only, so
  they never reached a stable release marked as such; 14 is the first version
  in which a client can discover it.
- `VideoLibrary.RefreshMovie`, `VideoLibrary.RefreshTVShow`,
  `VideoLibrary.RefreshEpisode` and `VideoLibrary.RefreshMusicVideo`,
  superseded by `VideoLibrary.Refresh`. The old names still work and are served
  by the same implementation, but the id moves inside an `item` object, so this
  one is not only a rename at the call site.

### Added

**Methods**

- `Settings.SetSettingValue` takes `confirmed`, which answers in advance the
  prompt a display mode change raises asking whether to keep the new mode.
  Without it the write blocks on the prompt for up to fifteen seconds and the
  mode reverts when nobody at the screen answers, which an automation cannot.
- `Application.SetLogLevel` - changes the log level, and which components
  have extra logging, and answers with what is now in force. Either part can
  be left out to keep its value. The level was previously reachable only as
  `debug.showloginfo`, a boolean over a four-value scale, and could not be
  read back at all: `--debug`, the interface toggle and the
  `advancedsettings.xml` hint all feed it and nothing reported the result.
- `AudioLibrary.RefreshAlbum` and `AudioLibrary.RefreshArtist` - refresh the
  additional information for an album or an artist.
- `Database.GetDatabaseName` - the database name in use for a given type,
  with the new `Database.Type`.
- `GUI.GetInfoLabels` and `GUI.GetInfoBooleans` - the unbranded names for the
  two deprecated `XBMC.*` methods.
- `GUI.TakeScreenshot` - takes a screenshot into the configured folder and
  answers with the `special://screenshots` path of every file it wrote. It
  waits for the capture to be encoded, so a file it names is complete when the
  response arrives, and `Files.PrepareDownload` serves it: the screenshot
  folder is reachable through the web server's `/vfs/` endpoint without being
  a media source. `content` chooses what is captured: `composite` for the full
  display output, `video` for the video frame alone, or `both`, which writes
  two files from the same rendered frame. An optional `target` names the file
  instead of taking the next `screenshotNNNNN.png`. With no folder configured
  it answers `Unavailable`, where it would otherwise raise a browse-for-folder
  dialog no caller is there to answer.
- `GUI.DeleteScreenshots` - deletes one screenshot or clears the folder, which
  is otherwise the one thing a client can fill and never reclaim: the
  auto-numbered name runs out at 65535 and nothing else in the API deletes a
  file. It is the only call that does, so it is **off unless**
  `<jsonrpc><allowscreenshotdeletion>true</allowscreenshotdeletion></jsonrpc>`
  is set in `advancedsettings.xml`, and answers `Unavailable` while it is off.
  `Files.GetDirectory` now lists `special://screenshots` so a client can see
  what is there before clearing it.
- `PVR.GetBroadcastsByChannelGroup` - the programme of every channel of a
  channel group within a time range, answered per channel, so an EPG grid is
  one call rather than one per channel and carries only the span it shows.
- `PVR.GetPlayableBroadcasts` - the playable broadcasts of a channel within a
  time range, for catchup availability.
- `PVR.GetProviders` and `PVR.GetProviderDetails`, with `PVR.Details.Provider`,
  `PVR.Fields.Provider` and `PVR.Provider.Type`.
- `Player.GetChapters` - the chapters of the playing item, with
  `Player.Chapter`.
- `Playlist.SetShuffle` and `Playlist.SetRepeat`.
- `Settings.GetLevel` and `Settings.SetLevel` - the setting level in force,
  which is what the settings window shows and what its level button cycles.
  It was previously reachable only from the interface, so a client driving
  Kodi could not reach an entry gated above the level it happened to be left
  at. `Settings.SetLevel` answers with the level that ended up in force,
  because the profile's settings lock can refuse the one asked for.
- `VideoLibrary.Refresh` - refreshes the library item its `item` parameter
  names, in place of the four deprecated per-type methods. A movie set and a
  season can be refreshed for the first time; the interface has always been
  able to and no per-type method covered either.
- `VideoLibrary.SetSourceContent` - assigns a content type and scraper to a
  video source path, as the "Set content" dialog does.

**Notifications**

- `GUI.OnSkinLoaded`, `GUI.OnSkinLoadFailed` and `GUI.OnSkinUnloading`.
- `Player.OnPlaybackFailed`, raised when playback was asked for and did not
  happen. Previously the request was acknowledged and nothing followed.
- `Playlist.OnPropertyChanged`, raised when a playlist's shuffle or repeat
  state changes.
- `Settings.OnLevelChanged`, raised when the setting level in force changes.
  The profile's settings lock lowers it without the viewer asking, which
  nothing could previously observe. `Settings` is a new notification
  namespace, and a client already receives it unless it has narrowed its
  subscription with `JSONRPC.SetConfiguration`.

**Properties and types**

- An error taxonomy in `JSONRPC.Introspect`, under `errors`: every status a
  call can fail with, its code, its message, and whether it carries `data`.
  `JSONRPC.Introspect` accepts `"error"` as a filter type.
- `AccessDenied` (-32096), for a path outside a source shared for remote
  access.
- `loglevel` on `Application.GetProperties`, with `Application.LogLevel`,
  `Application.LogLevel.Value` and `Application.LogComponent`: the level in
  force and every log component this build knows, by its own name, with
  whether each is enabled.
- `shuffled` and `repeat` on `Playlist.GetProperties`.
- `Playlist.AddResult` and `Playlist.UnresolvedItem`.
- `stationname` on `List.Item.Base`, the radio station serving an internet
  stream. `episodename` and `episodepart` are also newly requestable in
  `List.Fields.All`.
- `codec` on `Player.Subtitle`; `bitspersample` on `Player.Audio.Stream`.
- `starttime` and `endtime` on `PVR.GetBroadcasts`, which bound the answer
  to the broadcasts overlapping that range. Without them it answers as it
  always did.
- `season` and `episode` on `PVR.Details.Broadcast`, replacing `seasonnum`
  and `episodenum`.
- `parentalratingcode`, `parentalratingicon` and `parentalratingsource` on
  `PVR.Details.Broadcast`; those three plus `parentalrating` on
  `PVR.Details.Recording`.
- `status` and `trailer` on `Video.Details.TVShow`.
- `lastlibrarycheck` on `Textures.Details.Texture`.
- `games` on `Files.Media`, so `Files.GetSources` reaches the game sources.
- `Notifications.Library.Audio.Type` and `Notifications.Library.Video.Type`,
  the media types carried by the library notifications, named as types rather
  than repeated inline.

### Changed

- `Settings.GetSections`, `Settings.GetCategories` and `Settings.GetSettings`
  answer with the `level` they filtered at. That level is the one the call
  asked for and still defaults to `standard`, so it does not follow the level
  in force - which is exactly what the answer now makes it possible to tell.
- `Configuration.Notifications` declares `Info`, `Sources` and `Settings`.
  `JSONRPC.GetConfiguration` has always reported the first two, and the type
  forbids members it does not declare.
- `Player.OnPropertyChanged` carries the members `Player.Property.Value`
  declares, rather than a subset.
- PVR image properties (`icon`, `thumbnail`, `parentalratingicon`, and a
  recording's `art`) are URLs the web server's `/image/` endpoint can serve,
  rather than paths only the local machine could read.
- A PVR channel keeps its own logo in `icon` and `thumbnail` when the
  programme airing on it has its own artwork. The programme's artwork remains
  available under `broadcastnow`.
- Every cast member of a PVR item carries a `role` and an `order`, which
  `Video.Cast` requires.
- `XBMC.GetInfoBooleans` and `GUI.GetInfoBooleans` describe their return as an
  object of booleans. The description said strings; the implementation has
  always answered booleans.
- `Application.Property.Name` lists `volume` once. It was listed twice.
- `Textures.Details.Texture` declares `textureid` required.
- The header of a `JSONRPC.Introspect` answer names Kodi. `id` is
  `https://kodi.tv/jsonrpc/ServiceDescription.json` and `description` is
  "JSON-RPC API of Kodi"; both said XBMC.
- `Player.Open` with `item.path` plays a directory that holds no pictures as
  a playlist of its video and audio files, in the fullscreen video window
  with its OSD. It used to start a slideshow of the directory regardless,
  which played each video as a slide inside the slideshow window. A directory
  with pictures in it is a slideshow as before; `random` applies only there.
- `VideoLibrary.SetTVShowDetails` accepts `trailer`, so the trailer a show
  already reported through `Video.Details.TVShow` can now be written back.

### Fixed

- Announcements are no longer blocked while the TCP server is busy, and each
  request runs on its connection's own thread, so a method that raises a modal
  dialog no longer stalls every other client.
- A failing send gives up instead of spinning.
- The JSON-RPC methods are registered before anything can call them.
- `Settings.SetSettingValue` answers `InvalidParams` for a value the setting
  does not offer, including one outside a list the setting fills at runtime,
  and `Unavailable` for a change a handler declined, a display mode not kept
  for one. Both were a `false` result, indistinguishable from each other.
- `Video.Streams` declares the `source` and `version` every stream carries,
  and the `flags` bitmask on audio and subtitle streams, all of which the
  serializer has emitted since the fields were added.
- `JSONRPC.SetConfiguration` keeps every namespace the caller does not name.
  It silently dropped `PVR`, `Info` and `Sources` on every call, with no way
  to ask for them back, and kept `Application` by the state of `Other`. The
  three are now declared in its parameter alongside the rest.
- `Playlist.Clear` resets the playlist position that indexed the cleared
  items.
- `Player.GetItem` reports AirPlay cover art, and live stream metadata for a
  playing PVR radio channel.
- `Player.GetItem` and `Files.GetFileDetails` keep the metadata of an item the
  library does not hold, an add-on's typically, once it has been played.
- `file` agrees with `filetype` for a movie with versions or extras.
- `Files.GetFileDetails` reports the `file` and `filetype` its result type
  requires.
- `VideoLibrary.SetTVShowDetails` applies `playcount` and `lastplayed` to the
  show's episodes.
- `VideoLibrary.Clean` honours its `directory` parameter.
- `Playlist.Add` keeps an album's tracks together when several albums are
  added at once.
- A hidden subtitle keeps its selection when its stream is closed.
- `Files.PrepareDownload` reports the scheme the client reached Kodi by, so
  `protocol` can now be `https`. It was always `http`, which was wrong both
  when the web server serves HTTPS itself and behind a reverse proxy that
  terminates TLS - a browser blocked the composed URL as mixed content.
- `PVR.Details.Broadcast` declares `imdbnumber` as a string, and it and
  `PVR.Details.Recording` declare `genre` as an array of strings, which is
  what each has always sent.
- The `broadcastnow` and `broadcastnext` sub-objects of a PVR channel carry
  the `label` their type requires, and answer with the fields
  `PVR.Fields.Broadcast` declares. They are built the way a broadcast asked
  for by name is, so `hastimer`, `hastimerrule`, `hasreminder`,
  `hasrecording`, `recording` and `recordingid` are readable inside them for
  the first time, and the undeclared `channeluid`, `filenameandpath`,
  `serieslink` and `titleextrainfo` no longer appear - a caller has never
  been able to request those four by name.
- A broadcast reports a `starttime` or `endtime` that falls on or after
  2038-01-19, in place of a date in the past.
- The video streams of a library item declare `stereomode`, `language` and
  `hdrdetail`, which the serializer has always sent. The type refuses members
  it does not declare, so a client validating a response against it rejected
  one that was correct.
