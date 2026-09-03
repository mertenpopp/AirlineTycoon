//============================================================================================
// SmackPrs.cpp : Wrapper-Klasse für den Smacker für 16Bit Farben bei 8-Bit Videos:
//============================================================================================
#include "SmackPrs.h"

#include "global.h"
#include "helper.h"
#include "Proto.h"

#include <smacker.h>

#include <cmath>

#define AT_Error(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_ERROR, "SMACK", __VA_ARGS__)
#define AT_Warn(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_WARN, "SMACK", __VA_ARGS__)
#define AT_Info(...) Hdu.HercPrintfMsg(SDL_LOG_PRIORITY_INFO, "SMACK", __VA_ARGS__)
#define AT_Log(...) AT_Log_I("SMACK", __VA_ARGS__)

//--------------------------------------------------------------------------------------------
// Berechnet eine Remapper-Tabelle anhand einer 256-Farben Palette
//--------------------------------------------------------------------------------------------
void CalculatePalettemapper(const UBYTE *pPalette, SDL_Palette *pPaletteMapper) {
    if (pPalette == nullptr) {
        return;
    }

    SDL_Color colors[256];

    for (SLONG c = 0; c < 256; c++) {
        if (pPalette[c * 3] + pPalette[c * 3 + 1] + pPalette[c * 3 + 2] == 0) {
            colors[c] = SDL_Color{4, 4, 4, 0xFF};
        } else {
            colors[c] = SDL_Color{pPalette[c * 3], pPalette[c * 3 + 1], pPalette[c * 3 + 2], 0xFF};
        }
    }

    colors[0] = SDL_Color{0, 0, 0, 255};
    SDL_SetPaletteColors(pPaletteMapper, colors, 0, 256);
}

void CalculatePalettemapperFlc(const flic::Colormap &colormap, SDL_Palette *pPaletteMapper) {
    SDL_Color colors[256];

    for (SLONG c = 0; c < 256; c++) {
        if (colormap[c].r + colormap[c].g + colormap[c].b == 0) {
            colors[c] = SDL_Color{4, 4, 4, 0xFF};
        } else {
            colors[c] = SDL_Color{colormap[c].r, colormap[c].g, colormap[c].b, 0xFF};
        }
    }

    colors[0] = SDL_Color{0, 0, 0, 255};
    SDL_SetPaletteColors(pPaletteMapper, colors, 0, 256);
}

//--------------------------------------------------------------------------------------------
// Wrapper around FLIC library, containing file handles and all data structures
//--------------------------------------------------------------------------------------------
FlcWrapper::FlcWrapper(fs::path fileInput) : Path{fileInput} {
    if (!fs::exists(Path)) {
        AT_Error("FlcWrapper: File %s does not exist!", Path.c_str());
        return;
    }
    init_decoder();
}

FlcWrapper::~FlcWrapper() {
    Decoder = {nullptr};
    FileInterface = {nullptr};
    fclose(f);
    f = nullptr;
}

void FlcWrapper::info_all(unsigned long &FrameNum, unsigned long &Frames) {
    FrameNum = Decoder.getCurrentFrameCount();
    Frames = Header.frames;
}

void FlcWrapper::info_video(unsigned long &Width, unsigned long &Height, unsigned long &Speed) {
    Width = Header.width;
    Height = Header.height;
    Speed = Header.speed;
}

char FlcWrapper::first() {
    memset(Buffer.data(), 0x0, Buffer.size() * sizeof(uint8_t));
    init_decoder();
    return render();
}

char FlcWrapper::next() { return render(); }

const unsigned char *FlcWrapper::get_video() { return Buffer.data(); }

const flic::Colormap &FlcWrapper::get_palette() { return Frame.colormap; }

void FlcWrapper::init_decoder() {
    if (f) {
        fclose(f);
    }

    f = std::fopen(Path.string().c_str(), "rb");
    if (!f) {
        AT_Error("FlcWrapper: Failed to open %s: errno %d!", Path.c_str(), errno);
        return;
    }

    FileInterface = {f};
    if (!FileInterface.ok()) {
        AT_Error("FlcWrapper: Failed to create file interface for %s!", Path.c_str());
        FileInterface = {nullptr};
        fclose(f);
        f = nullptr;
        return;
    }

    // FileInterface.seek(0);

    Decoder = {&FileInterface};
    if (!Decoder.readHeader(Header)) {
        AT_Error("FlcWrapper: Failed to read header of %s!", Path.c_str());
        Decoder = {nullptr};
        return;
    }

    Buffer.resize(Header.width * Header.height);
    Frame.rowstride = Header.width;
}

char FlcWrapper::render() {
    Frame.pixels = Buffer.data();
    if (!Decoder.readFrame(Frame)) {
        AT_Error("FlcWrapper: Failed to decode frame!");
    }

    int cur = Decoder.getCurrentFrameCount();
    if (cur >= 0 && cur < Header.frames - 1) {
        // AT_Log("%s: SMK_MORE %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_MORE;
    }
    if (cur == Header.frames - 1) {
        // AT_Log("%s: SMK_LAST %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_LAST;
    }
    if (cur == Header.frames) {
        // AT_Log("%s: SMK_DONE %lu/%lu", Path.c_str(), cur, Header.frames);
        return SMK_DONE;
    }
    AT_Error("%s: SMK_ERROR %lu/%lu", Path.c_str(), cur, Header.frames);
    return SMK_ERROR;
}

//--------------------------------------------------------------------------------------------
// CSmack16::
//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CSmack16::~CSmack16() {
    if (PaletteMapper != nullptr) {
        SDL_FreePalette(PaletteMapper);
    }
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// Öffnet ein Smacker-Filmchen:
//--------------------------------------------------------------------------------------------
void CSmack16::Open(const CString &Filename) {
    fs::path path{Filename.c_str()};
    fs::path rootPath{SmackerPath.c_str()};
    auto pathToSmk = FullFilesystemPath(path, rootPath);
    auto pathToFlc = FullFilesystemPath(path.replace_extension("flc"), rootPath);

    if (fs::exists(pathToSmk)) {
        pSmack = smk_open_file(FullFilename(Filename, SmackerPath), SMK_MODE_MEMORY);
        smk_enable_video(pSmack, 1U);
        smk_info_video(pSmack, &Width, &Height, nullptr);
        FrameNext = 0;
        State = smk_first(pSmack);
        PaletteMapper = SDL_AllocPalette(256);
        CalculatePalettemapper(smk_get_palette(pSmack), PaletteMapper);
    } else if (fs::exists(pathToFlc)) {
        // AT_Info("Using FLC fallback: %s", pathToFlc.c_str());
        pFlc = new FlcWrapper(pathToFlc);
        pFlc->info_video(Width, Height, Speed);
        FrameNext = 0;
        State = pFlc->first();
        PaletteMapper = SDL_AllocPalette(256);
        CalculatePalettemapperFlc(pFlc->get_palette(), PaletteMapper);
    }
}

//--------------------------------------------------------------------------------------------
// Nächster Frame:
//--------------------------------------------------------------------------------------------
BOOL CSmack16::NextSmk(SBBM *pTargetBm) {
    if (AtGetTime() >= FrameNext && State == SMK_MORE) {
        // Take the next frame:
        State = smk_next(pSmack);

        DOUBLE usf = NAN;
        smk_info_all(pSmack, nullptr, nullptr, &usf);
        FrameNext = AtGetTime() + (usf / 1000.0);

        if (pTargetBm != nullptr) {
            if (SLONG(Width) != pTargetBm->Size.x || SLONG(Height) != pTargetBm->Size.y) {
                pTargetBm->ReSize(XY(Width, Height), CREATE_INDEXED);
            }
            SDL_SetSurfacePalette(pTargetBm->pBitmap->GetSurface(), PaletteMapper);
            SB_CBitmapKey key(*pTargetBm->pBitmap);
            memcpy(key.Bitmap, smk_get_video(pSmack), key.lPitch * Height);
        }
    }

    return static_cast<BOOL>(State == SMK_MORE);
}

BOOL CSmack16::NextFlc(SBBM *pTargetBm) {
    if (AtGetTime() >= FrameNext && State == SMK_MORE) {
        // Take the next frame:
        State = pFlc->next();

        FrameNext = AtGetTime() + Speed;

        if (pTargetBm != nullptr) {
            if (SLONG(Width) != pTargetBm->Size.x || SLONG(Height) != pTargetBm->Size.y) {
                pTargetBm->ReSize(XY(Width, Height), CREATE_INDEXED);
            }
            SDL_SetSurfacePalette(pTargetBm->pBitmap->GetSurface(), PaletteMapper);
            SB_CBitmapKey key(*pTargetBm->pBitmap);
            memcpy(key.Bitmap, pFlc->get_video(), key.lPitch * Height);
        }
    }

    return static_cast<BOOL>(State == SMK_MORE);
}

BOOL CSmack16::Next(SBBM *pTargetBm) {
    if (pSmack != nullptr) {
        return NextSmk(pTargetBm);
    }
    if (pFlc != nullptr) {
        return NextFlc(pTargetBm);
    }
    return false;
}

//--------------------------------------------------------------------------------------------
// Und Feierabend:
//--------------------------------------------------------------------------------------------
void CSmack16::Close() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// CSmackerClip::
//--------------------------------------------------------------------------------------------
// Konstruktor:
//--------------------------------------------------------------------------------------------
CSmackerClip::CSmackerClip() { PaletteMapper = SDL_AllocPalette(256); }

//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CSmackerClip::~CSmackerClip() {
    if (PaletteMapper != nullptr) {
        SDL_FreePalette(PaletteMapper);
    }
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
}

//--------------------------------------------------------------------------------------------
// Startet die Animation
//--------------------------------------------------------------------------------------------
void CSmackerClip::Start() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;

    if (Filename.GetLength() > 0) {
        fs::path path{Filename.c_str()};
        fs::path rootPath{SmackerPath.c_str()};
        auto pathToSmk = FullFilesystemPath(path, rootPath);
        auto pathToFlc = FullFilesystemPath(path.replace_extension("flc"), rootPath);

        if (fs::exists(pathToSmk)) {
            pSmack = smk_open_file(pathToSmk.string().c_str(), SMK_MODE_MEMORY);
            smk_enable_video(pSmack, 1U);
            smk_info_all(pSmack, &FrameNum, &Frames, nullptr);
            smk_info_video(pSmack, &Width, &Height, nullptr);
            smk_first(pSmack);
            CalculatePalettemapper(smk_get_palette(pSmack), PaletteMapper);
        } else if (fs::exists(pathToFlc)) {
            pFlc = new FlcWrapper(pathToFlc);
            pFlc->info_all(FrameNum, Frames);
            pFlc->info_video(Width, Height, Speed);
            pFlc->first();
            CalculatePalettemapperFlc(pFlc->get_palette(), PaletteMapper);
        }
    }

    if (IsFXPlaying == 0) {
        if (SoundFilename.GetLength() > 0) {
            if (strchr(SoundFilename, '|') != nullptr) {
                char *p = const_cast<char *>((LPCTSTR)SoundFilename);

                NumSoundFx = 1;
                while (true) {
                    p = strchr(p + 1, '|');

                    if (p != nullptr) {
                        NumSoundFx++;
                    } else {
                        break;
                    }
                }

                NextSyllable();
            } else {
                NumSoundFx = 1;
                SoundFx.ReInit(SoundFilename, SmackerPath);
            }
        } else {
            NumSoundFx = 0;
        }
    }

    RepeatCount = Repeat.x + rand() % (Repeat.y - Repeat.x + 1);
}

//--------------------------------------------------------------------------------------------
// Beendet sie:
//--------------------------------------------------------------------------------------------
void CSmackerClip::Stop() {
    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    pSmack = nullptr;
    if (pFlc != nullptr) {
        delete pFlc;
    }
    pFlc = nullptr;
    FrameNext = 0;
    LastFrame = 0;

    // SmackPic.ReSize (0);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CSmackerClip::ReSize(SLONG ClipId, const CString &Filename, const CString &SoundFilename,

                          XY ScreenOffset, SLONG MoodId,
                          CRepeat Repeat,     // Min..Max
                          CPostWait PostWait, // Min..Max in 20/stel Sekunden
                          BOOL CanCancelClip, // Kann der Clip vorzeitig abgebrochen werden?

                          SLONG *PostVar, // NULL oder Variable, die am Animationsende gesetzt werden soll
                          SLONG PostOperation, SLONG PostValue,

                          const SLONG *DecisionVar, // wenn !=NULL, legt sie fest, welche Folgeanimation gespielt wird...

                          const CString &SuccessorTokens, // z.B. "A2X8"...
                          SLONG SuccessorIds, ...) {
    SLONG c = 0;

    if (pSmack != nullptr) {
        smk_close(pSmack);
    }
    if (pFlc != nullptr) {
        delete pFlc;
    }

    CSmackerClip::Filename = Filename;
    CSmackerClip::SoundFilename = SoundFilename;
    CSmackerClip::pSmack = nullptr;
    CSmackerClip::LastFrame = 0;
    CSmackerClip::Width = 0;
    CSmackerClip::Height = 0;
    CSmackerClip::FrameNum = 0;
    CSmackerClip::Frames = 0;
    CSmackerClip::FrameNext = 0;

    // Statische Eigenschaften...
    CSmackerClip::ClipId = ClipId;
    CSmackerClip::ScreenOffset = ScreenOffset;
    CSmackerClip::MoodId = MoodId;
    CSmackerClip::Repeat = Repeat;
    CSmackerClip::PostWait = PostWait;
    CSmackerClip::CanCancelClip = CanCancelClip;

    CSmackerClip::PostVar = PostVar;
    CSmackerClip::PostOperation = PostOperation;
    CSmackerClip::PostValue = PostValue;
    CSmackerClip::DecisionVar = DecisionVar;

    CSmackerClip::SuccessorTokens = SuccessorTokens;
    CSmackerClip::SuccessorIds.ReSize(SuccessorTokens.GetLength() / 2);

    CSmackerClip::SuccessorIds[0] = SuccessorIds;

    // Hilfskonstruktion für beliebige viele Argumente deklarieren:
    {
        va_list Vars;

        // Tabelle initialisieren:
        va_start(Vars, SuccessorIds);

        for (c = 1; c < CSmackerClip::SuccessorIds.AnzEntries(); c++) {
            CSmackerClip::SuccessorIds[c] = va_arg(Vars, SLONG);
        }

        // Daten bereinigen:
        va_end(Vars);
    }

    CSmackerClip::IsFXPlaying = FALSE;

    CSmackerClip::pFlc = nullptr;

    // Dynamische Eigenschaften...
    CSmackerClip::State = SMACKER_CLIP_INACTIVE;
    CSmackerClip::RepeatCount = 0;
    CSmackerClip::WaitCount = 0;
}

//--------------------------------------------------------------------------------------------
// Lässt die Person die nächste Silbe raussuchen:
//--------------------------------------------------------------------------------------------
void CSmackerClip::NextSyllable() {
    if (SoundFilename.GetLength() > 0) {
        char *p = const_cast<char *>((LPCTSTR)SoundFilename);

        if (NumSoundFx < 3) {
            for (SLONG n = rand() % NumSoundFx; n > 0; n--) {
                p = strchr(p, '|') + 1;
            }
        } else {
            for (SLONG n = SoundRandom.Rand(0, NumSoundFx - 1); n > 0; n--) {
                p = strchr(p, '|') + 1;
            }
        }

        char buffer[200];
        strncpy(buffer, p, sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';
        char *pp = strchr(buffer, '|');

        if (pp != nullptr) {
            *pp = 0;
        }

        SoundFx.ReInit(buffer, SmackerPath);
    }
}

//--------------------------------------------------------------------------------------------
// Lässt die Person die nächste Silbe sprechen:
//--------------------------------------------------------------------------------------------
void CSmackerClip::PlaySyllable() {
    if (SoundFx.pFX != nullptr) {
        SoundFx.Play(0, Sim.Options.OptionEffekte * 100 / 7);
        TickerNext = AtGetTime() + SoundFx.pFX->GetByteLength() * 1000 / 22000;
        IsFXPlaying = TRUE;
    } else {
        TickerNext = 0;
    }
}

//--------------------------------------------------------------------------------------------
// CSmackerPerson::
//--------------------------------------------------------------------------------------------
////Konstruktor:
//--------------------------------------------------------------------------------------------
CSmackerPerson::CSmackerPerson() {
    ActiveClip = -1;
    CurrentMood = -1;
    DesiredMood = -1;
    AcceptedMood1 = -1;
    AcceptedMood2 = -1;
}

//--------------------------------------------------------------------------------------------
// Destruktor:
//--------------------------------------------------------------------------------------------
CSmackerPerson::~CSmackerPerson() = default;

bool CSmackerPerson::clipExists(SLONG clip) const {
    if (clip < 0 || clip >= Clips.AnzEntries()) {
        return false;
    }
    return (Clips[clip].pSmack != nullptr) || (Clips[clip].pFlc != nullptr);
}

bool CSmackerPerson::isEmptyClip(SLONG clip) const {
    if (clip < 0 || clip >= Clips.AnzEntries()) {
        return false;
    }
    return (Clips[ActiveClip].pSmack == nullptr) && (Clips[ActiveClip].pFlc == nullptr);
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
void CSmackerPerson::ReSize(SLONG NumberOfClips) { Clips.ReSize(NumberOfClips); }

//--------------------------------------------------------------------------------------------
// Legt fest, wie die Person labert:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::SetSpeakFx(const CString &Filename) { SpeakFx.ReInit(Filename, SmackerPath); }

//--------------------------------------------------------------------------------------------
// Legt die Stimmung (Freizeit, Reden, Zuhören) fest, in der man jemand haben will
//--------------------------------------------------------------------------------------------
void CSmackerPerson::SetDesiredMood(SLONG DesiredMood, SLONG AcceptedMood1, SLONG AcceptedMood2) {
    CSmackerPerson::DesiredMood = DesiredMood;
    CSmackerPerson::AcceptedMood1 = AcceptedMood1;
    CSmackerPerson::AcceptedMood2 = AcceptedMood2;
}

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetMood() { return (CurrentMood); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetDesiredMood() { return (DesiredMood); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetClip() { return (ActiveClip); }

//--------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------
SLONG CSmackerPerson::GetFrame() const {
    if (clipExists(ActiveClip)) {
        return (Clips[ActiveClip].FrameNum);
    }
    return (0);
}

//--------------------------------------------------------------------------------------------
// Beginnt den nächsten Clip, selbst wenn es verboten ist:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::ForceNextClip() {
    Clips[ActiveClip].Stop();
    Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
    NextClip();
}

void CSmackerPerson::CopyFrameSmk() {
    CalculatePalettemapper(smk_get_palette(Clips[ActiveClip].pSmack), Clips[ActiveClip].PaletteMapper);
    Bitmap.ReSize(XY(Clips[ActiveClip].Width, Clips[ActiveClip].Height), CREATE_INDEXED);
    SDL_SetSurfacePalette(Bitmap.pBitmap->GetSurface(), Clips[ActiveClip].PaletteMapper);
    {
        SB_CBitmapKey key(*Bitmap.pBitmap);
        memcpy(key.Bitmap, smk_get_video(Clips[ActiveClip].pSmack), key.lPitch * Clips[ActiveClip].Height);
    }
    BitmapPos = Clips[ActiveClip].ScreenOffset;
}

void CSmackerPerson::DecodeFrameSmk() {
    DOUBLE usf = NAN;
    if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
        smk_first(Clips[ActiveClip].pSmack);
    } else {
        smk_next(Clips[ActiveClip].pSmack);
    }
    smk_info_all(Clips[ActiveClip].pSmack, &Clips[ActiveClip].FrameNum, &Clips[ActiveClip].Frames, &usf);
    Clips[ActiveClip].FrameNext = AtGetTime() + (usf / 1000.0);
}

void CSmackerPerson::CopyFrameFlc() {
    CalculatePalettemapperFlc(Clips[ActiveClip].pFlc->get_palette(), Clips[ActiveClip].PaletteMapper);
    Bitmap.ReSize(XY(Clips[ActiveClip].Width, Clips[ActiveClip].Height), CREATE_INDEXED);
    SDL_SetSurfacePalette(Bitmap.pBitmap->GetSurface(), Clips[ActiveClip].PaletteMapper);
    {
        SB_CBitmapKey key(*Bitmap.pBitmap);
        memcpy(key.Bitmap, Clips[ActiveClip].pFlc->get_video(), key.lPitch * Clips[ActiveClip].Height);
    }
    BitmapPos = Clips[ActiveClip].ScreenOffset;
}

void CSmackerPerson::DecodeFrameFlc() {
    if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
        Clips[ActiveClip].pFlc->first();
    } else {
        Clips[ActiveClip].pFlc->next();
    }
    Clips[ActiveClip].pFlc->info_all(Clips[ActiveClip].FrameNum, Clips[ActiveClip].Frames);
    Clips[ActiveClip].FrameNext = AtGetTime() + Clips[ActiveClip].Speed;
}

void CSmackerPerson::CopyFrame() {
    if (ActiveClip < 0 || ActiveClip >= Clips.AnzEntries()) {
        return;
    }
    if (Clips[ActiveClip].pSmack != nullptr) {
        CopyFrameSmk();
    } else if (Clips[ActiveClip].pFlc != nullptr) {
        CopyFrameFlc();
    }
}

void CSmackerPerson::DecodeFrame() {
    if (ActiveClip < 0 || ActiveClip >= Clips.AnzEntries()) {
        return;
    }
    if (Clips[ActiveClip].pSmack != nullptr) {
        DecodeFrameSmk();
    } else if (Clips[ActiveClip].pFlc != nullptr) {
        DecodeFrameFlc();
    }
}

//--------------------------------------------------------------------------------------------
// Aktualisiert eine Animation
//--------------------------------------------------------------------------------------------
void CSmackerPerson::Pump() {
    if (Clips.AnzEntries() == 0) {
        return;
    }

    if (ActiveClip != -1 && (Clips[ActiveClip].IsFXPlaying != 0) && Clips[ActiveClip].NumSoundFx > 1 && AtGetTime() > Clips[ActiveClip].TickerNext) {
        Clips[ActiveClip].NextSyllable();
        Clips[ActiveClip].PlaySyllable();
    }

    // Wenn gerade ein Leerclip abgespielt wird:
    if (isEmptyClip(ActiveClip)) {
        Clips[ActiveClip].Stop();
        Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
        NextClip();

        CopyFrame();
        if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
            Clips[ActiveClip].PlaySyllable();
        }
        DecodeFrame();

        return;
    }

    if (ActiveClip == -1) {
        ActiveClip = 0;
        if (CurrentMood == -1) {
            CurrentMood = Clips[ActiveClip].MoodId;
        }
        if (DesiredMood == -1) {
            DesiredMood = Clips[ActiveClip].MoodId;
        }

        Clips[ActiveClip].State = SMACKER_CLIP_PLAYING;
        Clips[ActiveClip].Start();

        CopyFrame();
        if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
            Clips[ActiveClip].PlaySyllable();
        }
        DecodeFrame();
    }

    if ((Clips[ActiveClip].CanCancelClip != 0) &&
        (CurrentMood != DesiredMood || ((Clips[ActiveClip].DecisionVar != nullptr) && Clips[ActiveClip].DecisionVar[0] != -1))) {
        Clips[ActiveClip].Stop();
        Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
        NextClip();

        return;
    }

    if (Clips[ActiveClip].State == SMACKER_CLIP_PLAYING) {
        if (AtGetTime() >= Clips[ActiveClip].FrameNext) {
            CopyFrame();

            // Variablenveränderung, während der Film läuft?
            if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_FRAME) != 0)) // Variablen-Messageing:
            {
                if (Clips[ActiveClip].FrameNum >= ULONG(Clips[ActiveClip].PostOperation >> 13) &&
                    Clips[ActiveClip].LastFrame < (Clips[ActiveClip].PostOperation >> 13)) {

                    switch (Clips[ActiveClip].PostOperation & 1023) {
                    case SMACKER_CLIP_ADD:
                        Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_SUB:
                        Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_SET:
                        Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
                        break;
                    case SMACKER_CLIP_XOR:
                        Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
                        break;
                    default:
                        break;
                    }
                }
            }

            Clips[ActiveClip].LastFrame = Clips[ActiveClip].FrameNum;

            if (Clips[ActiveClip].FrameNum >= Clips[ActiveClip].Frames - 1) {
                Clips[ActiveClip].RepeatCount--;

                if (CurrentMood != DesiredMood) {
                    Clips[ActiveClip].RepeatCount = 0;
                }

                if (Clips[ActiveClip].RepeatCount > 0) {
                    if (CurrentMood != SPM_TALKING && CurrentMood != SPM_ANGRY_TALKING) {
                        Clips[ActiveClip].PlaySyllable();
                    }
                }
            }

            if (Clips[ActiveClip].RepeatCount == 0) {
                // End of Animation...
                if (Clips[ActiveClip].PostWait.y > 0) {
                    Clips[ActiveClip].WaitCount =
                        (Clips[ActiveClip].PostWait.x + rand() % (Clips[ActiveClip].PostWait.y - Clips[ActiveClip].PostWait.x + 1)) * 50 + AtGetTime();
                    Clips[ActiveClip].State = SMACKER_CLIP_WAITING;
                } else {
                    if (CurrentMood != DesiredMood || (CurrentMood != SPM_TALKING && CurrentMood != SPM_ANGRY_TALKING)) {
                        Clips[ActiveClip].SoundFx.Stop();
                        Clips[ActiveClip].IsFXPlaying = FALSE;
                    }

                    Clips[ActiveClip].Stop();
                    Clips[ActiveClip].State = SMACKER_CLIP_INACTIVE;
                    NextClip();
                }
            } else {
                if (Clips[ActiveClip].FrameNum == 0 && (Clips[ActiveClip].IsFXPlaying == 0)) {
                    Clips[ActiveClip].PlaySyllable();
                }

                DecodeFrame();
            }
        }
    } else if (Clips[ActiveClip].State == SMACKER_CLIP_WAITING) {
        if (SLONG(AtGetTime()) > Clips[ActiveClip].WaitCount) {
            Clips[ActiveClip].Stop();
            NextClip();
        }
    }
}

//--------------------------------------------------------------------------------------------
// Sucht den Folgeclip heraus:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::NextClip() {
    SLONG c = 0;
    SLONG PropSum = 0;

    if (Clips.AnzEntries() == 0) {
        return;
    }

    if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_POST) != 0)) // Variablen-Messageing:
    {
        switch (Clips[ActiveClip].PostOperation & 1023) {
        case SMACKER_CLIP_ADD:
            Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SUB:
            Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SET:
            Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_XOR:
            Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
            break;
        default:
            break;
        }
    }

    // Erste Priorität hat immer eine "DecisionVar"
    if ((Clips[ActiveClip].DecisionVar != nullptr) && Clips[ActiveClip].DecisionVar[0] != -1) {
        ActiveClip = Clips[ActiveClip].DecisionVar[0];
        goto found_next_clip;
    } else // Ansonsten entscheiden wir anhand von Zufall und Regeln
    {
        if (CurrentMood == DesiredMood) {
            // 1. Schauen, wie's weitergeht ohen die Stimmung zu ändern:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 2. Unmöglich, rettet bei Bugs vor dem Absturz:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
            }

            PropSum = rand() % PropSum;

            // Alternative auswählten:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                if (PropSum < 0) {
                    ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                    goto found_next_clip;
                }
            }
        } else // Die Stimmung wechseln:
        {
            // 1. Probieren, ob es Exits in die richtige Richtung gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 2. Probieren, ob es Exits in andere, akzeptierte Richtungen gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                     Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                    Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                        Clips[ActiveClip].SuccessorTokens[c * 2] == 'E') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 3. Probieren, ob es eine direkte Verbindung zur ZielMood gibt:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if (Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood && Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 4. Der Rest:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                     Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                    Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                    PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                }
            }

            if (PropSum != 0) {
                PropSum = rand() % PropSum;

                // Alternative auswählten:
                for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                    if ((Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == DesiredMood || Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == CurrentMood ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood1 ||
                         Clips[Clips[ActiveClip].SuccessorIds[c]].MoodId == AcceptedMood2) &&
                        Clips[ActiveClip].SuccessorTokens[c * 2] != 'S') {
                        PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                        if (PropSum < 0) {
                            ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                            goto found_next_clip;
                        }
                    }
                }
            }

            // 5. Unmöglich, rettet bei Bugs vor dem Absturz:
            //---------------------------------------------------------------------------
            PropSum = 0; // Wahrscheinlichkeiten aufsummieren:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum += (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
            }

            PropSum = rand() % PropSum;

            // Alternative auswählten:
            for (c = 0; c < Clips[ActiveClip].SuccessorIds.AnzEntries(); c++) {
                PropSum -= (Clips[ActiveClip].SuccessorTokens[c * 2 + 1] - '0');
                if (PropSum < 0) {
                    ActiveClip = Clips[ActiveClip].SuccessorIds[c];
                    goto found_next_clip;
                }
            }
        }
    }
    DebugBreak();

found_next_clip:
    BOOL LastTalking = static_cast<BOOL>(CurrentMood == SPM_TALKING || CurrentMood == SPM_ANGRY_TALKING);
    BOOL Talking = static_cast<BOOL>(Clips[ActiveClip].MoodId == SPM_TALKING || Clips[ActiveClip].MoodId == SPM_ANGRY_TALKING);

    if ((Talking != 0) && (LastTalking == 0) && ((Sim.Options.OptionTalking * Sim.Options.OptionDigiSound) != 0)) {
        SpeakFx.Play(DSBPLAY_NOSTOP | DSBPLAY_LOOPING, Sim.Options.OptionTalking * 100 / 7);
    } else if ((Talking == 0) && (LastTalking != 0)) {
        SpeakFx.Stop();
    }

    CurrentMood = Clips[ActiveClip].MoodId;
    Clips[ActiveClip].State = SMACKER_CLIP_PLAYING;
    Clips[ActiveClip].Start();

    if ((Clips[ActiveClip].PostVar != nullptr) && ((Clips[ActiveClip].PostOperation & SMACKER_CLIP_PRE) != 0)) // Variablen-Messageing:
    {
        switch (Clips[ActiveClip].PostOperation & 1023) {
        case SMACKER_CLIP_ADD:
            Clips[ActiveClip].PostVar[0] += Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SUB:
            Clips[ActiveClip].PostVar[0] -= Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_SET:
            Clips[ActiveClip].PostVar[0] = Clips[ActiveClip].PostValue;
            break;
        case SMACKER_CLIP_XOR:
            Clips[ActiveClip].PostVar[0] ^= Clips[ActiveClip].PostValue;
            break;
        default:
            break;
        }
    }

    if (isEmptyClip(ActiveClip) && Clips[ActiveClip].PostWait.x == 0 && Clips[ActiveClip].PostWait.y == 0) {
        NextClip();
    }
}

//--------------------------------------------------------------------------------------------
// Blittet eine Person in eine Bitmap:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::BlitAtT(SBBM &RoomBm, XY Offset) {
    if (clipExists(ActiveClip)) {
        RoomBm.BlitFromT(Bitmap, BitmapPos + Offset);
    }
}

//--------------------------------------------------------------------------------------------
// Blittet eine Person in eine Bitmap:
//--------------------------------------------------------------------------------------------
void CSmackerPerson::BlitAtT(SBBM &RoomBm) { BlitAtT(RoomBm, XY(0, 0)); }
