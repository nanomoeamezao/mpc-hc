# SponsorBlock chapter skipping and GitHub Actions build

## Intent and selected approach

Automatically skip chapters whose titles begin with a configured marker in videos already downloaded with yt-dlp. The default marker is `[SponsorBlock]: Sponsor`; users can configure additional markers. Explicitly seeking into a matching chapter must allow that chapter to play. Implement this inside MPC-HC on the existing `autoskip-sponsor` branch, and provide a GitHub Actions build that requires no local development dependencies.

The earlier exploration considered an external companion application and a native player change. The user selected the native change. A general chapter scripting system would add unnecessary scope; use a narrowly scoped chapter rule and the existing playback and settings infrastructure.

## Playback behavior

- Add `SkipSponsorBlockChapters` under Options > Advanced > Playback. Proposed default: disabled, with explicit instructions to enable it. Persist the setting through the existing INI/registry settings system.
- Add a separate `SponsorBlockSkipMarkers` text option in the same settings group. Its default value is `[SponsorBlock]: Sponsor`. Users can append, replace, or remove markers in a semicolon-separated list, for example `[SponsorBlock]: Sponsor;Intro;Credits`. Trim whitespace around entries, ignore empty entries, and match each entry as a literal, case-sensitive chapter-title prefix. An empty list skips nothing. Persist this option through the existing INI/registry settings system.
- Match any configured marker. Other SponsorBlock categories are not skipped by default unless their title also begins with the configured sponsor marker. No regular expressions or SponsorBlock network access are required.
- Treat a chapter as the interval from its start to the next chapter start, or media duration for the final chapter. Merge adjacent matching intervals before seeking. Ignore invalid or empty intervals.
- During normal file playback, seek to the end of a matching interval using MPC-HC's existing seek path. Handle playback starting or restoring a saved position inside an interval. Normal chapter navigation remains available.
- A manual seek into a matching chapter exempts that chapter from automatic skipping for the current visit. This includes timeline dragging, keyboard seeks, explicit chapter navigation, and externally requested seeks. Record manual intent before seek callbacks can trigger the playback timer. Automatic seeks and saved-position restoration do not create this exemption.
- Keep the exemption while playback remains in the selected chapter, including when the user seeks while paused and then presses Play. Clear it when playback leaves that chapter. A following matching chapter is eligible for automatic skipping, even if it is adjacent. A later manual seek into a matching chapter creates a new exemption.
- Paused playback and frame stepping do not trigger automatic seeking.
- Suspend automatic skipping while A–B repeat is active so it does not alter the user's loop.
- Guard against recursive timer callbacks, pending seeks, and repeated attempts after a failed seek. Reset interval and manual-exemption state when chapters or the current file change. Recalculate matching intervals when the marker list changes.
- A final marked interval seeks to the media end and preserves the existing end-of-playback and playlist behavior. This needs explicit playback validation.
- Reuse the existing playback timer. Skipping is not guaranteed to be sample-accurate; a short portion of a segment may play before the next check.

## Implementation boundaries

Keep marker parsing, interval calculation, manual-seek exemptions, and skip-attempt state in a small C++ helper that can be tested without the full player. Integrate it with `MainFrm`, `AppSettings`, the Advanced settings page, settings keys, and resource strings. Distinguish user-requested seeks from automatic seeks internally without changing external-control API formats. Add only project entries needed by the new helper. Do not change codecs, video files, or downloads.

## GitHub Actions

Add a manually runnable workflow and a push trigger for `autoskip-sponsor`, so the first build can run even before the workflow reaches the default branch. Use the standard `windows-2022` runner and x64 Release configuration.

Check out recursive submodules with shell script line endings preserved, use the runner's Visual Studio and Windows SDK, obtain a pinned NASM release, and configure `MPCHC_LAV_TOOLCHAIN=GCC`. Install MSYS2 shell tools and the upstream MinGW compiler archive only on the runner. Pin and verify the compiler archive's SHA-256. This follows the user's approved switch to the upstream GCC build route after stale MSVC-generated configuration caused CI failures. Restore the MSVC-specific repair files to their upstream baseline. Install translation tooling only on the runner. Check compiler and shell discovery, run the focused C++ tests, build using the existing scripts, and upload the portable release package with short retention. Give the workflow read-only repository permissions and a bounded timeout. Upload FFmpeg configure/build logs alongside the player build logs on failure.

Standard runner compute is free when the fork is public. Private repositories consume the owner's included allowance. Document artifact storage limits and how to download and enable the feature. Do not publish releases or use paid larger runners.

## Validation and constraints

Test the default sponsor marker, multiple markers, whitespace and empty entries, prefix matching, unmarked chapters, boundaries, adjacent markers, first/final markers, invalid timestamps, repeated timer callbacks, failed-seek suppression, and reset behavior. Test manual seeks while playing and paused, exemption lasting until the selected chapter ends, the next adjacent matching chapter being skipped, and automatic resume not creating an exemption. Review the integration for frame stepping, pending seeks, A–B repeat, file changes, marker edits, and end-of-playback handling. Provide a short manual playback checklist for MKV/MP4 downloads and playlist transitions.

No local dependency installation, staging, commits, worktrees, or remote pushes are authorized. Use the current branch. Add no code comments except package header comments. Clearly distinguish static checks from C++ tests and full builds that have actually run.

## Approval

The user approved this design, including the corrected default marker and manual-seek exemption, on 2026-11-08.
