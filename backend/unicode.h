// Unicode 工具 (gōngjù — Unicode helpers)
// NFC 归一化 + 字符分类校验 (guīyī huà + zìfú fēnlèi — normalization + category validation) — via ICU

#pragma once

#include <string>

using namespace std;

namespace uni {
    // NFC 归一化 (guīyī huà — normalize to NFC)
    string nfc(const string& s);
    // 是否仅包含字母(L*)与十进制数字(Nd) (shìfǒu jǐn bāohán — true if only letters + decimal digits)
    bool is_letter_or_digit(const string& s);
}
