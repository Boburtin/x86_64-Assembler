#ifndef SOURCE_H
#define SOURCE_H

#include <fstream>
#include <iterator>
#include <string>

class Source
{
  public:
    Source(std::string &&str) : bytes_(str)
    {
        // TODO: handle UTF16 and UTF32 BOM
        if (bytes_.size() >= 3 && (static_cast<std::uint8_t>(bytes_[0]) == 0xEF) &&
            (static_cast<std::uint8_t>(bytes_[1]) == 0xBB) && (static_cast<std::uint8_t>(bytes_[2]) == 0xBF))
        {
            bytes_.erase(0, 3);
        }
    }
    static Source fromFile(const char *fp)
    {
        std::ifstream file(fp, std::ios::binary);

        return Source({(std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()});
    }

    std::uint32_t size() const noexcept
    {
        return bytes_.size();
    }
    char at(std::uint32_t idx) const noexcept
    {
        return bytes_[idx];
    }
    const char *data() const noexcept
    {
        return bytes_.data();
    }

  private:
    std::string bytes_;
};

#endif
