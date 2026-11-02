#pragma once

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

namespace loader3ds {

// All positions are nonnegative stream offsets bounded by the physical file.
// Subtraction precedes addition so hostile uint32 chunk lengths cannot wrap.
class Reader {
public:
    struct Chunk { uint16_t id; std::streamoff end; };

    explicit Reader(std::ifstream& stream) : file(stream) {
        file.seekg(0, std::ios::end);
        size = file.tellg();
        if (!file || size < 0)
            throw std::runtime_error("cannot determine 3DS file size");
        file.seekg(0);
        if (!file)
            throw std::runtime_error("cannot seek 3DS file");
    }

    void require(std::streamoff bytes, std::streamoff end) const {
        if (bytes < 0 || end > size || pos > end || bytes > end - pos)
            throw std::runtime_error("truncated 3DS chunk payload");
    }

    void read(void* data, std::streamsize bytes, std::streamoff end) {
        require(bytes, end);
        if (!file.read(static_cast<char*>(data), bytes))
            throw std::runtime_error("cannot read 3DS chunk payload");
        pos += bytes;
    }

    template<typename T> T value(std::streamoff end) {
        T result{};
        read(&result, sizeof(T), end);
        return result;
    }

    Chunk chunk(std::streamoff end) {
        require(6, end);
        const auto id = value<uint16_t>(end);
        const auto length = value<uint32_t>(end);
        if (length < 6)
            throw std::runtime_error("3DS chunk length is smaller than its header");
        const auto payload = static_cast<std::streamoff>(length) - 6;
        require(payload, end);
        return {id, pos + payload};
    }

    void skip(std::streamoff end) {
        require(0, end);
        file.seekg(end);
        if (!file)
            throw std::runtime_error("cannot seek to 3DS chunk end");
        pos = end;
    }

    std::string string(std::streamoff end) {
        std::string result;
        while (pos < end) {
            const auto ch = value<char>(end);
            if (!ch) return result;
            result.push_back(ch);
        }
        throw std::runtime_error("unterminated 3DS string");
    }

    template<typename F> void children(std::streamoff end, F visit) {
        while (pos < end) {
            const auto child = chunk(end);
            visit(child);
            skip(child.end);
        }
    }

    std::streamoff size = 0;
private:
    std::ifstream& file;
    std::streamoff pos = 0;
};

} // namespace loader3ds
