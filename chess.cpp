#include "chess.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace chess {
namespace {

using Direction = std::pair<int, int>;
constexpr Direction rays[] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {1,-1}, {-1,-1}};
constexpr Direction knightSteps[] = {{1,2}, {2,1}, {-1,2}, {-2,1}, {1,-2}, {2,-1}, {-1,-2}, {-2,-1}};

Color opposite(Color color) {
    return color == Color::white ? Color::black : Color::white;
}

char lower(char piece) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
}

char pieceFor(char kind, Color color) {
    return color == Color::white ? static_cast<char>(std::toupper(kind)) : kind;
}

bool belongs(char piece, Color color) {
    return piece != '.' && (piece != lower(piece)) == (color == Color::white);
}

int advance(int square, int dx, int dy) {
    const int file = square % 8 + dx;
    const int rank = square / 8 + dy;
    return file >= 0 && file < 8 && rank >= 0 && rank < 8 ? rank * 8 + file : -1;
}

int forward(Color color) {
    return color == Color::white ? 1 : -1;
}

bool attacked(const Position& p, int target, Color by) {
    for (int dx : {-1, 1}) {
        const int from = advance(target, dx, -forward(by));
        if (from >= 0 && p.board[from] == pieceFor('p', by)) return true;
    }
    for (auto [dx, dy] : knightSteps) {
        const int from = advance(target, dx, dy);
        if (from >= 0 && p.board[from] == pieceFor('n', by)) return true;
    }
    for (auto [dx, dy] : rays) {
        int from = advance(target, dx, dy);
        bool adjacent = true;
        while (from >= 0) {
            const char piece = p.board[from];
            if (piece != '.') {
                if (belongs(piece, by)) {
                    const char kind = lower(piece);
                    const char slider = dx != 0 && dy != 0 ? 'b' : 'r';
                    if (kind == 'q' || kind == slider || (adjacent && kind == 'k')) return true;
                }
                break;
            }
            adjacent = false;
            from = advance(from, dx, dy);
        }
    }
    return false;
}

void addPawnMove(std::vector<Move>& moves, int from, int to) {
    if (to / 8 == 0 || to / 8 == 7) {
        for (char kind : {'q', 'r', 'b', 'n'}) moves.push_back({from, to, kind});
    } else {
        moves.push_back({from, to});
    }
}

void generatePawn(const Position& p, int from, std::vector<Move>& moves) {
    const int dy = forward(p.turn);
    const int next = advance(from, 0, dy);
    const int home = p.turn == Color::white ? 1 : 6;
    if (next >= 0 && p.board[next] == '.') {
        addPawnMove(moves, from, next);
        if (from / 8 == home && p.board[next + 8 * dy] == '.')
            moves.push_back({from, next + 8 * dy});
    }
    for (int dx : {-1, 1}) {
        const int to = advance(from, dx, dy);
        if (to < 0) continue;
        const char target = p.board[to];
        const bool capture = belongs(target, opposite(p.turn)) && lower(target) != 'k';
        const bool enPassant = to == p.enPassant && target == '.' &&
            p.board[to - 8 * dy] == pieceFor('p', opposite(p.turn));
        if (capture || enPassant) addPawnMove(moves, from, to);
    }
}

void generateCastles(const Position& p, int from, std::vector<Move>& moves) {
    const bool white = p.turn == Color::white;
    if (from != (white ? 4 : 60) || inCheck(p, p.turn)) return;
    for (int side : {-1, 1}) {
        const unsigned right = side == 1 ? (white ? whiteShort : blackShort)
                                          : (white ? whiteLong : blackLong);
        const int rook = from + (side == 1 ? 3 : -4);
        const int transit = from + side;
        const int destination = from + 2 * side;
        if (!(p.castling & right) || p.board[rook] != pieceFor('r', p.turn) ||
            p.board[transit] != '.' || p.board[destination] != '.' ||
            (side == -1 && p.board[from - 3] != '.')) continue;
        if (!inCheck(afterMove(p, {from, transit}), p.turn)) moves.push_back({from, destination});
    }
}

unsigned rookRight(int square) {
    switch (square) {
        case 0: return whiteLong;
        case 7: return whiteShort;
        case 56: return blackLong;
        case 63: return blackShort;
        default: return 0;
    }
}

}  // namespace

bool Move::operator==(const Move& other) const {
    return from == other.from && to == other.to && promotion == other.promotion;
}

Position initialPosition() {
    Position p;
    constexpr std::string_view squares =
        "RNBQKBNRPPPPPPPP................................pppppppprnbqkbnr";
    std::copy(squares.begin(), squares.end(), p.board.begin());
    p.castling = whiteShort | whiteLong | blackShort | blackLong;
    return p;
}

bool inCheck(const Position& position, Color color) {
    const auto king = std::find(position.board.begin(), position.board.end(), pieceFor('k', color));
    return king != position.board.end() &&
           attacked(position, static_cast<int>(king - position.board.begin()), opposite(color));
}

std::vector<Move> legalMoves(const Position& p) {
    std::vector<Move> candidates;
    candidates.reserve(40);
    for (int from = 0; from < 64; ++from) {
        const char piece = p.board[from];
        if (!belongs(piece, p.turn)) continue;
        const char kind = lower(piece);
        if (kind == 'p') {
            generatePawn(p, from, candidates);
        } else if (kind == 'n') {
            for (auto [dx, dy] : knightSteps) {
                const int to = advance(from, dx, dy);
                if (to >= 0 && !belongs(p.board[to], p.turn) && lower(p.board[to]) != 'k')
                    candidates.push_back({from, to});
            }
        } else {
            for (auto [dx, dy] : rays) {
                const bool diagonal = dx != 0 && dy != 0;
                if ((kind == 'b' && !diagonal) || (kind == 'r' && diagonal)) continue;
                int to = advance(from, dx, dy);
                while (to >= 0 && !belongs(p.board[to], p.turn)) {
                    if (lower(p.board[to]) != 'k') candidates.push_back({from, to});
                    if (kind == 'k' || p.board[to] != '.') break;
                    to = advance(to, dx, dy);
                }
            }
            if (kind == 'k' && p.castling) generateCastles(p, from, candidates);
        }
    }
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](Move move) {
        return inCheck(afterMove(p, move), p.turn);
    }), candidates.end());
    return candidates;
}

char capturedPiece(const Position& p, Move move) {
    if (lower(p.board[move.from]) == 'p' && move.to == p.enPassant &&
        p.board[move.to] == '.' && move.from % 8 != move.to % 8)
        return p.board[move.to - 8 * forward(p.turn)];
    return p.board[move.to];
}

Position afterMove(const Position& p, Move move) {
    Position next = p;
    const char piece = p.board[move.from];
    const char kind = lower(piece);
    const char target = p.board[move.to];
    const int distance = move.to - move.from;
    const int step = 8 * forward(p.turn);

    next.board[move.from] = '.';
    next.board[move.to] = move.promotion == '.' ? piece : pieceFor(move.promotion, p.turn);
    next.enPassant = -1;
    next.halfmoves = (kind == 'p' || target != '.') ? 0 : p.halfmoves + 1;
    if (kind == 'p') {
        if (move.to == p.enPassant && target == '.') next.board[move.to - step] = '.';
        if (distance == 2 * step) next.enPassant = move.from + step;
    }
    if (kind == 'k') {
        const unsigned rights = p.turn == Color::white ? whiteShort | whiteLong : blackShort | blackLong;
        next.castling &= ~rights;
        if (std::abs(distance) == 2) {
            const int rook = move.from + (distance > 0 ? 3 : -4);
            next.board[move.from + distance / 2] = p.board[rook];
            next.board[rook] = '.';
        }
    }
    if (kind == 'r') next.castling &= ~rookRight(move.from);
    if (lower(target) == 'r') next.castling &= ~rookRight(move.to);
    next.turn = opposite(p.turn);
    return next;
}

std::optional<Move> parseMove(std::string_view input) {
    if (input.size() != 4 && input.size() != 5) return std::nullopt;
    const auto square = [&](std::size_t index) {
        const char file = lower(input[index]);
        const char rank = input[index + 1];
        return file >= 'a' && file <= 'h' && rank >= '1' && rank <= '8'
            ? (rank - '1') * 8 + (file - 'a') : -1;
    };
    const int from = square(0), to = square(2);
    if (from < 0 || to < 0 || from == to) return std::nullopt;
    if (input.size() == 4) return Move{from, to};
    const char promotion = lower(input[4]);
    if (std::string_view("qrbn").find(promotion) == std::string_view::npos) return std::nullopt;
    return Move{from, to, promotion};
}

std::string notation(Move move) {
    std::string text{
        static_cast<char>('a' + move.from % 8), static_cast<char>('1' + move.from / 8),
        static_cast<char>('a' + move.to % 8), static_cast<char>('1' + move.to / 8)};
    if (move.promotion != '.') text += move.promotion;
    return text;
}

bool insufficientMaterial(const Position& p) {
    int knights = 0, bishops = 0, bishopSquareColor = -1;
    for (int square = 0; square < 64; ++square) {
        switch (lower(p.board[square])) {
            case '.':
            case 'k': break;
            case 'n': ++knights; break;
            case 'b': {
                ++bishops;
                const int squareColor = (square / 8 + square % 8) % 2;
                if (bishopSquareColor >= 0 && squareColor != bishopSquareColor) return false;
                bishopSquareColor = squareColor;
                break;
            }
            default: return false;
        }
    }
    return knights == 0 || (knights == 1 && bishops == 0);
}

std::string repetitionKey(const Position& p, const std::vector<Move>& legal) {
    std::string key(p.board.begin(), p.board.end());
    key += p.turn == Color::white ? 'w' : 'b';
    key += static_cast<char>('A' + p.castling);
    if (p.enPassant >= 0 && std::any_of(legal.begin(), legal.end(), [&](Move move) {
        return move.to == p.enPassant && move.from % 8 != move.to % 8 &&
               lower(p.board[move.from]) == 'p';
    })) {
        key += static_cast<char>('!' + p.enPassant);
    }
    return key;
}

Result result(const Position& p, const std::vector<Move>& legal, int repetitions) {
    if (legal.empty()) return inCheck(p, p.turn) ? Result::checkmate : Result::stalemate;
    if (insufficientMaterial(p)) return Result::materialDraw;
    if (repetitions >= 3) return Result::repetitionDraw;
    if (p.halfmoves >= 100) return Result::moveCountDraw;
    return Result::ongoing;
}

}  // namespace chess
