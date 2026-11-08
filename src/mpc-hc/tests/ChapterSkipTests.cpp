#include "../ChapterSkip.h"

#include <cstdlib>
#include <iostream>

static int checks = 0;

static void Expect(bool condition, const char* name)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << name << '\n';
        std::exit(1);
    }
}

static ChapterSkip Example()
{
    ChapterSkip skip;
    skip.SetChapters({{0, L"Content"}, {10, L"[SponsorBlock]: Sponsor"},
                      {20, L"[SponsorBlock]: Sponsor 2"}, {30, L"Content"},
                      {40, L"[SponsorBlock]: Outro"}});
    return skip;
}

int main()
{
    auto skip = Example();
    Expect(!skip.GetSkipTarget(0, 50), "unmarked opening");
    Expect(!skip.GetSkipTarget(9, 50), "before sponsor boundary");
    Expect(skip.GetSkipTarget(10, 50) == 30, "default marker merges adjacent chapters");
    Expect(!skip.GetSkipTarget(10, 50), "reentrant callback does not repeat seek");
    Expect(!skip.GetSkipTarget(21, 50), "failed seek does not retry in merged range");
    Expect(!skip.GetSkipTarget(30, 50), "skip end is exclusive");
    Expect(!skip.GetSkipTarget(45, 50), "other SponsorBlock categories stay enabled");
    Expect(skip.GetSkipTarget(15, 50) == 30, "new visit can skip again");

    skip = Example();
    skip.OnManualSeek(15);
    Expect(!skip.GetSkipTarget(15, 50), "manual seek while playing exempts chapter");
    Expect(!skip.GetSkipTarget(19, 50), "manual exemption lasts until chapter end");
    Expect(skip.GetSkipTarget(20, 50) == 30, "adjacent chapter remains eligible");
    skip.OnManualSeek(22);
    Expect(!skip.GetSkipTarget(22, 50, false), "manual seek while paused");
    Expect(!skip.GetSkipTarget(22, 50), "play preserves paused manual exemption");
    Expect(!skip.GetSkipTarget(5, 50, false), "paused seek outside clears exemption");
    Expect(skip.GetSkipTarget(22, 50) == 30, "automatic resume has no exemption");

    skip = Example();
    Expect(!skip.GetSkipTarget(12, 50, false), "paused repeat or frame step cannot skip");
    Expect(skip.GetSkipTarget(12, 50) == 30, "resuming automatic playback skips");
    skip.ResetVisit();
    Expect(skip.GetSkipTarget(12, 50) == 30, "reset permits retry after replay");
    skip.OnManualSeek(12);
    skip.SetMarkers(L" [SponsorBlock]: Sponsor ; [SponsorBlock]: Outro ;; \t ");
    Expect(!skip.GetSkipTarget(12, 50), "marker edit preserves manual intent");
    Expect(skip.GetSkipTarget(40, 50) == 50, "multiple trimmed markers and final chapter");
    Expect(!skip.GetSkipTarget(50, 50), "media end cannot loop");
    skip.SetMarkers(L";; \t\r\n ;");
    Expect(!skip.GetSkipTarget(12, 50), "empty markers disable all skips");

    skip.SetChapters({{0, L"Intro: opening"}, {10, L"intro"}, {20, L"An Intro"}, {30, L"Content"}});
    skip.SetMarkers(L"Intro");
    Expect(skip.GetSkipTarget(0, 40) == 10, "first chapter prefix matches");
    Expect(!skip.GetSkipTarget(10, 40), "prefix is case sensitive");
    Expect(!skip.GetSkipTarget(20, 40), "marker must be at start of title");
    skip.SetMarkers(L"Intro: [ad].*");
    skip.SetChapters({{0, L"Intro: [ad].* segment"}, {10, L"Content"}});
    Expect(skip.GetSkipTarget(0, 20) == 10, "markers are literal not regex");

    skip.SetMarkers(ChapterSkip::DefaultMarkers);
    skip.SetChapters({{20, L"Content"}, {-10, ChapterSkip::DefaultMarkers},
                      {10, ChapterSkip::DefaultMarkers}, {10, L"Content"}});
    Expect(!skip.GetSkipTarget(0, 30), "negative chapter ignored and no chapter before first");
    Expect(!skip.GetSkipTarget(10, 30), "last duplicate start determines nonempty chapter");
    skip.SetChapters({{20, L"Content"}, {10, L"Content"}, {10, ChapterSkip::DefaultMarkers}});
    Expect(skip.GetSkipTarget(10, 30) == 20, "unsorted and duplicate chapters normalize");
    skip.ResetVisit();
    Expect(skip.GetSkipTarget(10, 15) == 15, "chapter end is clamped to duration");
    Expect(!skip.GetSkipTarget(10, 0), "unknown duration cannot skip");
    Expect(!skip.GetSkipTarget(-1, 30), "negative position cannot skip");
    Expect(!skip.GetSkipTarget(30, 30), "out of range position cannot skip");
    skip.OnManualSeek(10);
    skip.SetChapters({{10, ChapterSkip::DefaultMarkers}});
    Expect(skip.GetSkipTarget(10, 20) == 20, "new chapter bag clears manual exemption");
    skip.SetChapters({});
    Expect(!skip.GetSkipTarget(10, 20), "closing file clears chapters");

    skip = Example();
    Expect(skip.GetSkipTarget(12, 50) == 30, "initial attempt");
    skip.SetMarkers(ChapterSkip::DefaultMarkers);
    Expect(!skip.GetSkipTarget(12, 50), "unchanged markers preserve attempt suppression");
    skip.SetMarkers(L"[SponsorBlock]: Sponsor;[SponsorBlock]: Outro");
    Expect(skip.GetSkipTarget(12, 50) == 30, "changed markers permit reevaluation");
    skip.OnManualSeek(2);
    Expect(skip.GetSkipTarget(12, 50) == 30, "manual seek into content does not exempt sponsor");

    skip = Example();
    {
        ChapterSkip::SeekScope seekScope(skip);
        skip.OnManualSeek(12);
        Expect(!skip.GetSkipTarget(5, 50), "old position during seek cannot clear exemption");
        {
            ChapterSkip::SeekScope nestedScope(skip);
            Expect(!skip.GetSkipTarget(22, 50), "nested seek cannot trigger skip");
        }
        Expect(!skip.GetSkipTarget(22, 50), "outer seek remains protected");
    }
    Expect(!skip.GetSkipTarget(12, 50), "manual exemption survives seek callbacks");
    Expect(skip.GetSkipTarget(22, 50) == 30, "skipping resumes after seek scope");

    skip.SetChapters({{10, ChapterSkip::DefaultMarkers}, {20, L"Content"}});
    skip.OnManualSeek(12);
    skip.SetChapters({{20, L"Content"}, {10, ChapterSkip::DefaultMarkers}});
    Expect(!skip.GetSkipTarget(12, 30), "presentation refresh preserves manual exemption");
    skip.ResetVisit();
    Expect(skip.GetSkipTarget(12, 30) == 20, "reset after refresh allows skipping");
    skip.SetChapters({{10, ChapterSkip::DefaultMarkers}, {20, L"Content"}});
    Expect(!skip.GetSkipTarget(12, 30), "presentation refresh preserves failed attempt");

    skip = Example();
    auto generation = skip.GetGeneration();
    Expect(skip.GetSkipTarget(12, 50, true, generation) == 30, "prepare automatic seek");
    Expect(skip.CanApplySkip(generation), "fresh automatic seek can execute");
    {
        ChapterSkip::SeekScope seekScope(skip);
        skip.OnManualSeek(22);
        Expect(!skip.CanApplySkip(generation), "nested manual request cancels prepared automatic seek");
    }
    Expect(!skip.GetSkipTarget(22, 50), "nested manual destination remains exempt");
    generation = skip.GetGeneration();
    skip.OnManualSeek(12);
    Expect(!skip.GetSkipTarget(35, 50, true, generation), "stale position query ignored after manual request");
    Expect(!skip.GetSkipTarget(12, 50), "stale position cannot clear new manual exemption");
    generation = skip.GetGeneration();
    skip.OnManualSeek(22);
    Expect(!skip.GetSkipTarget(12, 50, true, generation), "stale marked position cannot trigger skip");
    skip.ResetVisit();
    generation = skip.GetGeneration();
    Expect(skip.GetSkipTarget(12, 50, true, generation) == 30, "prepare seek before pause");
    skip.CancelPendingSkip();
    Expect(!skip.CanApplySkip(generation), "pause cancels prepared automatic seek");
    Expect(!skip.GetSkipTarget(12, 50, false), "paused observation remains passive");
    generation = skip.GetGeneration();
    Expect(skip.GetSkipTarget(12, 50, true, generation) == 30, "resume retries canceled skip");
    skip.SetMarkers(L"[SponsorBlock]: Sponsor;Intro");
    Expect(!skip.CanApplySkip(generation), "marker edit cancels obsolete target");
    skip.OnManualSeek(12);
    skip.CancelPendingSkip();
    Expect(!skip.GetSkipTarget(12, 50), "canceling pending skips preserves manual exemption");
    skip.ResetVisit();
    generation = skip.GetGeneration();
    Expect(skip.GetSkipTarget(12, 50, true, generation) == 30, "prepare seek before chapter change");
    skip.SetChapters({{0, L"Content"}});
    Expect(!skip.CanApplySkip(generation), "chapter replacement cancels obsolete target");

    std::cout << checks << " chapter skip checks passed\n";
    return 0;
}
