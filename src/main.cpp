#include <cstdio>

#include "Preproc.h"
#include "Prim.h"
#include "Source.h"
#include "TokUtils.h"

const char *BOLD = "\033[1m[%05llu]\033[0m %s";

const char *chSuccess = "[+]";
const char *chFailure = "[-]";
const char *chWarning = "[!]";
const char *chUpdate = "[*]";

const char *Eof = " \033[1;32mEOF!\033[0m";
const char *Punct = " \033[3;31mPunct\033[0m";
const char *Endl = "  \033[1;33mEndl\033[0m";
const char *String = " \033[3;30mString\033[0m";
const char *Symbol = "\033[3;34mSymbol\033[0m";

const char *TokType[] = {String, Symbol, Punct, Eof, Endl};

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "\n\033[1;36m%s usage:\033[35m pqry <file>\033[0m\n", chFailure);
        return 2;
    }

    auto src = Source::fromFile(argv[1]);
    auto toks = TokArray(src.size() + 1);

    TMaker(src).scan(toks);
    Preproc preproc{src, toks};
    preproc.run();

    auto tknPrint = [](const TokArray &t) {
        for (std::size_t i{}; i < t.size; ++i)
        {
            std::printf(BOLD, i, TokType[std::to_underlying(t.kind[i])]);

            if (i % 12 == 11)
            {
                std::putchar('\n');
            }
        }
    };

    std::puts("\n\nTokens after first pass:\n");

    tknPrint(toks);

    std::puts("\n\nTokens after second pass:\n");

    tknPrint(preproc.out);

    return 0;
}
