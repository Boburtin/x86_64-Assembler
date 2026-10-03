#ifndef PREPROC_H
#define PREPROC_H

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "Prim.h"
#include "Source.h"

struct Preproc
{
    const Source &src;
    const TokArray &in;

    TextArena textArena;

    OriginArray origin;

    TokArray out;

    DefTable defs;

    MacroTable macros;

    std::uint64_t localCtr{};
    std::uint32_t depth{};
    std::uint32_t i{};

    Preproc(const Source &s, const TokArray &t) : src(s), in(t)
    {
        out.reserve(t.size);
        origin.reserve(t.size);
        textArena.bytes.reserve(s.size());
    }

    static bool ieq(std::string_view a, std::string_view b)
    {
        bool isEqual = true;

        if (a.size() != b.size())
        {
            isEqual = false;
        }
        else
        {
            for (auto j = b.size(); j-- > 0;)
            {
                if ((a[j] | 0x20) != b[j])
                {
                    isEqual = false;
                    break;
                }
            }
        }
        return isEqual;
    }

    static std::string fold(std::string_view s)
    {
        std::string r(s);

        for (char &c : r)
        {
            c |= 0x20;
        }
        return r;
    }

    std::string_view view(std::uint32_t j) const
    {
        return {src.data() + in.start[j], in.end[j] - in.start[j]};
    }

    std::uint32_t skipEndl(std::uint32_t e) const
    {
        return ((e < in.size) && (in.kind[e] == TokKind::Endl)) ? (e + 1) : e;
    }

    void emitRaw(std::uint32_t j)
    {
        auto t = view(j);

        std::uint32_t off = textArena.put(t.data(), (std::uint32_t)t.size());

        out.push(in.kind[j], off, off + (std::uint32_t)t.size());

        origin.push(j, depth);
    }

    void emitText(TokKind k, std::string_view t)
    {
        std::uint32_t off = textArena.put(t.data(), (std::uint32_t)t.size());

        out.push(k, off, off + static_cast<std::uint32_t>(t.size()));

        origin.push(U32_MAX, depth);
    }

    void emitStored(TokKind k, std::uint32_t off, std::uint32_t len)
    {
        out.push(k, off, off + len);
        origin.push(U32_MAX, depth);
    }

    void emitExpanded(std::uint32_t j)
    {
        if (in.kind[j] != TokKind::Symbol)
        {
            emitRaw(j);
            return;
        }

        auto it = defs.byName.find(fold(view(j)));
        if (it == defs.byName.end() || defs.lock[it->second])
        {
            emitRaw(j);
            return;
        }

        std::uint32_t id = it->second;

        defs.lock[id]++;

        for (std::uint32_t n = defs.valBeg[id]; n < defs.valEnd[id]; ++n)
        {
            emitStored(defs.val.kind[n], defs.val.start[n], defs.val.end[n] - defs.val.start[n]);
        }
        defs.lock[id]--;
    }

    std::uint32_t lineEnd(std::uint32_t b) const
    {
        for (auto j = b;; j++)
        {
            auto k = in.kind[j];
            if (k == TokKind::Eof)
            {
                return j;
            }
            if (k == TokKind::Endl)
            {
                /*TODO: find out if only checking for view == backslash would be optimal */
                if (!(j > b && in.kind[j - 1] == TokKind::Symbol && view(j - 1) == "\\"))
                {
                    return j;
                }
            }
        }
    }

    void defineEqu(const std::string &name, std::uint32_t b, std::uint32_t e)
    {
        std::uint32_t vb = defs.val.size;

        for (std::uint32_t j = b; j < e; ++j)
        {
            if (auto it = defs.byName.find(fold(view(j))); it != defs.byName.end() && defs.lock[it->second] == 0)
            {
                std::uint32_t s = it->second;
                for (auto n = defs.valBeg[s]; n < defs.valEnd[s]; ++n)
                {
                    defs.val.push(defs.val.kind[n], defs.val.start[n], defs.val.end[n]);
                }
            }
            else
            {
                auto t = view(j);
                std::uint32_t off = textArena.put(t.data(), static_cast<std::uint32_t>(t.size()));
                defs.val.push(in.kind[j], off, off + static_cast<std::uint32_t>(t.size()));
            }
        }

        std::uint32_t ve = defs.val.size;

        std::uint32_t prevId = U32_MAX;

        if (auto it = defs.byName.find(name); it != defs.byName.end())
        {
            prevId = it->second;
        }

        defs.byName[name] = defs.add(DefKind::Equ, vb, ve, prevId);

        i = skipEndl(e);
    }

    void doRestore(std::uint32_t b, std::uint32_t e)
    {
        for (std::uint32_t j = b; j < e; ++j)
        {
            if (in.kind[j] == TokKind::Punct && view(j) == ",")
            {
                continue;
            }
            std::string name = fold(view(j));
            //
            // auto it = defs.byName.find(name);
            // if (it == defs.byName.end())
            // {
            //     continue;
            // }
            // if (auto id = it->second; defs.prev[id] != U32_MAX)
            // {
            //     defs.byName[name] = defs.prev[id];
            // }
            // else
            // {
            //     defs.byName.erase(it);
            // }
            //
            if (auto it = defs.byName.find(name); it != defs.byName.end())
            {
                if (auto id = it->second; defs.prev[id] != U32_MAX)
                {
                    defs.byName[name] = defs.prev[id];
                }
                else
                {
                    defs.byName.erase(it);
                }
            }
        }
        i = skipEndl(e);
    }

    void captureMacro(std::uint32_t b)
    {
        std::string name = fold(view(b + 1));
        std::uint32_t j = b + 2;
        std::uint32_t pb = macros.params.size;

        for (;; ++j)
        {
            auto ckind = in.kind[j];
            auto cview = view(j);

            if (j >= in.size || ckind == TokKind::Eof)
            {
                i = j;
                return;
            }
            if (ckind == TokKind::Endl)
            {
                continue;
            }
            if (ckind == TokKind::Punct)
            {
                if (cview == "{")
                {
                    break;
                }
                if (cview == ",")
                {
                    continue;
                }
            }

            auto cmode = ParamMode::Plain;
            if (!cview.empty() && cview.back() == '*')
            {
                cmode = ParamMode::Greedy;
                cview.remove_suffix(1);
            }

            std::string folded = fold(cview);

            std::uint32_t nOff = textArena.put(folded.data(), static_cast<std::uint32_t>(folded.size()));

            std::uint32_t dBeg = U32_MAX, dEnd = U32_MAX;

            if ((j + 1 < in.size) && (in.kind[j + 1] == TokKind::Punct) && (view(j + 1) == ":"))
            {
                dBeg = macros.body.size;
                ckind = in.kind[(j += 2)];
                cview = view(j);
                while ((j < in.size) && (ckind != TokKind::Eof) && (ckind != TokKind::Endl) &&
                       /* TODO: find out if I can get rid of some of these redundant
                        * checks, and reevaluate if the enum class is really the right tool
                        * for this job */
                       !(ckind == TokKind::Punct && (cview == "," || cview == "{")))
                {
                    cview = view(j);
                    ckind = in.kind[j];
                    std::uint32_t off{textArena.put(cview.data(), (std::uint32_t)cview.size())};
                    macros.body.push(ckind, off, off + (std::uint32_t)cview.size());
                    ++j;
                }
                dEnd = macros.body.size;
                --j;
            }
            macros.params.push(nOff, (std::uint32_t)folded.size(), cmode, dBeg, dEnd);
        }

        std::uint32_t pe = macros.params.size;
        std::uint32_t bb = macros.body.size;

        int braceDepth = 1;

        for (j += 1;; ++j)
        {
            auto ckind = in.kind[j];

            auto cview = view(j);

            if (j >= in.size || ckind == TokKind::Eof)
            {
                break;
            }
            if (ckind == TokKind::Punct)
            {
                if (cview == "{")
                {
                    ++braceDepth;
                }
                else if (cview == "}" && (--braceDepth == 0))
                {
                    ++j;
                    break;
                }
            }

            std::uint32_t off = textArena.put(cview.data(), (std::uint32_t)cview.size());

            macros.body.push(ckind, off, off + static_cast<std::uint32_t>(cview.size()));
        }

        std::uint32_t be = macros.body.size;

        macros.byName[name] = macros.add(MacroKind::Macro, pb, pe, bb, be);

        i = skipEndl(j);
    }

    void expandMacro(std::uint32_t id, std::uint32_t b, std::uint32_t e)
    {
        if (depth + 1 > K_MAX_EXPAND)
        { // TODO: recursion error handling
            i = e;
            return;
        }
        std::uint32_t pb{macros.paramBeg[id]}, pe{macros.paramEnd[id]};
        std::uint32_t bb{macros.bodyBeg[id]}, be{macros.bodyEnd[id]};
        std::uint32_t nParams{pe - pb};

        std::vector<std::pair<std::uint32_t, std::uint32_t>> arg(nParams, {U32_MAX, U32_MAX});
        std::uint32_t j{b + 1}, p{};
        while (j < e && p < nParams)
        {
            bool greedy{macros.params.mode[pb + p] == ParamMode::Greedy};
            std::uint32_t ab{j};
            if (greedy)
            {
                j = e;
            }
            else
            {
                while (j < e && !(in.kind[j] == TokKind::Punct && view(j) == ","))
                    ++j;
            }
            arg[p] = {ab, j};
            if (j < e && in.kind[j] == TokKind::Punct && view(j) == ",")
                ++j;
            ++p;
        }

        ++depth;
        for (std::uint32_t n{bb}; n < be; ++n)
        {
            if (macros.body.kind[n] == TokKind::Symbol)
            {
                std::string_view t(textArena.data() + macros.body.start[n], macros.body.end[n] - macros.body.start[n]);
                std::string folded = fold(t);
                std::uint32_t matched{U32_MAX};
                for (std::uint32_t q{}; q < nParams; ++q)
                {
                    std::string_view pn(textArena.data() + macros.params.nameOff[pb + q],
                                        macros.params.nameLen[pb + q]);
                    if (folded == pn)
                    {
                        matched = q;
                        break;
                    }
                }
                if (matched != U32_MAX)
                {
                    if (arg[matched].first != U32_MAX)
                    {
                        for (std::uint32_t k{arg[matched].first}; k < arg[matched].second; ++k)
                            emitExpanded(k);
                    }
                    else
                    {
                        std::uint32_t dBeg{macros.params.defltBeg[pb + matched]},
                            dEnd{macros.params.defltEnd[pb + matched]};
                        for (std::uint32_t n{dBeg}; n != U32_MAX && n < dEnd; ++n)
                        {
                            emitStored(macros.body.kind[n], macros.body.start[n],
                                       macros.body.end[n] - macros.body.start[n]);
                        }
                    }
                    continue;
                }
            }
            emitStored(macros.body.kind[n], macros.body.start[n], macros.body.end[n] - macros.body.start[n]);
        }
        --depth;
        i = skipEndl(e);
    }

    bool dispatchDirective(std::uint32_t b, std::uint32_t e)
    {
        if (in.kind[b] == TokKind::Symbol && ieq(view(b), "macro"))
        {
            captureMacro(b);
        }
        else if (in.kind[b] == TokKind::Symbol && ieq(view(b), "restore"))
        {
            doRestore(b + 1, e);
        }
        else if (b + 1 < e && in.kind[b] == TokKind::Symbol &&
                 ((in.kind[b + 1] == TokKind::Symbol && ieq(view(b + 1), "equ")) ||
                  (in.kind[b + 1] == TokKind::Punct && view(b + 1) == "=")))
        {
            defineEqu(fold(view(b)), b + 2, e);
        }
        else
        {
            return false;
        }
        return true;
    }

    void run()
    {
        while (i < in.size)
        {
            TokKind k = in.kind[i];
            if (k == TokKind::Eof)
            {
                emitRaw(i);
                break;
            }
            if (k == TokKind::Endl)
            {
                emitRaw(i++);
                continue;
            }

            std::uint32_t b = i;
            std::uint32_t e = lineEnd(i);

            if (dispatchDirective(b, e))
            {
                continue;
            }
            for (auto j = b; j < e; ++j)
            {
                if ((in.kind[j] == TokKind::Symbol) && (view(j) == "\\") && (j + 1 < e) &&
                    (in.kind[j + 1] == TokKind::Endl))
                {
                    j++;
                    continue;
                }
                emitExpanded(j);
            }
            i = e;
        }
    }
};

#endif
