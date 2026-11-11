#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace chess {

enum class SoundCue { move, capture, check, victory, defeat, draw };

// Original, synthesized effects: PCM stays alive throughout asynchronous playback.
inline const std::vector<unsigned char>& soundData(SoundCue cue) {
    static const auto bank = [] {
        std::array<std::vector<unsigned char>, 6> clips;
        constexpr unsigned rate = 22050;
        constexpr double pi = 3.14159265358979323846;
        const std::array<std::vector<double>, 6> notes{{
            {440}, {260, 180}, {740, 988}, {523.25, 659.25, 783.99, 1046.5},
            {392, 329.63, 261.63}, {440, 523.25, 440}
        }};
        const std::array<double, 6> lengths{{0.09, 0.08, 0.14, 0.17, 0.21, 0.17}};
        for (std::size_t c = 0; c < clips.size(); ++c) {
            auto& wave = clips[c];
            const unsigned framesPerNote = static_cast<unsigned>(rate * lengths[c]);
            const unsigned frames = framesPerNote * static_cast<unsigned>(notes[c].size());
            const auto word = [&](unsigned value, int bytes) {
                for (int b = 0; b < bytes; ++b) wave.push_back(static_cast<unsigned char>(value >> (b * 8)));
            };
            const auto tag = [&](const char* text) { wave.insert(wave.end(), text, text + 4); };
            wave.reserve(44 + frames * 2);
            tag("RIFF"); word(36 + frames * 2, 4); tag("WAVE");
            tag("fmt "); word(16, 4); word(1, 2); word(1, 2); // PCM, mono.
            word(rate, 4); word(rate * 2, 4); word(2, 2); word(16, 2);
            tag("data"); word(frames * 2, 4);
            for (double frequency : notes[c]) {
                for (unsigned i = 0; i < framesPerNote; ++i) {
                    const double t = static_cast<double>(i) / rate;
                    const double remaining = static_cast<double>(framesPerNote - 1 - i) / rate;
                    const double envelope = std::min(1.0, t / 0.004) *
                        std::min(1.0, remaining / 0.025) * std::exp(-t * (c < 2 ? 28 : 4));
                    const double tone = std::sin(2 * pi * frequency * t) +
                        0.22 * std::sin(2 * pi * frequency * 2.01 * t);
                    const auto sample = static_cast<std::int16_t>(6500 * envelope * tone);
                    word(static_cast<std::uint16_t>(sample), 2);
                }
            }
        }
        return clips;
    }();
    return bank[static_cast<std::size_t>(cue)];
}

class ChessAudio {
public:
    using Playback = BOOL (WINAPI*)(LPCWSTR, HMODULE, DWORD);
    explicit ChessAudio(Playback playback = PlaySoundW) : playback_(playback) {}
    ~ChessAudio() { stop(); }
    ChessAudio(const ChessAudio&) = delete;
    ChessAudio& operator=(const ChessAudio&) = delete;
    bool enabled() const { return enabled_; }
    void toggle() { enabled_ = !enabled_; if (!enabled_) stop(); }
    void stop() { playback_(nullptr, nullptr, 0); }
    bool play(SoundCue cue) {
        if (!enabled_) return false;
        const auto& wave = soundData(cue);
        return playback_(reinterpret_cast<LPCWSTR>(wave.data()), nullptr,
                         SND_MEMORY | SND_ASYNC | SND_NODEFAULT) != FALSE;
    }
private:
    Playback playback_;
    bool enabled_ = true;
};

} // namespace chess
