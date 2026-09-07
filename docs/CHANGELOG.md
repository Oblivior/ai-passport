<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## Unreleased

- Add non-regressing monthly archive reconciliation through a trusted local
  personal-data provider. Freeze the original goal, care days, branch and shared
  allocation denominator; never credit current food or invent care days. Persist
  high-water marks before acknowledging and show pending/completed status in the
  family album. Explicitly migrate v3/v4 CRC records to v5, retaining the old
  source slot and treating legacy archives as local-only. Verify retries, missing
  coverage, shared caps, eviction, failed saves, protocol bounds and rendered UI.

- Add opt-in, time-bounded nearby greetings using public ephemeral BLE scan
  responses and mutual on-device confirmation. Show both companions and pairing
  messages; cancel on page exit, timeout, identity change or computer connection.
  Enable observer mode without another connection slot or credentials. Keep
  unauthenticated greeting data entirely separate from food, bond and archives;
  cover simulated peers, replay, consent, cancellation and real-size UI rendering.

- Add an opt-in Agumon dark route at 30 bond, SkullGreymon and BlackWarGreymon
  sprites, explicit confirm/cancel, standard-route recovery without growth loss,
  independent discoveries and accurate monthly branch archives. Migrate validated
  v3 saves to v4 in the same CRC slots, changing only the version on upgrade;
  reject unknown schemas and preserve bond, pairing and existing growth.

- Add a three-round timing training game with per-species pixel shots, result
  feedback and optional daily bond rewards. Share three rewards across partners,
  retain lifetime bond by species, and unlock home greetings/celebrations without
  altering meals or evolution. Persist bond in isolated CRC slots, protect
  duplicate/stale results and save retries, and allow offline practice. Verify
  timing, daily caps, fault recovery, page navigation, layout and interrupted play.

- Remove obsolete robot family records from active saves through CRC-protected
  startup transactions, preserving current companions and real Digimon history.
  Keep legacy source saves unchanged and retry failed cleanup on reboot. Make
  family-page OK return home for zero or one records and cycle only for multiple
  records, with matching footer hints. Test mixed archives, migration, failed
  writes, CRC fallback, idempotency and real LVGL button navigation.

- Align companion-page titles and grass-footer actions, show separate food/day
  evolution bars, label catalog form positions and partner selection positions,
  and add empty-archive navigation and wireless-page return hints. Compact large
  Token counts with explicit approximate units while retaining exact sync data.
  Preserve controls and saves; test numeric boundaries, bar values, footer cleanup,
  glyph coverage and label/character/bar separation on real LVGL.

- Center the home-screen companion, pair its phase with a compact Lv.0–Lv.6
  badge, and group pantry and growth below it. Keep the action hint on the grass
  footer and detailed companion-day totals on the progress page. Replace the
  decorative tray with next-form requirement progress
  (the lesser of capped food/day target completion), remaining requirements and
  an explicit final-form state. Preserve growth rules, saves and animation timing.

- Add complete Agumon, Gabumon and Patamon lines, confirmed egg adoption and a
  partner house. Preserve individual growth while sharing the daily pantry;
  prevent historical food from granting pre-adoption days. Keep lifetime catalog
  discoveries and archive each adopted partner on rollover. Add a fixed-ID v3
  save with read-only v1/v2 migration, CRC slots, fail-closed downgrade handling
  and stale-button protection. Verify transactions, failure/reboot recovery,
  three-line pixel rendering and real selection/cancellation/page interactions.

- Present the seven-stage Agumon line with independent pixel fan art, eating and
  sleeping variants, and a read-only evolution catalog. Preserve v2 growth,
  protocol and legacy demo memories; retain usage-route data without treating it
  as a Digimon species selector. Document character-rights scope and verify the
  actual scaled pixels in LVGL, not only label bounds.

- Add a five-slot daily lunchbox with source-date Token snapshots, next-meal
  progress, a daily cap, older-food accounting and stale-sync labels. Announce
  newly earned food once without changing saves, protocols or rewards. Cover
  boundary arithmetic and real LVGL page rendering with host regressions.

- Localize the badge UI into Simplified Chinese, including pet forms, lunch,
  evolution, routes, family, connection status and demo menus. Embed a small
  OFL-licensed 14/20 px font subset and check glyph coverage and page bounds.
  Keep save data, wire identifiers and growth rules unchanged.

- Add an opt-in cached cumulative export source for both desktop transports;
  preserve full-export compatibility, privacy and idempotent meal allowances.

- Add three local food-intensity evolution routes, a read-only final-form preview
  page, route-specific bodies and archived route identity. Lock at RANGER without
  changing v2 save layout, growth limits or existing STATUS/SYNC. Add authenticated
  read-only ROUTE diagnostics; Bits-certified routes remain separate future work.

- Retry cold BLE discovery/connection timeouts up to three times with bounded
  backoff, and handle Python 3.9 asyncio timeouts without stopping watch mode.
  Identity and authentication failures remain fail-closed.

- Start the persistent radio after board bring-up and match badge identity plus
  service locally during macOS discovery to handle cold scan name availability.

- Add USB-provisioned AES-256-GCM wireless lunch delivery, replay protection,
  private desktop pairing files, a Pet Link status page and transport-aware sync
  feedback. Keep the pet save format and wired fallback unchanged.

- Initialize the ESP-IDF USB receive driver for food sync; keep the companion's
  serial connection open between watch cycles and retry startup handshakes.

- Replaced unlimited test feeding with a Kaboo-local USB lunchbox, food and
  active-day evolution gates, animated moods, next-stage requirements and
  browsable family records. Added cumulative sync, local calendar rollover and
  non-destructive legacy-save import. Bits and BLE integration remain pending.

- Fixed feeding animations reading unresolved LVGL coordinates and moving the
  pet over its title. Jumps now use a relative offset and cannot accumulate
  position drift when interrupted. Added real LVGL 9.5.0 regression coverage.
- Separated the pet stage title from the animated character into a fixed pixel
  plate and simplified the battery readout to prevent overlap on the 240x320
  display.
- Added an offline AI Pet MVP for the Passport: seven original growth stages,
  button-driven feeding and monthly settlement, a 12-month family archive,
  sleep-without-decay behavior, battery status, and CRC-protected dual-slot NVS
  persistence. Added host tests for thresholds, idempotency, settlement, month
  transitions, corrupted state, and counter saturation.
- Made mini-program BLE install compatibility a template-level invariant: fixed
  protected `cardid`/Recovery partitions, retained the five-second UP-key
  Recovery boot hook, and added CI validation for merged-image structure,
  partition MD5/ranges, the 3 MB app limit, and protected payload exclusion.
- Documented a release-title convention for multi-app releases: name tags as `v<version>-<app-name>` (e.g. `v0.1.0-voice-keychain`) so the release title carries the version and the app, and confirm the title after the release is published so a release list is scannable by app.
- Added a post-release follow-up workflow: an `issue-suggestions` skill for filing user feedback as issues against the upstream project, an `experience-pr` skill for submitting reusable development experience as a documentation PR, a `docs/experiences/` directory for per-entry experience files, and supporting `project-completion`, `file-issues`, and experience-index documents.
- Simplified the tracked repository root: moved GitHub-recognized community documents into `.github/`, moved the changelog into `docs/`, updated every reference, and added a root-document allowlist to repository checks.
- Repository-wide language policy: every maintained Markdown default `.md` file is English, Simplified Chinese uses a paired `.zh_CN.md`, and both provide language switches. Static checks reject missing peers, missing switches, and Chinese prose in English defaults.
- Phase one of the AI development workflow: streamlined task-based context routing, unified local/CI validation, added PR checks and a template, and committed the dependency lock for reproducible builds.
- PR review fixes: pinned GitHub Actions to full commit SHAs, split build/release jobs by least privilege, disabled persisted sync checkout credentials, added Feature Request and Usage Question forms, clarified private security-report fallback, and corrected stale README, CI-trigger, and branch descriptions.
- Changed commit titles, PR titles, and PR bodies from Chinese-default to English; updated the Chinese punctuation rule so it no longer applies to PR descriptions.
- Reworked `build-firmware.yml` to pass `SDKCONFIG_DEFAULTS=sdkconfig.defaults`, enable `partitions.csv`, preserve the 8 MB image header, merge a flashable `FoloToy-AI-Passport-full.bin`, publish only that artifact, and use Actions cache v5.
- Integrated upstream PR #6 to resolve PR #4 conflicts: Wi-Fi, Bluetooth LE, radio lifecycle, and low-power demos; a 3 MB factory partition; build/menu/configuration updates; hardware-guide coverage; and bilingual capability tables.
- Defined English imperative Conventional Commit formatting for both commits and PR titles.
- Removed stale sync-workflow template comments and generalized an irrelevant Redis TTL rule to cache components.
- Added Chinese punctuation, credential safety, and recoverable file-deletion conventions.
- Expanded source-comment requirements for functions, state, ownership, concurrency, timing, registers, and magic values.
- Removed AI execution instructions from product READMEs so they remain human-facing product and repository overviews.
- Added `docs/development/agent-guide.md` as the focused AI workflow guide.
- Updated `AGENTS.md`, `docs/INDEX.md`, and the development index for the agent guide.
- Documented why the root README path is reserved for fork owners and how GitHub README precedence supports it.
- Created `main-update` from the upstream-aligned baseline and combined the repository-structure, firmware-CI, and upstream-sync work.
- Corrected the merged documentation index, workflow path, project tree, and CI references.
- Moved CI documentation from software design to `docs/development/`.
- Moved fork-only documentation assets from `assets/docs/` to `docs/assets/`.
- Moved the upstream English/Chinese project READMEs under `docs/` and renamed the documentation catalog to `docs/INDEX.md`.
- Initialized `AGENTS.md`, `CLAUDE.md`, and `CHANGELOG.md`.
- Standardized the initial project README language filenames.
- Added the `docs/`, `assets/`, and `skills/` directory structure.
- Moved the upstream hardware guide into `docs/hardware-design/`.
- Standardized subdirectory README capitalization and introduced fork conventions.
- Allowed fork-owned root README and supplemental documentation content on fork `main`.
- Added and documented the fork-only supplemental-document directory.
- Moved the build CI document to its dedicated CI branch before consolidation.
- Documented clean-`main` reasons, the direct-development exception, and Actions enablement for forks.
- Split the original agent rules into contribution, development, and fork documents with a compact root index.
- Updated software-design and project README references for the new documentation structure.
- Added the documentation catalog and task-triggered routing based on the earlier repository model.
- Added bilingual contribution, code-of-conduct, security, and support documents tailored to this ESP-IDF and fork workflow.
