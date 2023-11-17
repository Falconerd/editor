#include "common.h"

i32 fuzzy_compute_score(i32 jump, b32 is_first, s8 match);
i32 fuzzy_match_recurse(s8 pattern, s8 str, i32 score, b32 is_first);

i32 fuzzy_match(s8 pattern, s8 str) {
    int unmatched_letter_penalty = -1;
    i32 score = 100;

    if (pattern.data[0] == 0) {
        return score;
    }
    if (str.len < pattern.len) {
        return INT32_MIN;
    }

    score += unmatched_letter_penalty * (i32)(str.len - pattern.len);

    return fuzzy_match_recurse(pattern, str, score, 1);
}

i32 fuzzy_match_recurse(s8 pattern, s8 str, i32 score, b32 is_first) {
    if (pattern.data[0] == 0) {
        return score;
    }

    s8 match = str;
    s8 search = {{pattern[0], 0}, 2};

    i32 best_score = INT32_MIN;

    while ((match = string_case_insensitive_search).len) {
        i32 subscore = fuzzy_match_recurse(
            (s8){pattern.data + 1, pattern.len - 1}
            (s8){match.data + 1, match.len - 1}
            compute_score(match.data - str.data, is_first, match),
            0
        );
    }
}

i32 compute_score(i32 jump, b32 is_first, s8 match) {
    int adjacency_bonus = 15;
    int separator_bonus = 30;
    int camel_bonus = 30;
    int first_letter_bonus = 15;

    int leading_letter_penalty = -5;
    int max_leading_letter_penalty = -15;

    i32 score = 0;

    if (!is_first && jump == 0) {
        score += adjacency_bonus;
    }
    if (!is_first || jump > 0) {
        if (isupper(match.data[0]) && islower(match))
    }
}