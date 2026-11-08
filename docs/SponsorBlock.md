# Automatic chapter skipping

This build can skip chapters embedded by yt-dlp's `--sponsorblock-mark all`. It reads chapter titles already in the media file and does not contact SponsorBlock or modify the file.

## Enable and configure

Open **View > Options > Advanced**, under **Playback**:

- Set `SkipSponsorBlockChapters` to `true`. It is disabled by default.
- Leave `SponsorBlockSkipMarkers` at `[SponsorBlock]: Sponsor` to skip sponsor chapters.
- To skip other markers, enter a semicolon-separated list, for example:

  ```text
  [SponsorBlock]: Sponsor;[SponsorBlock]: Self Promotion;Intro;Credits
  ```

Each entry matches the beginning of a chapter title, literally and case-sensitively. Surrounding whitespace and empty entries are ignored. An empty list skips nothing. A broad entry such as `[SponsorBlock]` skips all SponsorBlock categories. You can replace the default marker as well as add markers.

Changes apply after clicking **Apply** or **OK**, and persist in the player's normal settings.

## Playback behavior

Matching chapters skip to the next unmarked chapter. Consecutive matching chapters are skipped together. A matching final chapter seeks to the media end and uses the player's existing completion behavior.

**Manually seeking inside a chapter lets that chapter play for the current visit.** This includes seeking while paused, followed by Play; the seekbar, keyboard, chapter navigation, subtitle navigation, and external seek commands use the same rule. The exemption ends when playback leaves the selected chapter. An adjacent matching chapter can still skip. Restoring a saved playback position does not create a manual exemption.

Skipping runs only during normal file playback. It is suspended while paused, frame stepping, seeking, or using A–B repeat. The normal position timer checks for skips (100 ms by default), so a small part of a segment may play before the jump. Incorrect or missing chapter metadata cannot be repaired by this feature. A failed automatic seek is not repeatedly retried within the same marked range.

## Build through GitHub Actions

The workflow is [`.github/workflows/build-sponsorblock.yml`](../.github/workflows/build-sponsorblock.yml). All full-player dependencies are prepared on the hosted runner; local Visual Studio, NASM, and Python are unnecessary for this route.

1. Enable Actions in your GitHub fork if GitHub asks you to enable workflows.
2. Commit and push these changes to `autoskip-sponsor`. A push to that branch starts the build, including the first run when the workflow is not yet on the default branch.
3. Open **Actions > Build MPC-HC with chapter skipping**, then open the run.
4. After success, download the `MPC-HC-SponsorBlock-x64-<run number>` artifact. Extract the downloaded artifact and then its `.7z` package. Run `mpc-hc64.exe` from the extracted player folder.
5. Enable the setting as described above. Download artifacts within three days; they expire afterward.

Manual **Run workflow** becomes available when this workflow also exists on the repository's default branch. Select `autoskip-sponsor` when running it. You can also rerun an existing run from its Actions page.

The job uses the standard `windows-2022` runner, Visual Studio 2022, MSVC for LAV Filters, NASM 2.16.03, and Python/polib. It runs the focused tests before building an x64 Release portable package, has a two-hour timeout, and cancels an older run when another starts on the same branch. Failed builds upload available `bin/logs` files; earlier setup failures remain visible in the Actions log. No releases are published.

Standard hosted runner compute is free for public repositories. Private repositories use the owner's included allowance. Artifact storage has its own allowance (GitHub Free currently includes 500 MB); the workflow uploads only the portable package and retains it for three days. See [GitHub Actions billing](https://docs.github.com/en/billing/concepts/product-billing/github-actions) for current allowances and usage controls.

## Local tests

The policy tests need only an MSVC C++17 toolset and a Windows SDK. From an initialized **x64 Native Tools Command Prompt**, run:

```bat
build\test-chapter-skip.cmd
```

The tests compile with `/W4 /WX` and cover markers, boundaries, malformed chapter times, adjacent chapters, manual exemptions, paused observations, reentrant callbacks, failed-seek suppression, and resets. They do not require MFC or media codecs. See [Compilation.md](Compilation.md) if you also want to build the full player locally.

## Playback validation checklist

Run these checks against the built player with chapter-marked MKV and MP4 downloads:

- With skipping disabled, verify all chapters play normally.
- Enable skipping and play across a sponsor boundary; confirm adjacent sponsor chapters skip in one jump.
- Seek inside a sponsor chapter while playing; confirm it continues playing. Repeat while paused, then press Play.
- Repeat with chapter navigation, a keyboard seek, subtitle navigation, and an external seek command. Verify the following marked chapter is still eligible.
- Close inside a sponsor chapter and reopen with remembered position enabled; confirm playback skips automatically.
- Add another marker, remove all markers, and restart the player; verify behavior and persistence. Check that marker edits preserve a current manual exemption.
- Enable A–B repeat across a marked chapter; confirm no automatic jump. Check frame stepping through the boundary.
- Play a final marked chapter in a playlist; confirm the next file starts once. Check repeat-file and stop-after-playback modes.
- Stop and replay, then open another file; confirm exemptions and prior skip attempts do not leak into the new visit.
