#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ChapterSkip
{
public:
    using Time = std::int64_t;
    struct Chapter {
        Time start;
        std::wstring title;
    };
    static constexpr wchar_t DefaultMarkers[] = L"[SponsorBlock]: Sponsor";

    class SeekScope
    {
    public:
        explicit SeekScope(ChapterSkip& owner) : m_owner(owner) { ++m_owner.m_seekDepth; }
        ~SeekScope() { --m_owner.m_seekDepth; }
        SeekScope(const SeekScope&) = delete;
        SeekScope& operator=(const SeekScope&) = delete;

    private:
        ChapterSkip& m_owner;
    };

    void SetChapters(std::vector<Chapter> chapters)
    {
        chapters.erase(std::remove_if(chapters.begin(), chapters.end(), [](const Chapter& chapter) {
            return chapter.start < 0;
        }), chapters.end());
        std::stable_sort(chapters.begin(), chapters.end(), [](const Chapter& a, const Chapter& b) {
            return a.start < b.start;
        });
        std::vector<Chapter> normalized;
        for (auto& chapter : chapters) {
            if (!normalized.empty() && normalized.back().start == chapter.start) {
                normalized.back() = std::move(chapter);
            } else {
                normalized.push_back(std::move(chapter));
            }
        }
        if (std::equal(normalized.begin(), normalized.end(), m_chapters.begin(), m_chapters.end(),
                       [](const Chapter& a, const Chapter& b) { return a.start == b.start && a.title == b.title; })) {
            return;
        }
        m_chapters = std::move(normalized);
        ResetVisit();
    }

    void SetMarkers(std::wstring_view text)
    {
        if (m_markerText == text) {
            return;
        }
        m_markerText = text;
        m_markers.clear();
        while (!text.empty()) {
            const auto separator = text.find(L';');
            const auto entry = text.substr(0, separator);
            const auto first = entry.find_first_not_of(L" \t\r\n\v\f");
            if (first != std::wstring_view::npos) {
                const auto last = entry.find_last_not_of(L" \t\r\n\v\f");
                m_markers.emplace_back(entry.substr(first, last - first + 1));
            }
            if (separator == std::wstring_view::npos) {
                break;
            }
            text.remove_prefix(separator + 1);
        }
        CancelPendingSkip();
    }

    void ResetVisit()
    {
        m_manualChapter.reset();
        CancelPendingSkip();
    }

    void OnManualSeek(Time position)
    {
        ResetVisit();
        m_manualChapter = FindChapter(position);
    }

    std::uint64_t GetGeneration() const { return m_generation; }

    bool CanApplySkip(std::uint64_t generation) const
    {
        return generation == m_generation && m_attempt.has_value();
    }

    void CancelPendingSkip()
    {
        m_attempt.reset();
        ++m_generation;
    }

    std::optional<Time> GetSkipTarget(Time position, Time duration, bool allowSkip = true,
                                      std::optional<std::uint64_t> generation = std::nullopt)
    {
        if (m_seekDepth || (generation && *generation != m_generation)) {
            return std::nullopt;
        }
        if (position < 0 || position >= duration) {
            ResetVisit();
            return std::nullopt;
        }
        const auto chapter = FindChapter(position);
        if (chapter != m_manualChapter) {
            m_manualChapter.reset();
        }
        if (m_attempt && (position < m_attempt->first || position >= m_attempt->second)) {
            m_attempt.reset();
        }
        if (!allowSkip || !chapter || chapter == m_manualChapter || m_attempt || !Matches(*chapter)) {
            return std::nullopt;
        }
        auto next = *chapter + 1;
        while (next < m_chapters.size() && m_chapters[next].start < duration && Matches(next)) {
            ++next;
        }
        const auto end = next < m_chapters.size() ? (std::min)(duration, m_chapters[next].start) : duration;
        if (end <= position) {
            return std::nullopt;
        }
        m_attempt = std::make_pair(m_chapters[*chapter].start, end);
        return end;
    }

private:
    std::optional<size_t> FindChapter(Time position) const
    {
        const auto next = std::upper_bound(m_chapters.begin(), m_chapters.end(), position,
                                          [](Time time, const Chapter& chapter) { return time < chapter.start; });
        if (next == m_chapters.begin()) {
            return std::nullopt;
        }
        return static_cast<size_t>(next - m_chapters.begin() - 1);
    }

    bool Matches(size_t chapter) const
    {
        const auto& title = m_chapters[chapter].title;
        return std::any_of(m_markers.begin(), m_markers.end(), [&title](const std::wstring& marker) {
            return title.compare(0, marker.size(), marker) == 0;
        });
    }

    std::vector<Chapter> m_chapters;
    std::wstring m_markerText = DefaultMarkers;
    std::vector<std::wstring> m_markers{DefaultMarkers};
    std::optional<size_t> m_manualChapter;
    std::optional<std::pair<Time, Time>> m_attempt;
    unsigned m_seekDepth = 0;
    std::uint64_t m_generation = 0;
};
