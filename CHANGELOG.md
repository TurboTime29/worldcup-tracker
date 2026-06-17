# Changelog

All notable changes to this project are documented here. Versions are the tags
that trigger a release; each adds a flashable build to the
[web installer](https://turbotime29.github.io/worldcup-tracker/).

## [1.0.5] — 2026-06-17

- **Country names** now show in a small font under each team's 3-letter code
  (wrapping onto two lines when long), and the score sits lower on the screen.
- **Fixed past games disappearing while a match was live.** Live updates now make
  a single API call for the day's games and merge them into the schedule by match
  id, instead of re-fetching the whole tournament every couple of minutes — which
  could drop games whenever one of those requests got rate-limited. The full
  tournament rebuild still runs at boot and every ~6 hours.

## [1.0.4] — 2026-06-17

- Show a match as **LIVE the moment its kickoff time passes** (the free
  football-data feed flips to in-play a few minutes late), displaying 0–0 until
  the real score arrives.
- **Confirm the start time before going live**: polling now begins ~5 minutes
  before each kickoff, so a delayed or rescheduled match keeps counting down to
  its updated time instead of going live early.
- Fixed a **frozen countdown** that could stick on "IN 5M" through kickoff.
- A match still only ends when the API reports it finished (no timer ever shows
  FULL TIME).

## [1.0.3] — 2026-06-17

- The view now **auto-follows the action**: it advances to a live match, jumps
  to the next match ~15 minutes before kickoff, and otherwise lingers on the
  most recent result. Manual browsing isn't interrupted — a tap re-centers.

## [1.0.2] — 2026-06-16

- Reworked the **page dots into a centered carousel**: the focused match is
  always the center dot, earlier matches fan left and later matches fan right.
  Fixes the old behavior where the highlight stuck to the far right with many
  matches.
- Added CI automation: pushing a version tag builds the firmware once and
  publishes both the release and the web installer from the same binary.

## [1.0.1] — 2026-06-16

- The full schedule now **refreshes about every 6 hours (~4×/day)**, so
  newly-scheduled fixtures (e.g. knockout matchups once teams are known) appear
  automatically.

## [1.0.0] — 2026-06-16

Initial release — a glanceable FIFA World Cup 2026 tracker for the Waveshare
ESP32-S3-Touch-LCD-1.28.

- Whole tournament, **swipeable**, with live / finished / upcoming states,
  flags, scores, and kickoff times in your local timezone.
- On-boot **auto-jump** to the live or next match.
- **12- or 24-hour clock**, chosen during setup.
- **On-device brightness control** — a single BOOT-button click opens a
  brightness screen.
- First-boot **Wi-Fi + API-token setup portal**; credentials persist on the
  device and are never part of the firmware.
- **Browser web installer** for one-click flashing.

[1.0.5]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.5
[1.0.4]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.4
[1.0.3]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.3
[1.0.2]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.2
[1.0.1]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.1
[1.0.0]: https://github.com/TurboTime29/worldcup-tracker/releases/tag/v1.0.0
