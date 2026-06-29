#pragma once

#include <string>
#include <unordered_map>

#define LOC(key) Language::Instance().Get(key)
enum class LangCode : uint8_t
{
    Invalid = 0,
    en_US,
    zh_CN_SP,
    zh_CN_TR,
    ja_JP,
    ru_RU,
    fr_FR,
    es_ES,
    pt_BR,
    ko_KR,
    ar_SA,
};

class Language {
public:
    static Language &Instance();

    bool Load(const std::string &langCode);
    const char *Get(const std::string &key) const;

private:
    Language() = default;

    std::string m_currentLang;
    std::unordered_map<std::string, std::string> m_strings;
};
