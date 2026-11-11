#pragma once
#include "chess.hpp"
#include <algorithm>
#include <unordered_map>

namespace chess {

struct MatchSnapshot {
    Position position;
    std::array<std::vector<char>, 2> captures;
};

struct Match {
    Position position;
    std::vector<Move> history;
    std::array<std::vector<char>, 2> captures;
    std::unordered_map<std::string, int> repetitions;
    std::vector<MatchSnapshot> past;

    explicit Match(Position start = initialPosition()) { reset(start); }

    void reset(Position start = initialPosition()) {
        position = start;
        history.clear();
        past.clear();
        for (auto& pieces : captures) pieces.clear();
        repetitions.clear();
        repetitions[repetitionKey(position, legalMoves(position))] = 1;
    }

    void apply(Move move) {
        past.push_back({position, captures});
        const char captured = capturedPiece(position, move);
        if (captured != '.') captures[position.turn == Color::white ? 0 : 1].push_back(captured);
        history.push_back(move);
        position = afterMove(position, move);
        ++repetitions[repetitionKey(position, legalMoves(position))];
    }

    std::optional<std::size_t> undoTarget(Color human) const {
        for (std::size_t i = past.size(); i > 0; --i)
            if (past[i - 1].position.turn == human) return i - 1;
        return std::nullopt;
    }

    bool undo(Color human) {
        const auto target = undoTarget(human);
        if (!target) return false;
        position = past[*target].position;
        captures = past[*target].captures;
        past.resize(*target);
        history.resize(*target);
        repetitions.clear();
        for (const auto& snapshot : past)
            ++repetitions[repetitionKey(snapshot.position, legalMoves(snapshot.position))];
        ++repetitions[repetitionKey(position, legalMoves(position))];
        return true;
    }
};
}  // namespace chess
