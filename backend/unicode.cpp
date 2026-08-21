// Unicode 工具实现 (gōngjù shíxiàn — Unicode helpers implementation)
#include "unicode.h"

#include <unicode/utypes.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>
#include <vector>

using namespace std;

namespace uni {

string nfc(const string& s) {
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* norm = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status) || !norm) return s;
    // UTF-8 → UTF-16
    int32_t u16len = 0;
    u_strFromUTF8(nullptr, 0, &u16len, s.data(), (int32_t)s.size(), &status);
    status = U_ZERO_ERROR;
    vector<UChar> buf(u16len + 1);
    u_strFromUTF8(buf.data(), (int32_t)buf.size(), nullptr, s.data(), (int32_t)s.size(), &status);
    if (U_FAILURE(status)) return s;
    // NFC 归一化 (UTF-16)
    int32_t normlen = unorm2_normalize(norm, buf.data(), u16len, nullptr, 0, &status);
    status = U_ZERO_ERROR;
    vector<UChar> nbuf(normlen + 1);
    normlen = unorm2_normalize(norm, buf.data(), u16len, nbuf.data(), (int32_t)nbuf.size(), &status);
    if (U_FAILURE(status)) return s;
    // UTF-16 → UTF-8
    int32_t u8len = 0;
    u_strToUTF8(nullptr, 0, &u8len, nbuf.data(), normlen, &status);
    status = U_ZERO_ERROR;
    string out(u8len, '\0');
    u_strToUTF8(&out[0], (int32_t)out.size(), nullptr, nbuf.data(), normlen, &status);
    if (U_FAILURE(status)) return s;
    return out;
}

bool is_letter_or_digit(const string& s) {
    if (s.empty()) return false;
    UErrorCode status = U_ZERO_ERROR;
    int32_t u16len = 0;
    u_strFromUTF8(nullptr, 0, &u16len, s.data(), (int32_t)s.size(), &status);
    status = U_ZERO_ERROR;
    vector<UChar> buf(u16len + 1);
    u_strFromUTF8(buf.data(), (int32_t)buf.size(), nullptr, s.data(), (int32_t)s.size(), &status);
    if (U_FAILURE(status)) return false;
    int32_t i = 0;
    while (i < u16len) {
        UChar32 cp;
        U16_NEXT(buf.data(), i, u16len, cp);
        int8_t t = u_charType(cp);
        bool letter = (t >= U_UPPERCASE_LETTER && t <= U_OTHER_LETTER);
        bool digit = (t == U_DECIMAL_DIGIT_NUMBER);
        if (!letter && !digit) return false;
    }
    return true;
}

} // namespace uni
