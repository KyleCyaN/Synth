#include "Language.h"
#include "json.hpp"
#include "languages/en_US.json.h"
#include "languages/ja_JP.json.h"
#include "languages/ru_RU.json.h"
#include "languages/es_ES.json.h"
#include "languages/pt_BR.json.h"
#include "languages/fr_FR.json.h"
#include "languages/ko_KR.json.h"
#include "languages/ar_SA.json.h"
#include "languages/zh_CN_SP.json.h"
#include "languages/zh_CN_TR.json.h"

Language& Language::Instance()
{
    static Language inst;
    return inst;
}

static LangCode LangCodeFromString(const std::string& code)
{
    static const std::unordered_map<std::string, LangCode> map = {
        {"zh_CN_SP", LangCode::zh_CN_SP},
        {"zh_CN_TR", LangCode::zh_CN_TR},
        {"en_US", LangCode::en_US},
        {"ja_JP", LangCode::ja_JP},
        {"ru_RU", LangCode::ru_RU},
        {"fr_FR", LangCode::fr_FR},
        {"es_ES", LangCode::es_ES},
        {"pt_BR", LangCode::pt_BR},
        {"ko_KR", LangCode::ko_KR},
        {"ar_SA", LangCode::ar_SA},
    };

    if (auto it = map.find(code); it != map.end())
        return it->second;
    return LangCode::Invalid;
}

bool Language::Load(const std::string& langCode)
{
    m_strings.clear();

    std::string_view jsonData;
    bool ok = false;

    switch (LangCodeFromString(langCode))
    {
        case LangCode::zh_CN_SP:
            jsonData = std::string_view(zh_CN_SP_JSON, sizeof(zh_CN_SP_JSON) - 1);
            ok = true;
            break;
        case LangCode::zh_CN_TR:
            jsonData = std::string_view(zh_CN_TR_JSON, sizeof(zh_CN_TR_JSON) - 1);
            ok = true;
            break;
        case LangCode::en_US:
            jsonData = std::string_view(en_US_JSON, sizeof(en_US_JSON) - 1);
            ok = true;
            break;
        case LangCode::ja_JP:
            jsonData = std::string_view(ja_JP_JSON, sizeof(ja_JP_JSON) - 1);
            ok = true;
            break;
        case LangCode::ru_RU:
            jsonData = std::string_view(ru_RU_JSON, sizeof(ru_RU_JSON) - 1);
            ok = true;
            break;
        case LangCode::fr_FR:
            jsonData = std::string_view(fr_FR_JSON, sizeof(fr_FR_JSON) - 1);
            ok = true;
            break;
        case LangCode::es_ES:
            jsonData = std::string_view(es_ES_JSON, sizeof(es_ES_JSON) - 1);
            ok = true;
            break;
        case LangCode::pt_BR:
            jsonData = std::string_view(pt_BR_JSON, sizeof(pt_BR_JSON) - 1);
            ok = true;
            break;
        case LangCode::ko_KR:
            jsonData = std::string_view(ko_KR_JSON, sizeof(ko_KR_JSON) - 1);
            ok = true;
            break;
        case LangCode::ar_SA:
            jsonData = std::string_view(ar_SA_JSON, sizeof(ar_SA_JSON) - 1);
            ok = true;
            break;
        default:
            return false;
    }

    if (!ok)
        return false;

    try
    {
        for (nlohmann::json j = nlohmann::json::parse(jsonData); auto& [k, v] : j.items())
        {
            m_strings[k] = v.get<std::string>();
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const char* Language::Get(const std::string& key) const
{
    if (const auto it = m_strings.find(key); it != m_strings.end())
        return it->second.c_str();

    static std::string fallback = "[MISSING:" + key + "]";
    return fallback.c_str();
}