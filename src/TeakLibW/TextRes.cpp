#include <regex>

#include "StdAfx.h"

const char *ExcTextResNotOpened = "TextRes not opened!";
const char *ExcTextResStaticOverflow = "TextRes is too long: %lx:%lx";
const char *ExcTextResFormat = "Bad TextRes format: %s (%li)";
const char *ExcTextResNotFound = "TextRes not found: %lx:%lx";

SLONG gLanguage;

std::string FindLanguageInString(const char* Dst, const SLONG wantedLanguageIndex) {
    const std::string allLanguageTokens = "DEFTPNISOBJKLMNQRTUV";

    const std::string targetLanguageToken(1, allLanguageTokens[wantedLanguageIndex]);
    std::regex languageTextPattern("^.*" + targetLanguageToken + "::(.*?)(?:[" + allLanguageTokens + "]::.*)?$");

    std::smatch match;
    std::string s(Dst);
    if (std::regex_search(s, match, languageTextPattern)) {
        return match[1];
    } else {
        return "";
    }
}

void LanguageSpecifyString(char *Dst) {
    const SLONG wantedLanguageIndex = 2;//gLanguage;

    std::string foundText = FindLanguageInString(Dst, wantedLanguageIndex);
    if (foundText.empty()) {
        // If we haven't found the language we want, try English
        foundText = FindLanguageInString(Dst, 1/*E - English*/);
    }

    // If we still haven't found anything, just return
    if (foundText.empty()) {
        return;
    }

    memmove(Dst, foundText.c_str(), foundText.length());
    Dst[foundText.length()] = 0;
}

TEXTRES::TEXTRES() = default;

TEXTRES::~TEXTRES() {
    if (hasOverride) {
        delete override;
    }
};

void TEXTRES::Open(char const *source, void *cached) {
    Strings.Clear();
    Path.Clear();
    Entries.Clear();
    if (cached != nullptr) {
        SLONG Group = -1;
        SLONG Identifier = -1;

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
        { delete[] String; }
    } else {
        Path.ReSize(strlen(source) + 1);
        strcpy(Path.getData(), source);
    }
}

char *TEXTRES::FindP(ULONG group, ULONG id) {
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

    char *buffer = new char[strlen(text) + 1];
    strcpy(buffer, text);
    LanguageSpecifyString(buffer);

   if (strlen(buffer) > 0x3FF) {
        delete[] buffer;
        TeakLibW_Exception(FNL, ExcTextResStaticOverflow, group, id);
    }
    static char result[0x3FF];
    strcpy(result, buffer);
    delete[] buffer;
    return result;
}

char *TEXTRES::FindS(ULONG group, ULONG id) {
    return TEXTRES::FindP(group, id);
}

char *TEXTRES::GetP(ULONG group, ULONG id) {
    char *buffer = TEXTRES::FindP(group, id);
    if (buffer == nullptr) {
        TeakLibW_Exception(FNL, ExcTextResNotFound, group, id);
        return nullptr;
    }
    return buffer;
}

char *TEXTRES::GetS(ULONG group, ULONG id) {
    char *buffer = TEXTRES::FindS(group, id);
    if (buffer == nullptr) {
        TeakLibW_Exception(FNL, ExcTextResNotFound, group, id);
        return nullptr;
    }
    return buffer;
}

// Override management
char *TEXTRES::FindOverridenS(ULONG group, ULONG id) {
    if (!hasOverride) {
        return nullptr;
    }
    return override->FindS(group, id);
}

void TEXTRES::SetOverrideFile(char const *c) {
    if (!DoesFileExist(c)) {
        return;
    }
    override = new TEXTRES();
    override->Open(c, TEXTRES_CACHED);
    hasOverride = true;
}
