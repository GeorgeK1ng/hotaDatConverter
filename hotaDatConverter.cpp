// hotaDatConverter.cpp
// Purpose:
//   A 1:1 C++ reimplementation of the provided Python script. It matches the exact
//   behavior and data format, including encoding handling and JSON structure.
//
// Usage:
//   hotaDatConverter.exe <file.dat|file.json> [-e <encoding>]
//   -e / --encoding : text encoding for .dat strings and for reading/writing JSON files.
//                     Default: cp1252
//
// Behavior parity with Python:
//   • If the input file ends with ".dat", the tool writes a ".json" file.
//     The JSON file is written using the specified encoding (default cp1252).
//   • If the input file ends with ".json", the tool writes a ".dat" file.
//     The JSON file is read using the specified encoding.
//   • The binary blob at data["9"] is emitted and expected as a continuous lowercase hex string
//     with no spaces (exactly like the Python script).
//   • The "10" array is written as int32 little-endian values (same as Python struct.pack('i')).
//   • JSON keys for "data" are strings "0".."10", same as in Python.
//
// File format (exactly as in Python):
//   Header:
//     4 bytes: 'HDAT'
//     int32  : version == 2 (little-endian)
//     int32  : count of records
//   Each record:
//     int32 nameLen; nameLen bytes (encoded with the selected code page)
//     int32 folderLen; folderLen bytes (encoded with the selected code page)
//     int32 == 9 (fixed)
//     9 × { int32 len; len bytes } => data["0"].."8" (strings)
//     bool has9 (1 byte)
//       if has9:
//         int32 binLen; binLen bytes (raw) => data["9"] as lowercase hex string, no spaces
//     int32 N; N × int32 (little-endian)   => data["10"] as array of ints
//
// JSON structure:
//   [
//     { "name": "...", "foldername": "...", "data": { "0": "...", ..., "9": "a1b2...", "10": [1,2,...] } },
//     ...
//   ]
//
// Encoding notes:
//   • Windows: converts via Win32 API between UTF-8 and the requested code page (cp1252, cp1250, ...).
//   • Non-Windows: only UTF-8 is supported (like a practical limitation).
//
// Build: set C++17 in your project settings.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include "json.hpp"

using json = nlohmann::json;

// ---------- Helpers & I/O ----------

static inline void ensure(bool cond, const char* msg) {
    if (!cond) throw std::runtime_error(msg);
}

struct File {
    std::ifstream in;
    std::ofstream out;
    explicit File(const std::string& path, bool write) {
        if (write) {
            out.open(path, std::ios::binary);
            if (!out) throw std::runtime_error("Cannot open for writing: " + path);
        }
        else {
            in.open(path, std::ios::binary);
            if (!in) throw std::runtime_error("Cannot open for reading: " + path);
        }
    }
};

static uint32_t read_u32(std::ifstream& f) {
    uint8_t b[4];
    f.read(reinterpret_cast<char*>(b), 4);
    ensure(!!f, "Unexpected EOF while reading u32");
    return uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}

static void write_u32(std::ofstream& f, uint32_t v) {
    uint8_t b[4] = {
        static_cast<uint8_t>(v & 0xFF),
        static_cast<uint8_t>((v >> 8) & 0xFF),
        static_cast<uint8_t>((v >> 16) & 0xFF),
        static_cast<uint8_t>((v >> 24) & 0xFF)
    };
    f.write(reinterpret_cast<const char*>(b), 4);
}

static uint8_t read_u8(std::ifstream& f) {
    char c;
    f.read(&c, 1);
    ensure(!!f, "Unexpected EOF while reading u8");
    return static_cast<uint8_t>(c);
}

static void write_u8(std::ofstream& f, uint8_t v) {
    char c = static_cast<char>(v);
    f.write(&c, 1);
}

// ---------- Hex helpers ----------

static std::string to_lower_hex(const std::vector<uint8_t>& data) {
    static const char* hexd = "0123456789abcdef";
    std::string s;
    s.resize(data.size() * 2);
    for (size_t i = 0; i < data.size(); ++i) {
        s[2 * i] = hexd[(data[i] >> 4) & 0xF];
        s[2 * i + 1] = hexd[data[i] & 0xF];
    }
    return s;
}

static std::vector<uint8_t> from_hex_concat(const std::string& hex) {
    ensure(hex.size() % 2 == 0, "Hex string length must be even");
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int hi = val(hex[i]);
        int lo = val(hex[i + 1]);
        ensure(hi >= 0 && lo >= 0, "Invalid hex character in data[\"9\"]");
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

// ---------- Encoding (Windows / non-Windows) ----------

#ifdef _WIN32
using CodePage = UINT;
#else
using CodePage = unsigned; // dummy alias for non-Windows
#endif

#ifdef _WIN32
// Map encoding label to Windows code page.
static CodePage mapEncodingToCodePage(const std::string& encIn) {
    std::string enc = encIn;
    std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);

    static const std::unordered_map<std::string, CodePage> map = {
        {"cp1252", 1252}, {"windows-1252", 1252}, {"win1252", 1252},
        {"cp1250", 1250}, {"windows-1250", 1250}, {"win1250", 1250},
        {"cp1251", 1251}, {"windows-1251", 1251}, {"win1251", 1251},
        {"utf-8", CP_UTF8}, {"utf8", CP_UTF8}
    };

    auto it = map.find(enc);
    if (it != map.end()) return it->second;

    // Allow variants like "cp1252" / "windows1252" without separators
    std::string e2; e2.reserve(enc.size());
    for (char c : enc) if (c != '-' && c != '_') e2.push_back(c);

    if (e2.rfind("cp", 0) == 0)      return static_cast<CodePage>(std::stoi(e2.substr(2)));
    if (e2.rfind("windows", 0) == 0) return static_cast<CodePage>(std::stoi(e2.substr(7)));

    return 1252; // fallback
}

static std::string bytesToUtf8(const std::vector<uint8_t>& src, CodePage cp) {
    if (cp == CP_UTF8) {
        return std::string(reinterpret_cast<const char*>(src.data()), src.size());
    }
    // multibyte -> wide
    int wlen = MultiByteToWideChar(cp, 0,
        reinterpret_cast<LPCSTR>(src.data()),
        static_cast<int>(src.size()),
        nullptr, 0);
    ensure(wlen >= 0, "MultiByteToWideChar(size) failed");
    std::wstring wstr;
    wstr.resize(static_cast<size_t>(wlen));
    if (wlen > 0) {
        int wlen2 = MultiByteToWideChar(cp, 0,
            reinterpret_cast<LPCSTR>(src.data()),
            static_cast<int>(src.size()),
            wstr.empty() ? nullptr : &wstr[0],
            wlen);
        ensure(wlen2 > 0, "MultiByteToWideChar(conv) failed");
    }

    // wide -> utf8
    int u8len = WideCharToMultiByte(CP_UTF8, 0,
        wstr.empty() ? L"" : wstr.c_str(), wlen,
        nullptr, 0, nullptr, nullptr);
    std::string out;
    out.resize(static_cast<size_t>(u8len));
    if (u8len > 0) {
        int u8len2 = WideCharToMultiByte(CP_UTF8, 0,
            wstr.empty() ? L"" : wstr.c_str(), wlen,
            out.empty() ? nullptr : &out[0],
            u8len, nullptr, nullptr);
        ensure(u8len2 > 0, "WideCharToMultiByte(conv) failed");
    }
    return out;
}

static std::vector<uint8_t> utf8ToBytes(const std::string& s, CodePage cp) {
    if (cp == CP_UTF8) {
        return std::vector<uint8_t>(s.begin(), s.end());
    }
    // utf8 -> wide
    int wlen = MultiByteToWideChar(CP_UTF8, 0,
        s.c_str(), static_cast<int>(s.size()),
        nullptr, 0);
    ensure(wlen >= 0, "MultiByteToWideChar(UTF8,size) failed");
    std::wstring wstr;
    wstr.resize(static_cast<size_t>(wlen));
    if (wlen > 0) {
        int wlen2 = MultiByteToWideChar(CP_UTF8, 0,
            s.c_str(), static_cast<int>(s.size()),
            wstr.empty() ? nullptr : &wstr[0],
            wlen);
        ensure(wlen2 > 0, "MultiByteToWideChar(UTF8,conv) failed");
    }

    // wide -> target code page
    int mblen = WideCharToMultiByte(cp, 0,
        wstr.empty() ? L"" : wstr.c_str(), wlen,
        nullptr, 0, nullptr, nullptr);
    std::vector<uint8_t> out;
    out.resize(static_cast<size_t>(mblen));
    if (mblen > 0) {
        int mblen2 = WideCharToMultiByte(cp, 0,
            wstr.empty() ? L"" : wstr.c_str(), wlen,
            reinterpret_cast<LPSTR>(out.data()),
            mblen, nullptr, nullptr);
        ensure(mblen2 > 0, "WideCharToMultiByte(conv) failed");
    }
    return out;
}
#else // -------- Non-Windows (UTF-8 only) --------

static CodePage mapEncodingToCodePage(const std::string& encIn) {
    std::string enc = encIn;
    std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);
    if (enc == "utf-8" || enc == "utf8") return 65001;
    if (enc.rfind("cp", 0) == 0 || enc.rfind("windows", 0) == 0) {
        throw std::runtime_error("Non-Windows build supports only UTF-8 encoding.");
    }
    return 65001;
}

static std::string bytesToUtf8(const std::vector<uint8_t>& src, CodePage /*cp*/) {
    return std::string(reinterpret_cast<const char*>(src.data()), src.size());
}

static std::vector<uint8_t> utf8ToBytes(const std::string& s, CodePage /*cp*/) {
    return std::vector<uint8_t>(s.begin(), s.end());
}
#endif

// ---------- String I/O helpers with encoding ----------

static std::string read_enc_string(std::ifstream& f, CodePage cp) {
    uint32_t len = read_u32(f);
    std::vector<uint8_t> buf(len);
    if (len) f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(len));
    ensure(!!f, "Unexpected EOF while reading encoded string");
    return bytesToUtf8(buf, cp); // internal representation is UTF-8
}

static void write_enc_string(std::ofstream& f, const std::string& utf8, CodePage cp) {
    std::vector<uint8_t> enc = utf8ToBytes(utf8, cp);
    write_u32(f, static_cast<uint32_t>(enc.size()));
    if (!enc.empty()) f.write(reinterpret_cast<const char*>(enc.data()), static_cast<std::streamsize>(enc.size()));
}

static std::string readFixedTag(std::ifstream& f, size_t n) {
    std::string s(n, '\0');
    // some toolsets treat string::data() as const; use &s[0]
    f.read(s.empty() ? nullptr : &s[0], static_cast<std::streamsize>(n));
    ensure(!!f, "Unexpected EOF while reading tag");
    return s;
}

// ---------- .dat <-> JSON ----------

static json extractDat(const std::string& filename, CodePage cp) {
    File file(filename, /*write*/false);
    auto& f = file.in;

    ensure(readFixedTag(f, 4) == "HDAT", "Bad magic (expected 'HDAT')");
    uint32_t version = read_u32(f);
    ensure(version == 2, "Bad version (expected 2)");
    uint32_t count = read_u32(f);

    json arr = json::array();

    for (uint32_t idx = 0; idx < count; ++idx) {
        json obj;
        obj["name"] = read_enc_string(f, cp);
        obj["foldername"] = read_enc_string(f, cp);

        uint32_t nine = read_u32(f);
        ensure(nine == 9, "Bad fixed integer (expected 9)");

        json data = json::object();

        // 9 encoded strings: keys "0".."8"
        for (int i = 0; i < 9; ++i) {
            std::string s = read_enc_string(f, cp);
            data[std::to_string(i)] = s;
        }

        // Optional binary blob at data["9"]
        uint8_t has9 = read_u8(f);
        if (has9) {
            uint32_t blen = read_u32(f);
            std::vector<uint8_t> bin(blen);
            if (blen) f.read(reinterpret_cast<char*>(bin.data()), static_cast<std::streamsize>(blen));
            ensure(!!f, "Unexpected EOF while reading data[9] blob");
            data["9"] = to_lower_hex(bin); // continuous lowercase hex, no spaces
        }

        // data["10"] as array of int32
        uint32_t nints = read_u32(f);
        std::vector<int32_t> ints;
        ints.reserve(nints);
        for (uint32_t k = 0; k < nints; ++k) {
            int32_t x = static_cast<int32_t>(read_u32(f)); // raw bits, same as Python 'i'
            ints.push_back(x);
        }
        data["10"] = ints;

        obj["data"] = data;
        arr.push_back(obj);
    }

    return arr;
}

static void createDat(const std::string& filename, const json& arr, CodePage cp) {
    File file(filename, /*write*/true);
    auto& f = file.out;

    // Header
    f.write("HDAT", 4);
    write_u32(f, 2);
    write_u32(f, static_cast<uint32_t>(arr.size()));

    for (const auto& obj : arr) {
        // name & foldername
        write_enc_string(f, obj.at("name").get<std::string>(), cp);
        write_enc_string(f, obj.at("foldername").get<std::string>(), cp);

        // fixed 9
        write_u32(f, 9);

        const auto& data = obj.at("data");

        // The first 9 fields: "0".."8" as strings
        for (int i = 0; i < 9; ++i) {
            std::string key = std::to_string(i);
            std::string s = data.at(key).get<std::string>();
            write_enc_string(f, s, cp);
        }

        // Optional "9" (binary blob as hex)
        bool has9 = data.contains("9");
        write_u8(f, has9 ? 1 : 0);
        if (has9) {
            std::string hex = data.at("9").get<std::string>();
            std::vector<uint8_t> bin = from_hex_concat(hex);
            write_u32(f, static_cast<uint32_t>(bin.size()));
            if (!bin.empty()) f.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
        }

        // data["10"]: array of int32
        const auto& a10 = data.at("10");
        ensure(a10.is_array(), "data[\"10\"] must be an array");
        write_u32(f, static_cast<uint32_t>(a10.size()));
        for (const auto& v : a10) {
            int32_t x = v.get<int32_t>();
            write_u32(f, static_cast<uint32_t>(x)); // little-endian
        }
    }
}

// ---------- small file helpers ----------

static std::string loadWholeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + path);
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return data;
}

static void saveWholeFile(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open for writing: " + path);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// ---------- main ----------

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <file.dat|file.json> [-e <encoding>]\n";
            return 2;
        }
        std::string filename = argv[1];
        std::string enc = "cp1252";
        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if ((a == "-e" || a == "--encoding") && i + 1 < argc) {
                enc = argv[++i];
            }
        }

        CodePage cp = mapEncodingToCodePage(enc);

        auto endsWith = [](const std::string& s, const std::string& suf) {
            if (s.size() < suf.size()) return false;
            return std::equal(
                suf.rbegin(), suf.rend(), s.rbegin(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) ==
                        std::tolower(static_cast<unsigned char>(b));
                }
            );
        };

        if (endsWith(filename, ".dat")) {
            // .dat -> .json (JSON written using selected encoding)
            json arr = extractDat(filename, cp);
            std::string pretty = arr.dump(2); // UTF-8 in memory

#ifdef _WIN32
            std::vector<uint8_t> encBytes = utf8ToBytes(pretty, cp);
            std::string outbytes(encBytes.begin(), encBytes.end());
#else
            std::string outbytes = pretty; // UTF-8 only
#endif
            std::string outname = filename.substr(0, filename.find_last_of('.')) + ".json";
            saveWholeFile(outname, outbytes);
            return 0;

        }
        else if (endsWith(filename, ".json")) {
            // .json -> .dat (JSON read using selected encoding)
            std::string raw = loadWholeFile(filename);
#ifdef _WIN32
            std::vector<uint8_t> bytes(raw.begin(), raw.end());
            std::string utf8 = bytesToUtf8(bytes, cp);
#else
            std::string utf8 = raw; // UTF-8 only
#endif
            json arr = json::parse(utf8);
            std::string outname = filename.substr(0, filename.find_last_of('.')) + ".dat";
            createDat(outname, arr, cp);
            return 0;

        }
        else {
            std::cerr << "Unsupported file extension. Use .dat or .json\n";
            return 3;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
