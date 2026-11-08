# SponsorBlock Chapter Skip Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task by task. Steps use checkboxes for tracking.

**Goal:** Skip configured chapter markers during playback, respect manual seeks, and build a portable player in GitHub Actions.

**Architecture:** A standard C++17 header owns chapter selection, marker parsing, exemptions, and attempt suppression. MainFrm supplies chapter metadata and playback state, distinguishing manual and automatic seeks. Existing settings and build infrastructure provide configuration and packaging.

**Tech Stack:** C++17, MFC/DirectShow, MSVC, PowerShell, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-08-sponsorblock-chapter-skip-design.md` (approved).

## Global Constraints

- Work on `autoskip-sponsor`; no staging, commits, worktrees, pushes, or local dependency installs.
- Add no code comments except package header comments.
- `SkipSponsorBlockChapters` defaults to false. `SponsorBlockSkipMarkers` defaults to `[SponsorBlock]: Sponsor`.
- Markers are semicolon-separated, trimmed, literal, case-sensitive title prefixes. Empty entries are ignored.
- Manual seeks exempt only the selected chapter for that visit, including seeks while paused.
- Skip only running file playback, outside A–B repeat and frame stepping. Preserve normal completion handling.
- Use standard `windows-2022`, x64 Release, read-only permissions, short artifact retention.

## Review Focus

- Reentrant DirectShow callbacks must not override a pending or active manual seek.
- Duplicate or unsorted chapter starts must not produce empty skips or seek loops.
- Adjacent marked chapters must not inherit the previous chapter's manual exemption.
- Marker changes must invalidate attempted skips without discarding a current manual exemption.
- Final marked chapters must reach normal completion and advance playlists once.

## Task 1: Chapter skip policy and standalone tests

**Files:** Create `src/mpc-hc/ChapterSkip.h`, `src/mpc-hc/tests/ChapterSkipTests.cpp`, and `build/test-chapter-skip.cmd`.

**Interfaces:** `ChapterSkip::Time = int64_t`; `Chapter { Time start; std::wstring title; }`; `SetChapters(std::vector<Chapter>)`; `SetMarkers(std::wstring_view)`; `ResetVisit()`; `OnManualSeek(Time)`; `GetSkipTarget(Time position, Time duration, bool allowSkip = true, std::optional<uint64_t> generation = std::nullopt) -> std::optional<Time>`; `GetGeneration() -> uint64_t`; `CanApplySkip(uint64_t) -> bool`; `CancelPendingSkip()`; `SeekScope(ChapterSkip&)`. `DefaultMarkers` contains the approved default text.

- [x] Write tests for exact defaults, prefix and case behavior, multiple and empty markers, adjacent and final intervals, malformed timestamps, manual exemptions, paused observations, retries, and resets.
- [x] Compile against a minimal interface and run tests; confirm missing behavior fails.
- [x] Implement sorted chapter lookup, marker matching, per-chapter exemption, and attempted-range suppression. Preserve exemptions on marker edits.
- [x] Run `build\test-chapter-skip.cmd` in an initialized x64 developer environment. Expected: all tests pass, no compiler warnings.

## Task 2: Player and settings integration

**Files:** Modify `MainFrm.h/.cpp`, `AppSettings.h/.cpp`, `SettingsDefines.h`, `PPageAdvanced.h/.cpp`, `resource.h`, `mpc-hc.rc`, `mpc-hc.vcxproj`, and `mpc-hc.vcxproj.filters` under `src/mpc-hc/`.

**Interfaces:** Add `bool bManual = true` and an optional chapter-skip generation to `DoSeekTo`. Automatic skips call it directly after checking for pending user seeks, avoiding delayed automatic jumps after Pause. Add `ChapterSkip m_chapterSkip` and use `ChapterSkip::SeekScope` to guard reentrant callbacks. Expose settings through existing Advanced settings factories and profile persistence.

- [x] Add the settings, default values, resources, and header project entries.
- [x] Refresh helper chapters at `UpdateSeekbarChapterBag`; clear them when closing media.
- [x] Evaluate skips in the stream-position poller using current position/duration and running/file/seek/repeat/frame-step guards.
- [x] Record manual intent before seek callbacks; queued user seeks remain manual and take priority over automatic skips. Mark A–B repeat as automatic. Cover subtitle-navigation seeks and reset visits on stop/replay.
- [x] Run helper tests and inspect every main-graph `SetPositions` call. Validate project XML and `git -c core.whitespace=cr-at-eol diff --check`.
- [x] Document the runtime checks requiring the full player: paused seek then Play, seekbar/chapter/API navigation, saved resume, repeat, and final-chapter playlist transition.

## Task 3: Hosted build and user instructions

**Files:** Create `.github/workflows/build-sponsorblock.yml` and `docs/SponsorBlock.md`.

**Interfaces:** Workflow invokes `build/test-chapter-skip.cmd` and `build.bat x64 Release 7z Silent`. Configure `MPCHC_LAV_TOOLCHAIN=MSVC`, NASM 2.16.03, and Python with polib on the runner.

- [x] Add manual dispatch and push on `autoskip-sponsor`, recursive checkout, a bounded job timeout, and concurrency cancellation.
- [x] Configure the workflow to run tests before the player build and upload the portable archive and failure logs with three-day retention.
- [x] Document free public-runner usage/private allowances, first run, artifacts, settings, marker examples, and runtime checklist.
- [x] Validate workflow syntax and embedded scripts locally. Full player build remains a hosted validation requirement.
- [x] Obtain one fresh final code review, address substantive findings, rerun relevant checks, and leave changes uncommitted.


## Execution and verification

Implemented on `autoskip-sponsor` without staging or commits. Native Windows bookkeeping and test logs are in the ignored `bin/agent-sponsorblock` directory.

- MSVC v143 compiled and ran 62 policy checks with `/W4 /WX`.
- The same 62 checks passed with `/O2` and `windows.h` forcibly included, exercising compatibility with Windows macros.
- Workflow YAML and its PowerShell script parse successfully. Both project XML files parse. Resource UTF-16 encoding and existing source line endings are preserved.
- One fresh reviewer checked the working tree. Presentation-refresh and nested-manual-seek regressions were reproduced with failing tests, fixed, and passed. Frame-step exemption timing was moved to `EC_STEP_COMPLETE`; that integration was inspected, but requires the documented playback check.
- Full MPC-HC compilation, actual GitHub Actions execution, and playback tests were not run. Final-chapter completion and exactly-once playlist advancement still require validation with the built player.

## Implementation decisions

- A scoped guard freezes chapter observations during DirectShow callbacks. Generation checks also reject stale position queries and obsolete automatic targets after newer manual input, pauses, or metadata changes.
- Automatic skips use the direct seek path after giving queued user seeks priority. They cannot remain in the delayed-seek queue after Pause.
- Identical normalized chapter metadata preserves the current visit; changed metadata and file closure reset it.
- Final chapters use the existing completion event path. This avoids synthesizing a second playlist transition, but splitter-specific end-seek behavior remains a runtime validation requirement.
