#ifndef COMPAT_ALBUM_H_
#define COMPAT_ALBUM_H_

#include "defines.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

//--------------------------------------------------------------------------------------------
// Some other basic functions
//--------------------------------------------------------------------------------------------

using std::strlen;
class CString : public std::string {
  public:
    CString() = default;
    CString(const char *str) : std::string(str) {}
    operator const char *() const { return c_str(); }
    SLONG GetLength() const { return length(); }
};

inline char *bprintf(char const *format, ...) {
    static char buffer[8192];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

inline DWORD AtGetTime() {
    std::chrono::nanoseconds now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

/* TEXTRES for weekdays only */
#define TOKEN_SCHED 0
class RES {
  public:
    CString GetS(SLONG /*token*/, SLONG idx) const {
        switch (idx) {
        case 3010:
            return "Montag";
        case 3011:
            return "Dienstag";
        case 3012:
            return "Mittwoch";
        case 3013:
            return "Donnerstag";
        case 3014:
            return "Freitag";
        case 3015:
            return "Samstag";
        case 3016:
            return "Sonntag";
        default:
            return "Fehler";
        }
    }
};

inline char *TeakStrRemoveEndingCodes(char *str, char const *codes) {
    SLONG i = 0;
    for (i = strlen(str) - 1; i >= 0 && (strchr(codes, str[i]) != nullptr); --i) {
        ;
    }
    str[i + 1] = 0;
    return str;
}

//--------------------------------------------------------------------------------------------
// TeakLibW.h
//--------------------------------------------------------------------------------------------

extern const char *ExcAssert;
extern const char *ExcGuardian;
extern const char *ExcImpossible;
extern const char *ExcNotImplemented;
extern const char *ExcOutOfMem;
extern const char *ExcStrangeMem;

#define FNL 0, 0

extern const char *ExcAlbumInsert;
extern const char *ExcAlbumFind;
extern const char *ExcAlbumDelete;
extern const char *ExcXIDUnrecoverable;
extern const char *ExcAlbumNotConsistent;
extern const char *ExcAlbumInvalidArg;

extern SLONG TeakLibW_Exception(char *, SLONG, const char *, ...);

template <typename T> inline void Limit(T min, T &value, T max) {
    if (value < min)
        value = min;
    if (value > max)
        value = max;
}

template <typename T> inline void Swap(T &a, T &b) {
    T c;
    c = a;
    a = b;
    b = c;
}

template <typename T> inline const T &Min(const T &a, const T &b) { return (b < a) ? b : a; }

template <typename T> inline const T &Max(const T &a, const T &b) { return (a < b) ? b : a; }

template <typename A, typename B> inline A min(const A &a, const B &b) { return (b < a) ? b : a; }

template <typename A, typename B> inline A max(const A &a, const B &b) { return (a < b) ? b : a; }

template <typename T> class BUFFER_V : public std::vector<T> {
  public:
    BUFFER_V() = default;
    BUFFER_V(SLONG size) : std::vector<T>(size) {}
    void ReSize(SLONG anz) {
        Offset = 0;
        std::vector<T>::resize(anz);
    }
    SLONG AnzEntries() const { return std::vector<T>::size(); }
    void Clear() {
        Offset = 0;
        std::vector<T>::clear();
    }
    void FillWith(T value) {
        for (SLONG i = 0; i < std::vector<T>::size(); i++)
            std::vector<T>::at(i) = value;
    }

    // operator T *() const { return std::vector<T>::data(); }
    const T *getData() const { return std::vector<T>::data() + Offset; }
    T *getData() { return std::vector<T>::data() + Offset; }

    // void operator+=(SLONG rhs) { DelPointer += rhs; }
    void incIter(SLONG i) { Offset += i; }
    SLONG getIter() const { return Offset; }

#ifdef DEBUG_ALBUM
    T &operator[](size_t pos) { return std::vector<T>::at(pos); }
    const T &operator[](size_t pos) const { return std::vector<T>::at(pos); };
#endif

    // keep using function names of old BUFFER class for compatibility, but
    // make sure that std::vector<> is not directly accessed
    void clear() { Clear(); }
    void resize(std::size_t count) { ReSize(count); }
    void resize(std::size_t count, const T &value) {
        ReSize(count);
        FillWith(value);
    }

  private:
    SLONG Offset{0};
};

class TEAKRAND {
  public:
    TEAKRAND(void) { Seed = 0; }
    TEAKRAND(ULONG _Seed) {
        Seed = _Seed;
        mMT.seed(Seed);
    }

    void SRand(ULONG _Seed) {
        Seed = _Seed;
        mMT.seed(Seed);
    }
    void SRandTime(void) {
        Seed = AtGetTime();
        mMT.seed(Seed);
    }
    void Reset(void) { mMT.seed(Seed); }

    UWORD Rand(void) { return getRandInt(0, UINT16_MAX); }
    UWORD Rand(SLONG Max) { return getRandInt(0, Max - 1); }
    UWORD Rand(SLONG Min, SLONG Max) { return getRandInt(Min, Max); }
    ULONG GetSeed(void) { return (Seed); }

    inline int getRandInt(int min, int max) {
        assert(max >= min);
        std::uniform_int_distribution<int> dist(min, max);
        return dist(mMT);
    }

  private:
    ULONG Seed{};

    std::mt19937 mMT{std::random_device{}()};
    // std::mt19937 mMT{};
};

template <typename T> class TXYZ {
  public:
    T x, y, z;

    TXYZ() : x(), y(), z() {}
    TXYZ(T s) : x(s), y(s), z(s) {}
    TXYZ(T x, T y, T z) : x(x), y(y), z(z) {}

    TXYZ operator+(const TXYZ &b) const { return TXYZ<T>(x + b.x, y + b.y, z + b.z); }

    TXYZ operator-(const TXYZ &b) const { return TXYZ<T>(x - b.x, y - b.y, z - b.z); }

    TXYZ operator*(const TXYZ &b) const { return TXYZ<T>(x * b.x, y * b.y, z * b.z); }

    TXYZ operator/(const TXYZ &b) const { return TXYZ<T>(x / b.x, y / b.y, z / b.z); }

    TXYZ operator*(const T &b) const { return TXYZ<T>(x * b, y * b, z * b); }

    TXYZ operator/(const T &b) const { return TXYZ<T>(x / b, y / b, z / b); }

    TXYZ operator-() const { return TXYZ<T>(-x, -y, -z); }

    bool operator==(const TXYZ &b) const { return x == b.x && y == b.y && z == b.z; }

    bool operator!=(const TXYZ &b) const { return x != b.x || y != b.y || z != b.z; }

    bool operator<(const TXYZ &b) const { return x < b.x && y < b.y && z < b.z; }

    bool operator>(const TXYZ &b) const { return x > b.x && y > b.y && z > b.z; }

    TXYZ &operator-=(const TXYZ &b) {
        x -= b.x;
        y -= b.y;
        z -= b.z;
        return *this;
    }

    TXYZ &operator+=(const TXYZ &b) {
        x += b.x;
        y += b.y;
        z += b.z;
        return *this;
    }

    TXYZ &operator/=(const TXYZ &b) {
        x /= b.x;
        y /= b.y;
        z /= b.z;
        return *this;
    }

    TXYZ &operator*=(const TXYZ &b) {
        x *= b.x;
        y *= b.y;
        z *= b.z;
        return *this;
    }

    DOUBLE abs() const { return sqrt(x * x + y * y + z * z); }

    DOUBLE operator*(const DOUBLE &b) const { return (x + y + z) * b; }

    DOUBLE operator/(const DOUBLE &b) const { return (x + y + z) / b; }
};
typedef TXYZ<FLOAT> FXYZ;

template <typename T> class ALBUM_V {
  public:
    /* album iter */

    using element_type = std::pair<T, ULONG>;
    class Iter {
      public:
        using difference_type = SLONG;
        using value_type = T;
        using pointer = T *;
        using reference = T &;
        using iterator_category = std::random_access_iterator_tag;

        Iter(typename std::vector<element_type>::iterator it, std::unordered_map<ULONG, SLONG> *h) : It(it), Hash(h) {}
        inline Iter &operator++() {
            It++;
            return (*this);
        }
        inline Iter &operator--() {
            It--;
            return (*this);
        }
        inline Iter &operator+=(SLONG i) {
            It += i;
            return (*this);
        }
        inline Iter &operator-=(SLONG i) {
            It -= i;
            return (*this);
        }
        friend Iter operator+(Iter it, SLONG i) { return Iter(it.It + i, it.Hash); }
        friend Iter operator-(Iter it, SLONG i) { return Iter(it.It - i, it.Hash); }
        friend Iter operator+(SLONG i, Iter it) { return Iter(it.It + i, it.Hash); }
        friend Iter operator-(SLONG i, Iter it) { return Iter(it.It - i, it.Hash); }
        inline difference_type operator-(Iter it) const { return It - it.It; }
        inline reference operator*() const { return It->first; }
        inline bool operator==(const Iter &i) const { return It == i.It; }
        inline bool operator!=(const Iter &i) const { return It != i.It; }

        inline bool operator<(const Iter &i) const { return It < i.It; }
        inline bool operator<=(const Iter &i) const { return It <= i.It; }
        inline bool operator>(const Iter &i) const { return It > i.It; }
        inline bool operator>=(const Iter &i) const { return It >= i.It; }

        static void swap(Iter &a, Iter &b) {
            if (a == b) {
                return;
            }
            auto *h = a.Hash;
            if (h->end() != h->find(a.It->second)) {
                h->at(a.It->second) += (b.It - a.It);
            }
            if (h->end() != h->find(b.It->second)) {
                h->at(b.It->second) -= (b.It - a.It);
            }
            std::iter_swap(a.It, b.It);
        }

        bool IsInAlbum() const { return It->second != 0; }

      private:
        typename std::vector<element_type>::iterator It;
        std::unordered_map<ULONG, SLONG> *Hash;
    };
    Iter begin() { return Iter(List.begin(), &Hash); }
    Iter end() { return Iter(List.end(), &Hash); }

    /* constructor */

    ALBUM_V(CString str) : Name(str) {}

    /* query capacity and resize */

    SLONG AnzEntries() const { return List.size(); }
    SLONG GetNumFree() const {
        return std::count_if(List.begin(), List.end(), [](const element_type &i) { return 0 == i.second; });
    }
    SLONG GetNumUsed() const { return AnzEntries() - GetNumFree(); }

    void ReSize(SLONG anz) {
        for (auto i = anz; i < AnzEntries(); i++) {
            Hash.erase(List[i].second);
            List[i].second = 0;
        }
        List.resize(anz);
        IdxBack = AnzEntries() - 1;
    }

    void ClearAlbum() {
        for (auto &i : List) {
            i.second = 0;
        }
        Hash = {};
        IdxFront = 0;
        IdxBack = AnzEntries() - 1;
    }

    void FillAlbum() {
        for (auto i = 0; i < AnzEntries(); i++) {
            if (List[i].second == 0) {
                ULONG id = GetUniqueId();
                List[i].second = id;
                Hash[id] = i;
            }
        }
        IdxFront = AnzEntries();
        IdxBack = -1;
    }

    /* accessing elements */

    ULONG GetIdFromIndex(SLONG i) const { return List[i].second; }

    SLONG IsInAlbum(ULONG id) const {
        if (id >= 0x1000000) {
            return (Hash.end() != Hash.find(id));
        }
        if (id < AnzEntries() && (List[id].second != 0)) {
            return 1;
        }
        return 0;
    }

    SLONG operator()(ULONG id) const { return find(id); }
    SLONG find(ULONG id) const {
        if (id >= 0x1000000) {
            auto it = Hash.find(id);
            if (it != Hash.end()) {
                return it->second;
            }
        } else if (id < AnzEntries()) {
            return id;
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumFind, Name.c_str());
        return 0;
    }

#ifdef DEBUG_ALBUM
    T &operator[](ULONG id) { return List.at(find(id)).first; }
    const T &operator[](ULONG id) const { return List.at(find(id)).first; }
#else
    T &operator[](ULONG id) { return List[find(id)].first; }
    const T &operator[](ULONG id) const { return List[find(id)].first; }
#endif
    T &at(ULONG id) { return List.at(find(id)).first; }
    const T &at(ULONG id) const { return List.at(find(id)).first; }

    /* comparison */

    bool operator==(const ALBUM_V<T> &l) const { return (LastId == l.LastId) && (Name == l.Name) && (List == l.List) && (Hash == l.Hash); }
    bool operator!=(const ALBUM_V<T> &l) const { return !operator==(l); }

    bool operator==(const std::vector<T> &l) const {
        if (AnzEntries() != l.size()) {
            return false;
        }
        for (SLONG i = 0; i < AnzEntries(); ++i) {
            if (List[i].first != l[i]) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const std::vector<T> &l) const { return !operator==(l); }

    /* modifiers */

    void ResetNextId() { LastId = 0xFFFFFF; }

    ULONG push_front(ULONG id, T rhs) {
        if (id >= 0x1000000 && (Hash.end() == Hash.find(id))) {
            for (SLONG i = IdxFront; i < AnzEntries(); ++i) {
                if (List[i].second == 0) {
                    std::swap(List[i].first, rhs);
                    List[i].second = id;
                    Hash[id] = i;
                    IdxFront = i + 1;
                    return id;
                }
            }
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumInsert, Name.c_str());
        return 0;
    }

    ULONG push_back(ULONG id, T rhs) {
        if (id >= 0x1000000 && (Hash.end() == Hash.find(id))) {
            for (SLONG i = IdxBack; i >= 0; --i) {
                if (List[i].second == 0) {
                    std::swap(List[i].first, rhs);
                    List[i].second = id;
                    Hash[id] = i;
                    IdxBack = i - 1;
                    return id;
                }
            }
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumInsert, Name.c_str());
        return 0;
    }

    ULONG operator*=(T rhs) {
        auto id = GetUniqueId();
        return push_front(id, std::move(rhs));
    }

    ULONG operator+=(T rhs) {
        auto id = GetUniqueId();
        return push_back(id, std::move(rhs));
    }

    void operator-=(ULONG id) {
        if (id >= 0x1000000) {
            auto it = Hash.find(id);
            if (it != Hash.end()) {
                List[it->second].second = 0;
                IdxFront = std::min(IdxFront, it->second);
                IdxBack = std::max(IdxBack, it->second);
            }
            Hash.erase(id);
            return;
        }
        SLONG idx = id;
        if (idx < AnzEntries() && (List[idx].second != 0)) {
            Hash.erase(List[idx].second);
            List[idx].second = 0;
            IdxFront = std::min(IdxFront, idx);
            IdxBack = std::max(IdxBack, idx);
            return;
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumDelete, Name.c_str());
    }

    SLONG GetRandomUsedIndex(TEAKRAND *random = NULL) const {
        SLONG used = GetNumUsed();
        if (used == 0) {
            TeakLibW_Exception(nullptr, 0, ExcAlbumFind, Name.c_str());
        }

        SLONG target = (random != nullptr) ? random->Rand(used) : rand() % used;
        SLONG index = 0;
        for (SLONG i = AnzEntries() - 1; i >= 0; --i) {
            if (List[i].second == 0) {
                continue;
            }
            if (++index > target) {
                return List[i].second;
            }
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumFind, Name.c_str());
        return 0;
    }

    void Sort() {
        IdxFront = 0;
        IdxBack = AnzEntries() - 1;

        auto a = List.begin();
        auto b = List.end() - 1;
        while (true) {
            while ((a->second != 0) && a < b) {
                ++a;
            }
            while ((b->second == 0) && a < b) {
                --b;
            }
            if (a >= b) {
                break;
            }
            std::iter_swap(a, b);
        }
#ifdef DEBUG_ALBUM
        assert(a == b);
#endif
        if (a->second != 0) {
            ++a;
        }
        std::stable_sort(List.begin(), a);
        rebuild_hash_table();
    }

    void Swap(SLONG a, SLONG b) {
        if (a == b) {
            return;
        }
        IdxFront = 0;
        IdxBack = AnzEntries() - 1;
        if (a >= 0x1000000 && b >= 0x1000000) {
            auto idxA = find(a);
            auto idxB = find(b);
            Hash.at(a) = idxB;
            Hash.at(b) = idxA;
            std::swap(List[idxA], List[idxB]);
            return;
        }
        if (a < 0x1000000 && b < 0x1000000) {
            auto idA = List[a].second;
            auto idB = List[b].second;
            if (Hash.end() != Hash.find(idA)) {
                Hash.at(idA) = b;
            }
            if (Hash.end() != Hash.find(idB)) {
                Hash.at(idB) = a;
            }
            std::swap(List[a], List[b]);
            return;
        }
        TeakLibW_Exception(nullptr, 0, ExcAlbumInvalidArg, Name.c_str());
    }

    void check_consistent_index() {
        for (auto i = 0; i < AnzEntries(); i++) {
            auto id = GetIdFromIndex(i);
            if ((id != 0) && find(id) != i) {
                TeakLibW_Exception(nullptr, 0, ExcAlbumNotConsistent, Name.c_str());
            }
        }
    }

  private:
    void rebuild_hash_table() {
        Hash.clear();
        for (SLONG i = 0; i < AnzEntries(); ++i) {
            auto id = List[i].second;
            if (id >= 0x1000000) {
                Hash[id] = i;
            }
        }
    }

  private:
    SLONG GetUniqueId() { return ++LastId; }

    ULONG LastId{0xFFFFFF};
    SLONG IdxFront{};
    SLONG IdxBack{};
    std::vector<element_type> List;
    std::unordered_map<ULONG, SLONG> Hash;
    CString Name;
};

class CRLEReader {
  public:
    CRLEReader(const char *path) : Ctx(nullptr), SeqLength(0), SeqUsed(0), IsSeq(false), Sequence(), IsRLE(false), Size(0), Key(0), Path(path) {
        Ctx = fopen(path, "rb");
        if (Ctx != nullptr) {
            char str[6];
            fread(str, sizeof(str), 1, Ctx);
            if (strcmp(str, "xtRLE") == 0) {
                IsRLE = true;
                SLONG version = -1;
                fread(&version, sizeof(version), 1, Ctx);
                if (version >= 0x102) {
                    Key = 0xA5;
                }
                if (version >= 0x101) {
                    Size = -1;
                    fread(&Size, sizeof(Size), 1, Ctx);
                }
            } else {
                fseek(Ctx, 0, SEEK_END);
                Size = static_cast<SLONG>(ftell(Ctx));
                fseek(Ctx, 0, SEEK_SET);
            }
        }
    }
    ~CRLEReader(void) { Close(); }

    bool Close(void) {
        if (Ctx == nullptr) {
            return false;
        }
        return fclose(Ctx) == 0;
    }
    bool Buffer(void *buffer, SLONG size) { return fread(buffer, size, 1, Ctx) > 0; }
    bool NextSeq(void) {
        char buf = 0;
        if (!Buffer(&buf, 1)) {
            return false;
        }
        SeqLength = buf;

        if ((SeqLength & 0x80) != 0) {
            SeqLength &= 0x7FU;
            SeqUsed = 0;
            IsSeq = true;
            if (!Buffer(Sequence, SeqLength)) {
                return false;
            }
            for (SLONG i = 0; i < SeqLength; i++) {
                Sequence[i] ^= Key;
            }
        } else {
            IsSeq = false;
            if (!Buffer(Sequence, 1)) {
                return false;
            }
        }
        return true;
    }
    bool Read(BYTE *buffer, SLONG size, bool decode) {
        if (!decode || !IsRLE) {
            return Buffer(buffer, size);
        }

        for (SLONG i = 0; i < size; i++) {
            if ((SeqLength == 0) && !NextSeq()) {
                return false;
            }

            if (IsSeq) {
                buffer[i] = Sequence[SeqUsed++];
                if (SeqUsed == SeqLength) {
                    SeqLength = 0;
                }
            } else {
                buffer[i] = Sequence[0];
                SeqLength--;
            }
        }
        return true;
    }

    SLONG GetSize() { return Size; }

    bool getIsRLE() { return IsRLE; }
    void SaveAsPlainText() {}

  private:
    FILE *Ctx;
    int8_t SeqLength;
    int8_t SeqUsed;
    bool IsSeq;
    BYTE Sequence[132];

    bool IsRLE;
    SLONG Size;
    SLONG Key;

    const char *Path;
};

inline BOOL DoesFileExist(char const *path) {
    FILE *ctx = fopen(path, "rb");
    if (ctx != nullptr) {
        fclose(ctx);
        return 1;
    }
    return 0;
}

inline BUFFER_V<BYTE> LoadCompleteFile(char const *path) {
    CRLEReader reader(path);
    BUFFER_V<BYTE> buffer(reader.GetSize());
    if (!reader.Read(buffer.getData(), buffer.AnzEntries(), true)) {
        return buffer;
    }

    if (reader.getIsRLE()) {
        CRLEReader konverter(path);
        konverter.SaveAsPlainText();
    }

    return buffer;
}

#endif // COMPAT_ALBUM_H_
