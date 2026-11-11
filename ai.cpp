#include "chess.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdint>

namespace chess {
namespace {

constexpr int mateScore = 100000;
constexpr int infinity = 1000000;

char kind(char piece) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
}

int value(char piece) {
    switch (kind(piece)) {
        case 'p': return 100;
        case 'n': return 320;
        case 'b': return 330;
        case 'r': return 500;
        case 'q': return 900;
        default: return 0;
    }
}

int evaluate(const Position& p) {
    int score = 0;
    for (int square = 0; square < 64; ++square) {
        const char piece = p.board[square];
        if (piece == '.') continue;
        const bool white = piece != kind(piece);
        const int file = square % 8, rank = square / 8;
        const int center = 14 - std::abs(2 * file - 7) - std::abs(2 * rank - 7);
        int bonus = 0;
        switch (kind(piece)) {
            case 'p': bonus = center + 6 * (white ? rank : 7 - rank); break;
            case 'n': bonus = 4 * center; break;
            case 'b': bonus = 2 * center; break;
            default: break;
        }
        score += (white ? 1 : -1) * (value(piece) + bonus);
    }
    return p.turn == Color::white ? score : -score;
}

void orderMoves(const Position& p, std::vector<Move>& moves) {
    const auto priority = [&](Move move) {
        const char mover = p.board[move.from];
        int victim = value(p.board[move.to]);
        if (kind(mover) == 'p' && move.to == p.enPassant) victim = 100;
        return (victim ? 10 * victim - value(mover) : 0) + 10 * value(move.promotion);
    };
    std::stable_sort(moves.begin(), moves.end(), [&](Move a, Move b) {
        return priority(a) > priority(b);
    });
}

// Negamax is minimax expressed from the perspective of the side to move.
int search(const Position& p, int depth, int ply, int alpha, int beta) {
    auto moves = legalMoves(p);
    const Result outcome = result(p, moves, 1);
    if (outcome == Result::checkmate) return -mateScore + ply;
    if (outcome != Result::ongoing) return 0;
    if (depth == 0) return evaluate(p);
    orderMoves(p, moves);
    int best = -infinity;
    for (Move move : moves) {
        const int score = -search(afterMove(p, move), depth - 1, ply + 1, -beta, -alpha);
        best = std::max(best, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) break;
    }
    return best;
}

int positionalEvaluation(const Position& p) {
    int pawns[2][8]{};
    int bishops[2]{};
    int phase = 0;
    for (int square = 0; square < 64; ++square) {
        const char piece = p.board[square];
        const int side = piece != kind(piece) ? 0 : 1;
        if (kind(piece) == 'p') ++pawns[side][square % 8];
        if (kind(piece) == 'b') ++bishops[side];
        switch (kind(piece)) {
            case 'n': case 'b': phase += 1; break;
            case 'r': phase += 2; break;
            case 'q': phase += 4; break;
            default: break;
        }
    }
    phase = std::min(phase, 24);
    int score = 0;
    for (int square = 0; square < 64; ++square) {
        const char piece = p.board[square];
        if (piece == '.') continue;
        const bool white = piece != kind(piece);
        const int side = white ? 0 : 1;
        const int file = square % 8;
        const int rank = white ? square / 8 : 7 - square / 8;
        const int center = 14 - std::abs(2 * file - 7) - std::abs(2 * rank - 7);
        int bonus = 0;
        switch (kind(piece)) {
            case 'p': {
                bonus = center + rank * 5;
                if (pawns[side][file] > 1) bonus -= 14;
                if ((file == 0 || pawns[side][file - 1] == 0) &&
                    (file == 7 || pawns[side][file + 1] == 0)) bonus -= 16;
                bool passed = true;
                for (int other = 0; other < 64; ++other) {
                    const int otherRank = white ? other / 8 : 7 - other / 8;
                    if (p.board[other] == (white ? 'p' : 'P') &&
                        std::abs(other % 8 - file) <= 1 && otherRank > rank) {
                        passed = false;
                        break;
                    }
                }
                if (passed) bonus += rank * rank * (48 - phase) / 12;
                break;
            }
            case 'n': bonus = 5 * center - (rank == 0 ? 15 : 0); break;
            case 'b': bonus = 3 * center + (bishops[side] >= 2 ? 18 : 0); break;
            case 'r':
                if (pawns[side][file] == 0) bonus += pawns[1 - side][file] == 0 ? 26 : 14;
                if (rank == 6) bonus += 20;
                break;
            case 'q': bonus = center - (rank > 2 ? phase : 0); break;
            case 'k': {
                int shelter = -4 * center - 10 * rank;
                if (rank == 0 && (file <= 2 || file >= 6)) shelter += 35;
                for (int offset = -1; offset <= 1; ++offset) {
                    const int shieldFile = file + offset;
                    const int shieldRank = square / 8 + (white ? 1 : -1);
                    if (shieldFile >= 0 && shieldFile < 8 && shieldRank >= 0 && shieldRank < 8 &&
                        p.board[shieldRank * 8 + shieldFile] == (white ? 'P' : 'p')) shelter += 14;
                }
                bonus = (phase * shelter + (24 - phase) * 5 * center) / 24;
                break;
            }
            default: break;
        }
        score += (white ? 1 : -1) * (value(piece) + bonus);
    }
    return p.turn == Color::white ? score : -score;
}

// A table belongs to one search, so GUI background searches never share mutable state.
enum class Bound { exact, lower, upper };
struct Entry {
    std::uint64_t key = 0;
    int depth = -1;
    int score = 0;
    Bound bound = Bound::exact;
    Move move{-1, -1};
};

std::uint64_t positionHash(const Position& p) {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto add = [&](unsigned value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    for (char piece : p.board) add(static_cast<unsigned char>(piece));
    add(static_cast<unsigned>(p.turn));
    add(p.castling);
    add(static_cast<unsigned>(p.enPassant + 1));
    add(static_cast<unsigned>(p.halfmoves));
    return hash;
}

int storedScore(int score, int ply) {
    if (score > mateScore - 1000) return score + ply;
    if (score < -mateScore + 1000) return score - ply;
    return score;
}

int restoredScore(int score, int ply) {
    return storedScore(score, -ply);
}

struct SearchExpired {};

class StrongSearch {
public:
    std::optional<Move> run(const Position& p, std::vector<Move> moves) {
        orderMoves(p, moves);
        Move completed = moves.front();
        for (int depth = 1; depth <= 8; ++depth) {
            try {
                prioritize(moves, completed);
                int bestScore = -infinity;
                Move best = completed;
                for (Move move : moves) {
                    const int score = -node(afterMove(p, move), depth - 1, 1, -infinity, -bestScore);
                    if (score > bestScore) {
                        bestScore = score;
                        best = move;
                    }
                }
                completed = best;
                if (bestScore >= mateScore - depth) break;
            } catch (const SearchExpired&) {
                // Only a fully searched root iteration replaces the previous decision.
                break;
            }
        }
        return completed;
    }

private:
    std::vector<Entry> table = std::vector<Entry>(1 << 16);
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
    unsigned nodes = 0;

    void checkTime() {
        if ((++nodes & 127) == 0 && std::chrono::steady_clock::now() >= deadline)
            throw SearchExpired{};
    }

    static void prioritize(std::vector<Move>& moves, Move preferred) {
        const auto found = std::find(moves.begin(), moves.end(), preferred);
        if (found != moves.end()) std::rotate(moves.begin(), found, found + 1);
    }

    int quiet(const Position& p, int ply, int alpha, int beta, int remaining) {
        checkTime();
        auto moves = legalMoves(p);
        const auto outcome = result(p, moves, 1);
        if (outcome == Result::checkmate) return -mateScore + ply;
        if (outcome != Result::ongoing) return 0;
        const bool checked = inCheck(p, p.turn);
        const int evaluation = positionalEvaluation(p);
        if (ply >= 32 || (remaining <= 0 && !checked)) return evaluation;
        if (!checked) {
            if (evaluation >= beta) return evaluation;
            alpha = std::max(alpha, evaluation);
            moves.erase(std::remove_if(moves.begin(), moves.end(), [&](Move move) {
                const bool enPassant = kind(p.board[move.from]) == 'p' && move.to == p.enPassant;
                return p.board[move.to] == '.' && move.promotion == '.' && !enPassant;
            }), moves.end());
        }
        orderMoves(p, moves);
        for (Move move : moves) {
            const int score = -quiet(afterMove(p, move), ply + 1, -beta, -alpha, remaining - 1);
            if (score >= beta) return score;
            alpha = std::max(alpha, score);
        }
        return alpha;
    }

    int node(const Position& p, int depth, int ply, int alpha, int beta) {
        if (depth <= 0) return quiet(p, ply, alpha, beta, 8);
        checkTime();
        const auto key = positionHash(p);
        // Copy before recursion, which can overwrite this table slot.
        const Entry cached = table[key % table.size()];
        if (cached.key == key && cached.depth >= depth) {
            const int score = restoredScore(cached.score, ply);
            if (cached.bound == Bound::exact ||
                (cached.bound == Bound::lower && score >= beta) ||
                (cached.bound == Bound::upper && score <= alpha)) return score;
        }
        auto moves = legalMoves(p);
        const auto outcome = result(p, moves, 1);
        if (outcome == Result::checkmate) return -mateScore + ply;
        if (outcome != Result::ongoing) return 0;
        orderMoves(p, moves);
        if (cached.key == key) prioritize(moves, cached.move);
        const int originalAlpha = alpha;
        int bestScore = -infinity;
        Move best = moves.front();
        for (Move move : moves) {
            const int score = -node(afterMove(p, move), depth - 1, ply + 1, -beta, -alpha);
            if (score > bestScore) {
                bestScore = score;
                best = move;
            }
            alpha = std::max(alpha, score);
            if (alpha >= beta) break;
        }
        Bound bound = Bound::exact;
        if (bestScore <= originalAlpha) bound = Bound::upper;
        else if (bestScore >= beta) bound = Bound::lower;
        table[key % table.size()] = {key, depth, storedScore(bestScore, ply), bound, best};
        return bestScore;
    }
};

}  // namespace

std::optional<Move> chooseMove(const Position& p, int depth) {
    auto moves = legalMoves(p);
    if (result(p, moves, 1) != Result::ongoing) return std::nullopt;
    if (depth >= 4) return StrongSearch{}.run(p, moves);
    orderMoves(p, moves);
    depth = std::clamp(depth, 1, 4);
    int bestScore = -infinity;
    Move best = moves.front();
    for (Move move : moves) {
        const int score = -search(afterMove(p, move), depth - 1, 1, -infinity, -bestScore);
        if (score > bestScore) {
            bestScore = score;
            best = move;
        }
    }
    return best;
}

}  // namespace chess
