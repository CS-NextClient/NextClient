# Master server HTTP protocol

The Internet tab of the server browser gets its server list from the master server configured in `platform/config/MasterServer.vdf`. When the configured address starts with `http`, the list is fetched over HTTP by `HttpMasterClient` (`nextclient/engine_mini/src/service/matchmaking/master`); this document is the contract between that client and the backend.

## Request

The client sends a single request to the configured URL:

```
GET <url>
Content-type: application/json
BuildVersion: <client version, e.g. 2.5.3>

{"method": "server_list", "data": {"extended": true}}
```

`data.extended` asks for the extended layout below; a backend that does not know the flag answers with the legacy layout, which the client still accepts. Clients built before this document send `"data": "null"` and expect the legacy layout, so the backend must keep answering those with text.

## Response

The client reads the body of a response with status `200`; a body larger than 8 MiB after content decoding aborts the request. Two layouts are accepted. The first non-blank character of the body, after an optional UTF-8 byte order mark, decides which one is parsed: `[` selects the extended JSON layout, anything else the legacy text layout. A body that fits neither (malformed JSON, JSON nested deeper than 16 levels, a text line that is not an address) makes the request fail, see [Failed request](#failed-request).

### Extended layout

```json
[
  {"address": "1.2.3.4:27015", "country": "RU", "game_mode": "Public"},
  {"address": "5.6.7.8:27016"}
]
```

- `address` (required, string): IPv4 address and port in decimal, `a.b.c.d:port`, with a non-zero address and port. The port is the port the server is queried on; it is also used as the connection port. An entry without such an `address` is skipped. Unknown members are ignored, but an object that repeats a member name makes the whole body malformed JSON.
- `country` (optional, string): ISO 3166-1 alpha-2 code of the server's country; any letter case, shown upper-cased. When absent, the client resolves the country from the address with its local GeoIP database (below). A value that is not two ASCII letters is treated as absent.
- `game_mode` (optional, string): game mode identifier from the table below, 1 to 31 ASCII letters, digits and underscores. When absent, the client guesses the mode from the server's own answer, see [Game mode detection](#game-mode-detection); a value of any other form is treated as absent. An identifier the client build does not know is shown as sent, so new modes can be published before every client is updated.

An empty array means "no servers": the browser shows an empty list and does not fall back to its cached list.

### Legacy text layout

One `ip:port` per line, `\n` or `\r\n` separated; blank lines and blanks around an address are ignored:

```
1.2.3.4:27015
5.6.7.8:27016
```

A line with a zero address or port, such as `0.0.0.0:0`, lists no server. Entries carry no mode and no country: the mode is guessed as [Game mode detection](#game-mode-detection) describes, the country comes from the GeoIP database. A body that lists no server, including an empty one, has the same meaning as an empty array.

## Failed request

A request fails when it does not complete (a connection error, a timeout, a decoded body over 8 MiB), when the status is not `200`, or when the body fits neither layout. The browser then shows the cached list (below) if `CacheServers` is enabled and the cache holds a list, and keeps taking the list from the cache instead of the master server for the rest of the session; otherwise the list stays empty.

## Game mode identifiers

Identifiers are matched case-insensitively. The display names are the `ServerBrowser_GameMode_*` localization tokens in `assets/cstrike/resource/nextclient_english.txt`, which every other language falls back to; the client table is `kServerGameModeNames` in `nextclient/gameui/src/ServerBrowser/ServerGameModeNames.h`. New modes get an identifier on the backend and a row in that table plus a token on the client.

The mode filter lists the modes in the alphabetical order of their display names, one item per identifier.

| Identifier    | Shown as     |
|---------------|--------------|
| `Public`      | Public       |
| `Deathmatch`  | CSDM         |
| `GunGame`     | GunGame      |
| `Zombie`      | Zombie Mod   |
| `Deathrun`    | Deathrun     |
| `Surf`        | Surf         |
| `JailBreak`   | JailBreak    |
| `Knife`       | Knife        |
| `Awp`         | AWP          |
| `HideAndSeek` | Hide'n'Seek  |
| `Kreedz`      | KZ/Bhop      |
| `Aim`         | Aim          |
| `Warcraft`    | War3FT       |
| `BaseBuilder` | Base Builder |
| `Mix`         | CW/MIX       |
| `Special`     | Special      |

Each identifier covers a family of mods, so that the filter stays short: `Zombie` every zombie mod (Zombie Plague, Biohazard, Zombie Escape, CSO), `Deathmatch` every CSDM variant (FFA, Guns+Lasers, Only HS), `Kreedz` the movement mods (KZ, bhop, jump), `Mix` the competitive ones (CW, PUG, retake), and `Special` the niche mods that no item of their own would pay for: role play, RPG, CTF, CS:GO mod, SuperHero, COD, paintball, SoccerJam, TTT, 1v1 arenas and servers that run several mods at once.

## Game mode detection

When the master server gave no mode for a server (its entry has no `game_mode`, the list came in the legacy layout, or the server is absent from the master server's list, as in favorites and history), the client guesses the mode from the server's own query answer with the rules in `platform/servers/game_mode_rules.json`. A mode the master server sent is never replaced. The file is read once per game session, the first time a mode is needed, and only from the `platform` directory, so a file of the same name under the game or downloads directory does not replace it. It is UTF-8 JSON, with or without a byte order mark.

```json
{
  "default_game_mode": "Public",
  "rules": [
    {"game_mode": "Zombie", "keywords": ["zombie plague", "zp"]},
    {"game_mode": "Zombie", "priority": -1, "keywords": ["zombie*"]},
    {"game_mode": "Zombie", "priority": -1, "maps": ["zm"]},
    {"game_mode": "Awp", "priority": -1, "keywords": ["awp*"], "maps": ["awp*"]},
    {"game_mode": "", "keywords": ["training"]}
  ]
}
```

`default_game_mode` (optional, string) is the identifier of the servers the rules do not name, `Public` in the shipped file; a value of another form, or none, leaves those servers without a mode. Every member of a rule:

- `game_mode` (required, string): the identifier the rule gives, in the form of the response field, normally one from the table above. An empty string makes a rule that leaves the mode of the servers it matches unknown.
- `priority` (optional, integer): 0 when absent; how priorities decide is described below.
- `keywords` (optional, array of strings): matched against the game description and the server name.
- `maps` (optional, array of strings): matched against the map name.

A rule matches a server when one of its keywords occurs in the game description or the server name and one of its map keywords occurs in the map name; a rule with only one of the two lists needs only that one to match. A rule with an identifier of another form, a priority that is not an integer, or neither list is skipped, and a warning is logged.

The game description, the server name and the map name are split into words: runs of digits and letters (Latin, Greek, Cyrillic), with a dot between two such characters kept inside the word, so `joingame.kz` stays one word. ASCII letters, the Latin-1 Supplement letters and the Cyrillic letters from U+0400 to U+045F (the Russian, Ukrainian and Belarusian alphabets except `Ґ`) compare in any letter case. A keyword matches whole words, and a keyword of several words matches them in sequence (`death run` matches `Death-Run`). A `*` at the start of a keyword lets its first word end a longer word, and a `*` at its end lets its last word start one (`zombie*` matches `ZombiePlague`).

Only the matching rules of the highest priority decide. When all of them give the same identifier, the server gets that mode; when they give different identifiers, the server takes `default_game_mode`, since none of those rules is worth trusting over the others. A rule that decides alone with an empty identifier leaves its servers without a mode.

The shipped rules give specific mods the default priority, the general families (`Zombie`, `Deathmatch`) and the modes told by map type (`Awp`, `Aim`, `Knife`) priority -1, and a server that names itself a multi-mod priority 1 with the identifier `Special`. Servers no rule names are `Public` through `default_game_mode`, so the file carries no rule for public servers of its own. A map prefix that several modes use, such as `de_`, `awp_` or `aim_`, gives no mode on its own. A missing file, a file larger than 1 MiB, or one that is not a JSON object with a `rules` array, leaves unknown the modes the master server does not send, and a warning is logged.

The keywords were checked against 2952 Counter-Strike servers listed by a public master server in September 2026, while every mod family still had an identifier of its own: for the 471 of them whose mod plugins publish their cvars, the rules agreed with the plugins on 369 (on 282 exactly, on the rest at the level of the Zombie or CSDM family), left 65 unknown and disagreed on 37, mostly servers that run plugins of two modes. 42% of all servers got no mode. Merging the families into the identifiers above removes a part of those disagreements, since rules of one family no longer name different modes, and `default_game_mode` names the rest.

## Server country

When the response carries no `country`, the client resolves it from the server address with a local MaxMind DB file, `platform/servers/geoip_country.mmdb`, read through libmaxminddb; the same record supplies the country name for the flag tooltip and the country filter, in the game language (`ISteamApps::GetCurrentGameLanguage`, which falls back to the Steam UI language) when the database carries it (en, de, es, fr, ja, pt-BR, ru, zh-CN, fa, ko for DB-IP). Portuguese gets the Brazilian Portuguese names, Traditional Chinese the Simplified Chinese ones, and any other language the English ones. A country the master reported keeps its code even when the address resolves elsewhere; its name is then taken from an earlier lookup of the same code, and until one is known the code stands in for the name. The repository ships the DB-IP "IP to Country Lite" database (https://db-ip.com/db/download/ip-to-country-lite, CC BY 4.0, attribution "IP Geolocation by DB-IP" in `platform/servers/geoip_country_notice.txt`, which ships next to the database), stored in Git LFS (`.gitattributes`). It is refreshed by replacing the file with a newer monthly build; any other MaxMind DB country database, such as GeoLite2 Country, works the same way. When the file is missing, only master-reported countries are shown, by their codes, and a warning is logged once.

The country filter also accepts a country's names in its own languages, listed in `platform/servers/country_native_names.txt`: text that begins one of them lets the country's servers through, as the name in the UI language does, and the whole name picks the country, so `Deutschland`, `Polska` or `Україна` work whatever the UI language is. Each line of that UTF-8 file holds an ISO 3166-1 alpha-2 code, a blank and the names separated by semicolons; lines starting with `//` are comments. The server browser reads the file once, when it is created, and only from the `platform` directory; without the file the filter knows only the names in the UI language.

Flags are `platform/servers/flags/<code>.tga` (lower-case ISO 3166-1 alpha-2 code, 16x16 32-bit TGA), converted from the public-domain famfamfam flag icons (16x11 PNG) by `tools/flags/convert_flags.py`; the column header icon comes from `tools/flags/make_country_column_icon.py`.

## Cached list

With `CacheServers` enabled in `MasterServer.vdf`, the last list the master server returned is stored in `%APPDATA%\CS-NextClient\internet_cache.dat`: four `0xFF` bytes, a layout version byte (`2`), then records of IPv4 address (host byte order), port (host byte order), and the game mode identifier and country code each as a length byte followed by the bytes. A mode or country read back from the file is kept only in the form the extended layout allows. Files written by older builds (6-byte address records without a header) are still read, without modes or countries.
