#include <regex>

#include "StdAfx.h"

#define AT_Log(...) AT_Log_I("TextRes", __VA_ARGS__)

const char *ExcTextResNotOpened = "TextRes not opened!";
const char *ExcTextResStaticOverflow = "TextRes is too long: %lx:%lx";
const char *ExcTextResFormat = "Bad TextRes format: %s (%li)";
const char *ExcTextResNotFound = "The following translation was not found: %s>>%li";

SLONG gLanguage;

inline bool checkStartAnyLang(char *str) {
    bool longEnough = (str && str[0] && str[1] && str[2]);
    if (!longEnough) {
        return false;
    }

    return (str[1] == ':' && str[2] == ':');
}

inline bool checkStartSpecificLang(char *str, char langIdent) {
    bool longEnough = (str && str[0] && str[1] && str[2]);
    if (!longEnough) {
        return false;
    }

    return (str[0] == langIdent && str[1] == ':' && str[2] == ':');
}

void TEXTRES::LanguageSpecifyString(char *Dst, bool fallback) {
    const char *langs = "DEFTPNISOBJKLMNQRTUV";
    char lang = fallback ? 'E' : langs[gLanguage];

    if (!checkStartAnyLang(Dst)) {
        return;
    }

    for (SLONG i = 0; Dst[i] != 0; ++i) {
        if (checkStartSpecificLang(Dst + i, lang)) {
            SLONG j = i + 2;
            while ((Dst[j] != 0) && !checkStartAnyLang(Dst + j)) {
                ++j;
            }
            memmove(Dst, &Dst[i + 3], j - i - 3);
            Dst[j - i - 3] = 0;
            if (j - i - 3 > 0 && Dst[j - i - 4] == 32) {
                Dst[j - i - 4] = 0;
            }
            return;
        }
    }

    /* use english as a fallback */
    if (!fallback) {
        LanguageSpecifyString(Dst, true);
    }
}

TEXTRES::TEXTRES() { ResultStr.resize(0xFFF); }

TEXTRES::~TEXTRES() {
    if (hasOverride) {
        delete override;
    }
};

void TEXTRES::Open(char const *source) {
    Strings.Clear();
    Path.Clear();
    Entries.Clear();

    SLONG Group = -1;
    SLONG Identifier = -1;

    if (!DoesFileExist(source)) {
        AT_Log("TextRes file not found: %s", source);
        return;
    }

    auto FileBuffer = LoadCompleteFile(source);
    char *String = new char[0x400U];
    if (String == nullptr) {
        TeakLibW_Exception(FNL, ExcOutOfMem);
    }

    SLONG AnzStrings = 0;
    SLONG AnzEntries = 0;
    for (SLONG i = 0, j = 0; i < FileBuffer.AnzEntries(); i += j) {
        if (FileBuffer[i] == '>' && FileBuffer[i + 1] == '>') {
            ++AnzEntries;
        }
        SLONG AnzChars = 0;
        SLONG AnzNonSpace = 0;
        for (j = 0; j + i < FileBuffer.AnzEntries() && FileBuffer[j + i] != '\r' && FileBuffer[j + i] != '\n' && FileBuffer[j + i] != '\x1A'; ++j) {
            if (FileBuffer[j + i] == '/' && FileBuffer[j + i + 1] == '/') {
                AnzChars = -1;
            }
            if (AnzChars >= 0) {
                ++AnzChars;
            }
            if (FileBuffer[j + i] != ' ' && AnzChars >= 0) {
                AnzNonSpace = AnzChars;
            }
        }
        if (FileBuffer[i] == ' ' && FileBuffer[i + 1] == ' ' && FileBuffer[i + 2] != ' ') {
            AnzStrings += AnzNonSpace + 1;
        }
        while (j + i < FileBuffer.AnzEntries() && (FileBuffer[j + i] == '\r' || FileBuffer[j + i] == '\n' || FileBuffer[j + i] == '\x1A')) {
            ++j;
        }
    }
    Strings.ReSize(AnzStrings + 5);
    Entries.ReSize(AnzEntries);

    for (SLONG i = 0; i < Entries.AnzEntries(); ++i) {
        Entries[i].Text = nullptr;
    }

    AnzStrings = 0;
    AnzEntries = -1;
    for (SLONG i = 0, j = 0; i < FileBuffer.AnzEntries(); i += j) {
        SLONG Size = 0;
        if (FileBuffer.AnzEntries() - i <= 1023) {
            Size = FileBuffer.AnzEntries() - i;
        } else {
            Size = 1023;
        }
        memcpy(String, FileBuffer.getData() + i, Size);
        for (j = 0; i + j < FileBuffer.AnzEntries() && String[j] != '\r' && String[j] != '\n' && String[j] != '\x1A'; ++j) {
            ;
        }
        String[j] = 0;
        TeakStrRemoveCppComment(String);
        TeakStrRemoveEndingCodes(String, " ");
        if (String[0] == '>' && String[1] != '>') {
            if (strlen(String + 1) == 4) {
                Group = *reinterpret_cast<SLONG *>(String + 1);
            } else {
                Group = atoi(String + 1);
            }
        }
        if (String[0] == '>' && String[1] == '>') {
            if (strlen(String + 2) == 4) {
                Identifier = *reinterpret_cast<SLONG *>(String + 2);
            } else {
                Identifier = atoi(String + 2);
            }
            ++AnzEntries;
        }
        if (String[0] == ' ' && String[1] == ' ' && strlen(String) > 2 && AnzEntries >= 0) {
            if (Entries[AnzEntries].Text != nullptr) {
                strcat(Entries[AnzEntries].Text, String + 2);
                strcat(Entries[AnzEntries].Text, "");
                AnzStrings += strlen(String + 2) + 1;
            } else {
                if (AnzEntries >= Entries.AnzEntries()) {
                    TeakLibW_Exception(FNL, ExcImpossible, "");
                }
                Entries[AnzEntries].Group = Group;
                Entries[AnzEntries].Id = Identifier;
                Entries[AnzEntries].Text = Strings.getData() + AnzStrings;
                strcpy(Entries[AnzEntries].Text, String + 2);
                if (strlen(String + 2) + AnzStrings >= Strings.AnzEntries()) {
                    TeakLibW_Exception(FNL, ExcImpossible, "");
                }
                AnzStrings += strlen(String + 2) + 1;
            }
        }
        while (j + i < FileBuffer.AnzEntries() && (FileBuffer[j + i] == '\r' || FileBuffer[j + i] == '\n' || FileBuffer[j + i] == '\x1A')) {
            ++j;
        }
    }

    delete[] String;
}

char *TEXTRES::FindTranslation(ULONG group, ULONG id) {
    char *text = FindOverridenS(group, id);
    if (text != nullptr) {
        return text;
    }
    for (SLONG i = 0; i < Entries.AnzEntries(); ++i) {
        if (Entries[i].Group == group && Entries[i].Id == id) {
            text = Entries[i].Text;
            break;
        }
    }

    if (text == nullptr) {
        return nullptr;
    }

    if (strlen(text) > ResultStr.size()) {
        TeakLibW_Exception(FNL, ExcTextResStaticOverflow, group, id);
    }

    strncpy(ResultStr.data(), text, ResultStr.size());
    ResultStr[(int)ResultStr.size()] = '\0';
    LanguageSpecifyString(ResultStr.data());
    return ResultStr.data();
}

char *TEXTRES::GetP(ULONG group, ULONG id) {
    char *buffer = FindTranslation(group, id);
    if (buffer == nullptr) {
        TeakLibW_Exception(FNL, ExcTextResNotFound, group, id);
        return nullptr;
    }
    return buffer;
}

char *TEXTRES::GetS(ULONG group, ULONG id) {
    char *buffer = FindTranslation(group, id);
    if (buffer == nullptr) {
        long long lGroup = group;
        AT_Log(ExcTextResNotFound, reinterpret_cast<char *>(&lGroup), id);

        char *defaultText = strdup("MISSING");
        return defaultText;
    }
    return buffer;
}

// Override management
char *TEXTRES::FindOverridenS(ULONG group, ULONG id) {
    if (!hasOverride) {
        return nullptr;
    }
    return override->FindTranslation(group, id);
}

void TEXTRES::SetOverrideFile(char const *c) {
    if (!DoesFileExist(c)) {
        return;
    }
    override = new TEXTRES();
    override->Open(c);
    hasOverride = true;
}
