# AREX source review — 2026-11-04

The clock uses the station endpoint behind the operator's passenger website,
not Seoul Open Data's authenticated API, when lineId is 1065.

- Page: https://www.airportrailroad.com/train/normal/live
- Terms: https://www.airportrailroad.com/policy/terms
- The page's JavaScript calls fnRealTimeStation from a 5000 ms interval.
  The raw HTML snapshot is a local research input.
- No published numerical quota or external API availability guarantee was found
  on these pages. This is not evidence of unlimited permitted use.
- Terms articles 13 and 15 allow changes, termination and suspension.
- Current firmware polls one selected station every 20 seconds after a successful
  response. It skips requests from 00:30 to 05:00 using the configured clock
  timezone. This is approximately 3510 requests per day at most in steady use,
  excluding restarts/settings changes. It consumes no Seoul API key quota.
- Transport/JSON failures wait 40, 80, 160, 320, then 600 seconds. The deadline is
  not shortened by the approaching-train optimization. Success restores 20 s.
- HTTP 403 or 429 waits 30 minutes; no alternate IP, credentials or endpoint is
  tried to evade a block. This is a local conservative cooldown, not a statement
  of the operator's actual quota. Retry-After is not parsed.
- Received train data expires after 75 seconds, so an outage does not leave an
  old train counted down indefinitely. Existing schema filtering and timeouts
  remain. No local travel-time fallback is substituted for operator data.
- HTTPS certificate validation remains enabled. The verified intermediate is
  stored as DER for firmware size; certificate rotation or endpoint/schema
  changes can still require maintenance.

The official website warns that arrival information can differ with traffic
conditions. Neither reduced polling nor backoff can guarantee accuracy or
prevent a provider-side service change. Seek the operator's permission and a
supported API agreement before distributing or scaling this implementation.

## Multi-line implementation update

- `scripts/metro_network.py` imports the latest available station attachment
  (2026-09-02, download sequence 57 at the time of import) from Seoul dataset OA-12764. This lists 19
  metropolitan lines; it is not a nationwide urban-rail API.
- UI/catalog assets now live separately in LittleFS, preserving firmware OTA
  space. After a UI/catalog change deploy `scripts/metro_assets.py` in addition
  to a firmware update. Existing assets survive ordinary firmware OTA.
- Non-AREX direction and two preceding stations are resolved from live response
  adjacent station IDs matched against the next-station direction text. Branch
  approach options appear when multiple incoming paths are available. Missing
  live metadata is reported instead of guessing from station-number order.
- Non-AREX arrivals use the saved Seoul key, the shared 900-call local budget,
  source-age correction, and source timestamps for expiry. Resolver requests
  also consume this budget. Responses may be cached/delayed upstream.
- Applying settings clears both train slots, receipt time, draw timer and
  operator backoff. UI revision guards reject stale asynchronous selection
  responses, and selectors are disabled while a save is in progress.
- AREX gauge timing no longer adds 59 seconds to the published minute. Same-
  minute updates retain the running interpolation deadline. All gauge paths
  reach the target when remaining ETA reaches zero; no fixed dwell/segment
  times are used. The gauge remains an estimate, not a GPS position feed.


To regenerate the derived catalog, download the vendor inputs separately and run:

```sh
python scripts/metro_network.py stations.xlsx arex-live.html --date YYYY-MM-DD
```

The Korean LCD labels were rasterized from [Noto Sans CJK KR](https://github.com/notofonts/noto-cjk/tree/main/Sans).
To regenerate the labels with an installed copy of the font, run
`python scripts/metro_arrival_glyphs.py --font NotoSansCJKkr-Regular.otf`.
