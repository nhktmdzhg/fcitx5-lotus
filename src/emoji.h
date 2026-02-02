#ifndef EMOJI_H
#define EMOJI_H

#include <cstddef>
#include <string>
#include <vector>
#include <algorithm>

struct EmojiEntry {
    std::string trigger;
    std::string output;
};

#include "emoji_data.h"

class EmojiLoader {
  private:
    std::vector<EmojiEntry> emojiList;

  public:
    EmojiLoader() {
        emojiList = EMOJI_LIST;
    }

    std::vector<EmojiEntry> search(const std::string& prefix) {
        if (prefix.empty())
            return {};

        struct EmojiEntryFuzzy {
            EmojiEntry entry;
            int        score;
        };
        std::vector<EmojiEntryFuzzy> results;

        for (const auto& entry : emojiList) {
            int    score              = 0;
            size_t queryIndex         = 0;
            int    lastMatchIndex     = -1;
            int    consecutiveMatches = 0;
            int    firstMatchIndex    = -1;

            for (size_t i = 0; i < entry.trigger.size() && queryIndex < prefix.size(); ++i) {
                if (entry.trigger[i] == prefix[queryIndex]) {
                    if (queryIndex == 0)
                        firstMatchIndex = i;

                    if (lastMatchIndex != -1 && (int)i == lastMatchIndex + 1) {
                        ++consecutiveMatches;
                        score += (20 * consecutiveMatches);
                    } else {
                        consecutiveMatches = 0;
                    }

                    if (i == 0 || entry.trigger[i - 1] == '_' || entry.trigger[i - 1] == '-') {
                        score += 50;
                    }

                    lastMatchIndex = i;
                    ++queryIndex;
                }
            }
            if (queryIndex == prefix.size()) {
                if (firstMatchIndex == 0)
                    score += 100;

                score -= static_cast<int>(entry.trigger.size());

                score -= (lastMatchIndex - firstMatchIndex);

                results.push_back({entry, score});
            }
        }

        std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
            if (a.score != b.score)
                return a.score > b.score;
            return a.entry.trigger.size() < b.entry.trigger.size();
        });

        std::vector<EmojiEntry> finalResults;
        for (const auto& result : results) {
            finalResults.push_back(result.entry);
        }
        return finalResults;
    }

    size_t size() const {
        return emojiList.size();
    }
};

#endif // EMOJI_H
