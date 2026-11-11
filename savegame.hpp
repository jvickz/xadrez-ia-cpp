#pragma once
#include "match.hpp"
#include <windows.h>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace chess {

struct SavedGame {
    Match match;
    Color human = Color::white;
    int depth = 2;
    bool sound = true;
};

inline std::filesystem::path defaultSavePath() {
    const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!size) return {};
    std::wstring folder(size, L'\0');
    const DWORD written = GetEnvironmentVariableW(L"LOCALAPPDATA", folder.data(), size);
    if (!written || written >= size) return {};
    folder.resize(written);
    return std::filesystem::path(folder) / L"XadrezIA" / L"partida.sav";
}

inline std::string encodeSavedGame(const SavedGame& saved) {
    if ((saved.human != Color::white && saved.human != Color::black) ||
        (saved.depth != 1 && saved.depth != 2 && saved.depth != 4) || saved.match.history.size() > 4096)
        throw std::runtime_error("invalid save settings");
    std::ostringstream out;
    out << "XADREZ_IA_SAVE 1\nhuman " << (saved.human == Color::white ? 0 : 1)
        << "\ndepth " << saved.depth << "\nsound " << saved.sound
        << "\nmoves " << saved.match.history.size() << '\n';
    for (Move move : saved.match.history) out << notation(move) << '\n';
    out << "END\n";
    return out.str();
}

// Replay only legal moves from the initial position, rebuilding captures, undo
// snapshots, repetition counts, castling rights and the en-passant target.
inline SavedGame decodeSavedGame(const std::string& data) {
    if (data.size() > 65536) throw std::runtime_error("save too large");
    std::istringstream in(data);
    const auto expect = [&](const char* wanted) {
        std::string token;
        if (!(in >> token) || token != wanted) throw std::runtime_error("invalid save format");
    };
    expect("XADREZ_IA_SAVE");
    int version = 0, human = -1, depth = 0, sound = -1, count = -1;
    if (!(in >> version) || version != 1) throw std::runtime_error("unsupported save version");
    expect("human"); in >> human;
    expect("depth"); in >> depth;
    expect("sound"); in >> sound;
    expect("moves"); in >> count;
    if (!in || (human != 0 && human != 1) || (depth != 1 && depth != 2 && depth != 4) ||
        (sound != 0 && sound != 1) || count < 0 || count > 4096)
        throw std::runtime_error("invalid save settings");
    SavedGame loaded;
    loaded.human = human == 0 ? Color::white : Color::black;
    loaded.depth = depth;
    loaded.sound = sound != 0;
    for (int i = 0; i < count; ++i) {
        std::string token;
        if (!(in >> token)) throw std::runtime_error("truncated save");
        const auto move = parseMove(token);
        const auto moves = legalMoves(loaded.match.position);
        const auto key = repetitionKey(loaded.match.position, moves);
        if (!move || std::find(moves.begin(), moves.end(), *move) == moves.end() ||
            result(loaded.match.position, moves, loaded.match.repetitions.at(key)) != Result::ongoing)
            throw std::runtime_error("illegal move in save");
        loaded.match.apply(*move);
    }
    expect("END");
    std::string extra;
    if (in >> extra) throw std::runtime_error("unexpected save data");
    return loaded;
}

inline SavedGame loadGameFile(const std::filesystem::path& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot open save");
    struct File { HANDLE value; ~File() { CloseHandle(value); } } guard{file};
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 65536)
        throw std::runtime_error("invalid save size");
    std::string data(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    if (!ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &read, nullptr) || read != data.size())
        throw std::runtime_error("cannot read save");
    return decodeSavedGame(data);
}

inline void saveGameFile(const std::filesystem::path& path, const SavedGame& saved) {
    if (path.empty()) throw std::runtime_error("save path unavailable");
    const auto data = encodeSavedGame(saved);
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) + L".tmp";
    const HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create temporary save");
    DWORD written = 0;
    const bool flushed = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                         written == data.size() && FlushFileBuffers(file);
    const bool closed = CloseHandle(file) != FALSE;
    // Replace only after a complete, flushed write. A failed save preserves the old file.
    if (!flushed || !closed || !MoveFileExW(temporary.c_str(), path.c_str(),
                                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("cannot commit save");
    }
}

} // namespace chess
