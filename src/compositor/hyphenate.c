// hyphenate.c - Hyphenation for compositor

#include "compositor_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Case-insensitive string comparison (avoids _stricmp which can deadlock on
// Windows when stderr has been redirected via freopen with unbuffered mode)
static int hyph_stricmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

// ============================================================================
// Hyphenation Exceptions
// ============================================================================

static const struct {
    const char *word;
    const char *mask;
} HYPH_EXC[] = {{"forever", "for-ever"},
                {"wherever", "where-ver"},
                {"however", "how-ever"},
                {"because", "be-cause"},
                {"between", "be-tween"},
                {"upon", "up-on"},
                {"mathematician", "math-e-ma-ti-cian"},
                {"pattern", "pat-tern"},
                {"paragraph", "para-graph"},
                {"computer", "com-put-er"},
                {NULL, NULL}};

static inline int is_vowel(int c) {
    c = tolower((unsigned char)c);
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y');
}

static inline int is_all_caps_ascii(const char *s) {
    int has = 0;
    for (; *s; ++s) {
        unsigned char c = *s;
        if (isalpha(c)) {
            has = 1;
            if (!isupper(c)) return 0;
        }
    }
    return has;
}

// ============================================================================
// Hyphenation Points
// ============================================================================

int hyphen_points(const char *w, int *pos, int maxpos, int Lmin, int Rmin) {
    int n = (int)strlen(w);
    if (n < Lmin + Rmin + 1) return 0;

    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)w[i];
        if (c < 32 || c >= 127) return 0;
    }

    if (n < 4) return 0;
    if (is_all_caps_ascii(w)) return 0;
    if (isupper((unsigned char)w[0])) return 0;

    for (int i = 0; HYPH_EXC[i].word; ++i) {
        if (hyph_stricmp(HYPH_EXC[i].word, w) == 0) {
            const char *m = HYPH_EXC[i].mask;
            int k = 0, idx = 0;
            for (int j = 0; m[j]; ++j) {
                if (m[j] == '-') {
                    if (idx >= Lmin && (n - idx) >= Rmin && k < maxpos) pos[k++] = idx;
                } else
                    idx++;
            }
            return k;
        }
    }

    int k = 0;
    for (int i = 1; i < n; i++) {
        int L = tolower((unsigned char)w[i - 1]);
        int R = tolower((unsigned char)w[i]);

        if (i + 2 < n && w[i] == 'i' && w[i + 1] == 'n' && w[i + 2] == 'g') continue;
        if (i + 1 < n && w[i] == 'e' && w[i + 1] == 'd') continue;

        if (is_vowel(L) && !is_vowel(R) && i >= Lmin && (n - i) >= Rmin) {
            if (k < maxpos) pos[k++] = i;
        }
    }
    return k;
}

int hyphen_best_fit(const char *w, int avail, int Lmin, int Rmin) {
    if (avail <= 1) return -1;
    int pts[64], k = hyphen_points(w, pts, 64, Lmin, Rmin);
    if (k <= 0) return -1;

    int best = -1, limit = avail - 1;
    for (int i = 0; i < k; i++) {
        if (pts[i] <= limit && pts[i] > best) best = pts[i];
    }
    return best;
}

int hyphen_best_fit_unicode(const char *w, int avail, int Lmin, int Rmin, int *cut_cols) {
    if (avail <= 1) return -1;

    int ascii_result = hyphen_best_fit(w, avail, Lmin, Rmin);
    if (ascii_result >= 0) {
        if (cut_cols) *cut_cols = ascii_result;
        return ascii_result;
    }

    int *offsets = NULL;
    int num_codepoints = 0;
    char *ascii = normalize_for_hyphenation(w, &offsets, &num_codepoints);
    if (!ascii) return -1;

    int pts[64], k = hyphen_points(ascii, pts, 64, Lmin, Rmin);
    if (k <= 0) {
        free(ascii);
        free(offsets);
        return -1;
    }

    int best_cp = -1, limit = avail - 1;
    for (int i = 0; i < k; i++) {
        if (pts[i] <= limit && pts[i] > best_cp) best_cp = pts[i];
    }

    int result = -1;
    if (best_cp > 0) {
        result = offsets[best_cp];
        if (cut_cols) *cut_cols = best_cp;
    }

    free(ascii);
    free(offsets);
    return result;
}
