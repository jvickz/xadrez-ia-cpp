#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chess {

enum class Color { white, black };
enum Castling { whiteShort = 1, whiteLong = 2, blackShort = 4, blackLong = 8 };

struct Move {
    int from;
    int to;
    char promotion = '.';
    bool operator==(const Move& other) const;
};

struct Position {
    // a1 = 0, h8 = 63. Uppercase pieces are white; '.' is empty.
    std::array<char, 64> board;
    Color turn = Color::white;
    unsigned castling = 0;
    int enPassant = -1;
    int halfmoves = 0;
};

Position initialPosition();
bool inCheck(const Position& position, Color color);
std::vector<Move> legalMoves(const Position& position);
// The caller supplies a move from legalMoves().
Position afterMove(const Position& position, Move move);
// Returns the captured piece (including en passant), or '.' for a legal quiet move.
char capturedPiece(const Position& position, Move move);
std::optional<Move> parseMove(std::string_view input);
std::string notation(Move move);
bool insufficientMaterial(const Position& position);
std::string repetitionKey(const Position& position, const std::vector<Move>& legal);

enum class Result { ongoing, checkmate, stalemate, materialDraw, repetitionDraw, moveCountDraw };
Result result(const Position& position, const std::vector<Move>& legal, int repetitions);
// Values 1-3 select the basic fixed-depth search; 4+ select the timed hard engine.
std::optional<Move> chooseMove(const Position& position, int depth);

}  // namespace chess
