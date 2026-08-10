# Migrating a client from JSON-RPC 13 to 14

Version 14 is a breaking release: a version 13 client is not guaranteed to
work against it unchanged. Everything that breaks is listed here, with what to
do about it.

The baseline is **13.5.0, the version Kodi 21 (Omega) shipped** — the last
version delivered in a stable release. If you tested against a Kodi 22
pre-release you were on 13.8.0 or 13.11.0, which already carry some of the
additions in the [changelog](CHANGELOG.md) but none of the breaks below.

Check what you are talking to before you assume either shape:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Version"}
```

```json
{"jsonrpc": "2.0", "id": 1, "result": {"version": {"major": 14, "minor": 0, "patch": 0}}}
```

Nothing below depends on Kodi's own version. A client that supports both
should branch on `version.major`.

---

## 1. Errors are more specific than `InvalidParams`

**Most likely to break you, and the easiest to miss**, because the calls
still work — only the failures changed.

Version 13 answered `-32602 InvalidParams` for almost everything that went
wrong after the parameters had been validated. A client could not tell "you
asked for a movie that is not in the library" from "you sent a malformed
request". Version 14 separates them:

| Code | Name | Means |
|---|---|---|
| -32602 | `InvalidParams` | the request itself is wrong |
| -32098 | `NotFound` | what you named does not exist |
| -32097 | `Unavailable` | it exists but cannot be provided now |
| -32096 | `AccessDenied` | the path is not in a source shared for remote access |
| -32603 | `InternalError` | the method failed for a reason none of the above describes |

Affected: `Files.GetDirectory`, `Files.GetFileDetails`,
`Files.SetFileDetails`, `Files.PrepareDownload`, `Files.Download`,
`Player.Open`, `VideoLibrary.Scan`, `VideoLibrary.Clean`,
`AudioLibrary.GetArtistDetails`.

**What to do.** If you test `error.code == -32602` to decide that something is
missing, you will now miss the case. Treat -32098, -32097 and -32096 as
failures too, and prefer them for the "gone" and "forbidden" messages you show
a user.

The full taxonomy is discoverable rather than hardcoded:

```json
{"jsonrpc": "2.0", "id": 1, "method": "JSONRPC.Introspect",
 "params": {"filter": {"type": "error", "id": "NotFound"}}}
```

---

## 2. `Playlist.Add` and `Playlist.Insert` return a result

Version 13 returned the string `"OK"` whether or not anything was added, and
dropped silently whatever it could not resolve.

```json
{"result": "OK"}
```

Version 14 says what happened:

```json
{"result": {"added": 2,
            "unresolved": [{"item": {"movieid": 4321}, "reason": "notfound"}]}}
```

`reason` is `notfound`, `unavailable` or `invalid`. When *nothing* was added
the call is an error instead — `NotFound` if anything named something real
that has gone, `InvalidParams` if every entry was malformed.

**What to do.** If you check `result == "OK"`, that test now fails against a
successful call. Read `result.added`, and show `result.unresolved` if you
report progress to a user.

---

## 3. A PVR channel's `uniqueid` is now `channeluid`

`PVR.Details.Channel.uniqueid` is gone. The same value is `channeluid`, which
is what a broadcast and a recording already called it.

```diff
- {"channelid": 12, "uniqueid": 8201}
+ {"channelid": 12, "channeluid": 8201}
```

`uniqueid` is no longer a member of `PVR.Fields.Channel`, so requesting it
returns `InvalidParams`.

**What to do.** Rename the field you request and the one you read. Note this
is unrelated to `uniqueid` on a library item, which is a set of scraper
identifiers and has not changed.

---

## 4. An unselected stream is `null`, not `{}`

`Player.GetProperties` reported `currentaudiostream`, `currentvideostream`
and `currentsubtitle` as an empty object when nothing was selected.

```diff
- {"currentsubtitle": {}}
+ {"currentsubtitle": null}
```

**What to do.** Test for `null` before reading `index`. A client that checks
"is this object non-empty" keeps working; one that reads `.index`
unconditionally will now fault on `null`.

`Player.Subtitle` also gains a `codec` member.

---

## 5. `XBMC.GetInfoLabels` and `XBMC.GetInfoBooleans` are deprecated

They still work and are the same implementation. They will be **removed in
Kodi 23, API version 15**.

```diff
- {"method": "XBMC.GetInfoLabels", "params": {"labels": ["System.Time"]}}
+ {"method": "GUI.GetInfoLabels",  "params": {"labels": ["System.Time"]}}
```

Parameters, result and required permission are identical, so this is a rename
at the call site and nothing more.

Note also that both methods describe their result correctly for the first
time: `GetInfoBooleans` returns an object of **booleans**. The schema said
strings; the wire has always carried booleans. If you generate code from the
schema, your generated type changes even though the traffic does not.

---

## 6. `JSONRPC.Introspect` answers in JSON Schema 2020-12

**Only affects you if you consume the service description itself.** A client
that just calls methods is unaffected by this section.

The description was JSON Schema draft-03. It is now 2020-12:

| draft-03 | 2020-12 |
|---|---|
| `"extends": "Name"` | `"allOf": [{"$ref": "#/$defs/Name"}]` |
| `"$ref": "Name"` | `"$ref": "#/$defs/Name"` |
| `"enums": [...]` | `"enum": [...]` |
| `"required": true` on a property | `"required": ["prop"]` on the object |
| `"type": [ {...}, {...} ]` | `"anyOf": [ {...}, {...} ]` |
| `"type": "any"` | the keyword is omitted |
| `{"name": "x", "type": "string"}` as a param | `{"name": "x", "schema": {"type": "string"}}` |

A method's parameters are now content descriptors: `name`, `required` and
`description` belong to the descriptor, and the schema of the value sits
under `schema`. Tuple-form `items`, `additionalItems`, `divisibleBy` and the
boolean `exclusiveMinimum`/`exclusiveMaximum` are no longer read; the shipped
schema never used them.

Two definitions that were inline and named by an `id` are now global types in
their own right: `Notifications.Library.Audio.Type` and
`Notifications.Library.Video.Type`.

**What to do.** If you validate against the description, use a 2020-12
validator. If you generate code from it, most generators support 2020-12
directly and needed a shim for draft-03. You can also skip `Introspect`
entirely and consume [openrpc.json](openrpc.json), which is generated from the
same schema and gated in CI so it cannot drift.

---

## 7. `seasonnum` and `episodenum` on a PVR broadcast are deprecated

Superseded by `season` and `episode`. The old names still work. Nothing has
been announced about when they go.

---

## Nothing to do, but worth knowing

New since Kodi 21 and safe to ignore until you want it. The
[changelog](CHANGELOG.md) has the complete list.

- **Playback failure is now reported.** `Player.OnPlaybackFailed` fires when
  playback was requested and did not happen, with a `reason` of `unplayable`,
  `unresolved`, `locked` or `error`. Previously a failed playback produced no
  notification at all.
- **Playlist shuffle and repeat** are readable (`Playlist.GetProperties`),
  settable (`Playlist.SetShuffle`, `Playlist.SetRepeat`) and observable
  (`Playlist.OnPropertyChanged`).
- **Skin lifecycle notifications**: `GUI.OnSkinLoaded`,
  `GUI.OnSkinLoadFailed` and `GUI.OnSkinUnloading`.
- **PVR providers** are listable via `PVR.GetProviders` and
  `PVR.GetProviderDetails`, and `PVR.GetPlayableBroadcasts` answers which
  broadcasts in a time range can be played back.
- **`VideoLibrary.SetSourceContent`** assigns a content type and scraper to a
  source path, which previously only the "Set content" dialog could do.
- **`Player.GetChapters`** returns the playing item's chapters.
- **`GUI.TakeScreenshot`**, `Database.GetDatabaseName`,
  `AudioLibrary.RefreshAlbum` and `AudioLibrary.RefreshArtist`.
- **PVR image properties are URLs** the web server's `/image/` endpoint can
  serve. Previously they were paths that only the machine running Kodi could
  read, so a remote client could not display them.
- **A PVR channel keeps its own logo** in `icon` and `thumbnail` even when the
  programme airing has its own artwork; the programme's artwork is under
  `broadcastnow`.
- **`Player.OnPropertyChanged`** carries every member its declared type
  promises, rather than a subset.
- **New properties**: `stationname`, `episodename` and `episodepart` on list
  items, the parental rating fields on PVR broadcasts and recordings,
  `bitspersample` on an audio stream, `status` and `trailer` on a TV show,
  and `lastlibrarycheck` on a texture.
