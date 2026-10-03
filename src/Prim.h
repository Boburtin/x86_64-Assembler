#ifndef PRIM_H
#define PRIM_H

#include <limits>
#include <string>

constexpr std::uint32_t U32_MAX = (std::numeric_limits<std::uint32_t>::max)();
constexpr std::uint32_t K_MAX_EXPAND = 1U << 12;
constexpr std::size_t K_CACHE_LINE = 64ULL;

#if defined(_WIN32)
#include <malloc.h>
#define ALIGNED_ALLOC(align, size) _aligned_malloc((size), (align))
#define ALIGNED_REALLOC(p, newSz, align) _aligned_realloc((p), (newSz), (align))
#define ALIGNED_FREE(p) _aligned_free(p)
#else
#include <cstdlib>
#include <cstring>
#define ALIGNED_ALLOC(align, size) std::aligned_alloc((align), (size))
#define ALIGNED_FREE(p) std::free(p)
#endif

#if defined(__cpp_lib_start_lifetime_as)
#include <memory>
#define HAVE_START_LIFETIME_AS 1
#else
#define HAVE_START_LIFETIME_AS 0
#endif

#if defined(__cpp_lib_flat_map)
#include <flat_map>
template <class K, class V> using T_Map = std::flat_map<K, V>;
#else
#include <unordered_map>
template <class K, class V> using T_Map = std::unordered_map<K, V>;
#endif

#if __has_cpp_attribute(assume)
#define ASSUME(expr) [[assume(expr)]]
#else
#define ASSUME(expr) ((void)0)
#endif

template <class T> inline T *alloc_array(void *p, std::uint32_t n)
{
#if HAVE_START_LIFETIME_AS
    return std::start_lifetime_as_array<T>(p, n);
#else
    return static_cast<T *>(p);
#endif
}

inline void *aligned_grow(void *old_p, std::size_t oldBytes, std::size_t newBytes, std::size_t align)
{
#if defined(_WIN32)
    return ALIGNED_REALLOC(old_p, newBytes, align);
#else
    void *p = ALIGNED_ALLOC(align, newBytes);
    if (old_p)
    {
        std::memcpy(p, old_p, oldBytes < newBytes ? oldBytes : newBytes);
        ALIGNED_FREE(old_p);
    }
    return p;
#endif
}

inline std::size_t aligned_size(std::size_t n)
{
    return (n + K_CACHE_LINE - 1) & ~(K_CACHE_LINE - 1);
}

enum class TokKind : std::uint8_t
{
    String,
    Symbol,
    Punct,
    Eof,
    Endl,
};
enum class DefKind : std::uint8_t
{
    Equ,
    Define,
    Fix
};
enum class MacroKind : std::uint8_t
{
    Macro,
    Struc
};
enum class ParamMode : std::uint8_t
{
    Plain,
    Greedy,
    Group
};

struct TokArray
{
    std::uint32_t size{};
    std::uint32_t cap{};

    TokKind *kind{};

    std::uint32_t *start{};
    std::uint32_t *end{};

    TokArray() = default;
    explicit TokArray(std::uint32_t n)
    {
        reserve(n);
    }

    TokArray(const TokArray &) = delete;
    TokArray &operator=(const TokArray &) = delete;

    // TODO: rvalue assignment operator and move constructor

    ~TokArray()
    {
        ALIGNED_FREE(kind);
        ALIGNED_FREE(start);
        ALIGNED_FREE(end);
    }

    void reserve(std::uint32_t n)
    {
        kind = alloc_array<TokKind>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(TokKind))), n);

        start = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        end = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        cap = n;
    }

    void grow()
    {
        std::uint32_t n = cap ? cap * 2 : 64;

        kind = alloc_array<TokKind>(
            aligned_grow(kind, cap * sizeof(TokKind), aligned_size(n * sizeof(TokKind)), K_CACHE_LINE), n);

        start = alloc_array<std::uint32_t>(
            aligned_grow(start, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE), n);

        end = alloc_array<std::uint32_t>(
            aligned_grow(end, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE), n);

        cap = n;
    }

    std::uint32_t push(TokKind k, std::uint32_t s, std::uint32_t e)
    {
        ASSUME(size <= cap);

        if (size == cap)
        {
            grow();
        }

        kind[size] = k;

        start[size] = s;

        end[size] = e;

        return size++;
    }

    std::uint32_t diff(std::uint32_t n) const noexcept
    {
        return (end[n] - start[n]);
    }
};

struct DefTable
{
    std::uint32_t size{}, cap{};
    DefKind *kind{};
    std::uint32_t *valBeg{};
    std::uint32_t *valEnd{};
    std::uint32_t *lock{};
    std::uint32_t *prev{};
    TokArray val;
    T_Map<std::string, std::uint32_t> byName;

    DefTable() = default;
    explicit DefTable(std::uint32_t n)
    {
        reserve(n);
    }
    DefTable(const DefTable &) = delete;
    DefTable &operator=(const DefTable &) = delete;
    ~DefTable()
    {
        ALIGNED_FREE(kind);
        ALIGNED_FREE(valBeg);
        ALIGNED_FREE(valEnd);
        ALIGNED_FREE(lock);
        ALIGNED_FREE(prev);
    }
    void reserve(std::uint32_t n)
    {
        kind = alloc_array<DefKind>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(DefKind))), n);

        valBeg = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        valEnd = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        lock = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        prev = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        cap = n;
    }
    void grow()
    {
        std::uint32_t n = cap ? cap * 2 : 32;

        kind = alloc_array<DefKind>(
            aligned_grow(kind, cap * sizeof(DefKind), aligned_size(n * sizeof(DefKind)), K_CACHE_LINE), n);

        valBeg = alloc_array<std::uint32_t>(
            aligned_grow(valBeg, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        valEnd = alloc_array<std::uint32_t>(
            aligned_grow(valEnd, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        lock = alloc_array<std::uint32_t>(
            aligned_grow(lock, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE), n);

        prev = alloc_array<std::uint32_t>(
            aligned_grow(prev, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE), n);

        cap = n;
    }
    std::uint32_t add(DefKind k, std::uint32_t vb, std::uint32_t ve, std::uint32_t pr = U32_MAX)
    {
        ASSUME(size <= cap);

        if (size == cap)
        {
            grow();
        }

        kind[size] = k;

        valBeg[size] = vb;

        valEnd[size] = ve;

        lock[size] = 0;

        prev[size] = pr;

        return size++;
    }
};

struct ParamStore
{
    std::uint32_t size{};
    std::uint32_t cap{};

    std::uint32_t *nameOff{};
    std::uint32_t *nameLen{};

    ParamMode *mode{};

    std::uint32_t *defltBeg{};
    std::uint32_t *defltEnd{};

    ParamStore() = default;
    explicit ParamStore(std::uint32_t n)
    {
        reserve(n);
    }

    ParamStore(const ParamStore &) = delete;
    ParamStore &operator=(const ParamStore &) = delete;

    ~ParamStore()
    {
        ALIGNED_FREE(nameOff);
        ALIGNED_FREE(nameLen);
        ALIGNED_FREE(mode);
        ALIGNED_FREE(defltBeg);
        ALIGNED_FREE(defltEnd);
    }

    void reserve(std::uint32_t n)
    {
        nameOff = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        nameLen = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        mode = alloc_array<ParamMode>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(ParamMode))), n);

        defltBeg = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        defltEnd = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        cap = n;
    }

    void grow()
    {
        std::uint32_t n = cap ? cap * 2 : 32;

        nameOff = alloc_array<std::uint32_t>(
            aligned_grow(nameOff, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        nameLen = alloc_array<std::uint32_t>(
            aligned_grow(nameLen, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        mode = alloc_array<ParamMode>(
            aligned_grow(mode, cap * sizeof(ParamMode), aligned_size(n * sizeof(ParamMode)), K_CACHE_LINE), n);

        defltBeg = alloc_array<std::uint32_t>(
            aligned_grow(defltBeg, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        defltEnd = alloc_array<std::uint32_t>(
            aligned_grow(defltEnd, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        cap = n;
    }
    std::uint32_t push(std::uint32_t no, std::uint32_t nl, ParamMode m, std::uint32_t db = U32_MAX,
                       std::uint32_t de = U32_MAX)
    {
        ASSUME(size <= cap);

        if (size == cap)
        {
            grow();
        }
        nameOff[size] = no;

        nameLen[size] = nl;

        mode[size] = m;

        defltBeg[size] = db;

        defltEnd[size] = de;

        return size++;
    }
};

struct MacroTable
{
    std::uint32_t size{};
    std::uint32_t cap{};

    MacroKind *kind{};

    std::uint32_t *paramBeg{};
    std::uint32_t *paramEnd{};

    std::uint32_t *bodyBeg{};
    std::uint32_t *bodyEnd{};

    ParamStore params;

    TokArray body;

    T_Map<std::string, std::uint32_t> byName;

    MacroTable() = default;

    explicit MacroTable(std::uint32_t n)
    {
        reserve(n);
    }

    MacroTable(const MacroTable &) = delete;
    MacroTable &operator=(const MacroTable &) = delete;

    ~MacroTable()
    {
        ALIGNED_FREE(kind);
        ALIGNED_FREE(paramBeg);
        ALIGNED_FREE(paramEnd);
        ALIGNED_FREE(bodyBeg);
        ALIGNED_FREE(bodyEnd);
    }
    void reserve(std::uint32_t n)
    {
        kind = alloc_array<MacroKind>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(MacroKind))), n);

        paramBeg = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        paramEnd = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        bodyBeg = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        bodyEnd = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        cap = n;
    }
    void grow()
    {
        std::uint32_t n = cap ? cap * 2 : 16;

        kind = alloc_array<MacroKind>(
            aligned_grow(kind, cap * sizeof(MacroKind), aligned_size(n * sizeof(MacroKind)), K_CACHE_LINE), n);

        paramBeg = alloc_array<std::uint32_t>(
            aligned_grow(paramBeg, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        paramEnd = alloc_array<std::uint32_t>(
            aligned_grow(paramEnd, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        bodyBeg = alloc_array<std::uint32_t>(
            aligned_grow(bodyBeg, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        bodyEnd = alloc_array<std::uint32_t>(
            aligned_grow(bodyEnd, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        cap = n;
    }
    std::uint32_t add(MacroKind k, std::uint32_t pb, std::uint32_t pe, std::uint32_t bb, std::uint32_t be)
    {
        ASSUME(size <= cap);

        if (size == cap)
        {
            grow();
        }
        kind[size] = k;

        paramBeg[size] = pb;

        paramEnd[size] = pe;

        bodyBeg[size] = bb;

        bodyEnd[size] = be;

        return size++;
    }
};

struct OriginArray
{
    std::uint32_t size{};
    std::uint32_t cap{};

    std::uint32_t *srcTok{};
    std::uint32_t *depth{};

    OriginArray() = default;

    OriginArray(const OriginArray &) = delete;
    OriginArray &operator=(const OriginArray &) = delete;

    ~OriginArray()
    {
        ALIGNED_FREE(srcTok);
        ALIGNED_FREE(depth);
    }

    void reserve(std::uint32_t n)
    {
        srcTok = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        depth = alloc_array<std::uint32_t>(ALIGNED_ALLOC(K_CACHE_LINE, aligned_size(n * sizeof(std::uint32_t))), n);

        cap = n;
    }

    void grow()
    {
        std::uint32_t n = cap ? cap * 2 : 256;

        srcTok = alloc_array<std::uint32_t>(
            aligned_grow(srcTok, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE),
            n);

        depth = alloc_array<std::uint32_t>(
            aligned_grow(depth, cap * sizeof(std::uint32_t), aligned_size(n * sizeof(std::uint32_t)), K_CACHE_LINE), n);

        cap = n;
    }

    void push(std::uint32_t s, std::uint32_t d)
    {
        ASSUME(size <= cap);

        if (size == cap)
        {
            grow();
        }

        srcTok[size] = s;

        depth[size] = d;

        size++;
    }
};

struct TextArena
{
    std::string bytes;

    std::uint32_t put(const char *p, std::uint32_t n)
    {
        auto off = bytes.size();

        bytes.append(p, n);

        return off;
    }

    const char *data() const
    {
        return bytes.data();
    }

    std::uint32_t size() const noexcept
    {
        return static_cast<std::uint32_t>(bytes.size());
    }

    std::uint32_t length() const noexcept
    {
        return size();
    }
};

#endif
