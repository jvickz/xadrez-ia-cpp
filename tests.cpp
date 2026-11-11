#include "chess.hpp"
#include "match.hpp"

#include <algorithm>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace chess;

void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

Position fixture(std::string_view diagram, Color turn = Color::white,
                 unsigned castling = 0, int enPassant = -1) {
    Position p;
    p.board.fill('.');
    p.turn = turn;
    p.castling = castling;
    p.enPassant = enPassant;
    int rank = 7, file = 0;
    for (char c : diagram) {
        if (c == '/') { --rank; file = 0; }
        else if (c >= '1' && c <= '8') file += c - '0';
        else p.board.at(rank * 8 + file++) = c;
    }
    return p;
}

bool allows(const Position& p, std::string_view input) {
    auto moves = legalMoves(p);
    return std::find(moves.begin(), moves.end(), parseMove(input).value()) != moves.end();
}

Position play(const Position& p, std::string_view input) {
    require(allows(p, input), "fixture contains an illegal move");
    return afterMove(p, parseMove(input).value());
}

std::uint64_t perft(const Position& p, int depth) {
    if (depth == 0) return 1;
    std::uint64_t total = 0;
    for (Move move : legalMoves(p)) total += perft(afterMove(p, move), depth - 1);
    return total;
}

// Enumerate every defence independently of the AI's evaluation.
bool forcesMate(const Position& p, Color attacker, int remaining) {
    const auto moves = legalMoves(p);
    if (moves.empty()) return p.turn != attacker && inCheck(p, p.turn);
    if (remaining == 0) return false;
    const bool attackersTurn = p.turn == attacker;
    for (Move move : moves) {
        const bool wins = forcesMate(afterMove(p, move), attacker, remaining - 1);
        if (wins == attackersTurn) return attackersTurn;
    }
    return !attackersTurn;
}

int main() {
    try {
        Match match;
        const auto apply = [&](std::string_view input) {
            require(allows(match.position, input), "match fixture has legal moves");
            match.apply(*parseMove(input));
        };
        require(!match.undo(Color::white), "no undo before human move");
        apply("e2e4");
        require(!match.undo(Color::black), "black cannot undo the AI opening");
        apply("d7d5");
        apply("e4d5");
        apply("d8d5");
        require(match.captures[0].size() == 1 && match.captures[1].size() == 1, "captures recorded");
        require(match.undo(Color::white), "undo complete human and AI turn");
        require(match.history.size() == 2 && match.position.board[28] == 'P' &&
                match.position.board[35] == 'p' && match.captures[0].empty() &&
                match.captures[1].empty(), "undo restores both captured pieces and lists");
        require(match.position.enPassant == 43, "undo restores en passant rights");
        apply("e4d5");
        require(match.undo(Color::white) && match.history.size() == 2, "undo before AI replies");
        require(match.undo(Color::black) && match.history.size() == 1 &&
                match.position.turn == Color::black, "undo black restores its decision point");
        match.reset();
        for (auto move : {"g1f3", "g8f6", "f3g1", "f6g8"}) apply(move);
        auto key = repetitionKey(match.position, legalMoves(match.position));
        require(match.repetitions[key] == 2, "repetition tracked");
        match.undo(Color::white);
        require(match.repetitions[repetitionKey(initialPosition(), legalMoves(initialPosition()))] == 1,
                "undo removes reverted repetitions");
        for (auto move : {"f3g1", "f6g8"}) apply(move);
        require(match.repetitions[key] == 2, "replaying does not inflate repetition count");
        match.reset(fixture("4k3/8/8/8/8/8/8/R3K2R", Color::white, 3));
        apply("e1g1");
        match.undo(Color::white);
        require(match.position.castling == 3 && match.position.board[7] == 'R' &&
                match.position.board[4] == 'K', "undo restores castling and pieces");
        match.reset(fixture("4k3/8/8/3pP3/8/8/8/4K3", Color::white, 0, 43));
        apply("e5d6");
        match.undo(Color::white);
        require(match.position.board[35] == 'p' && match.position.board[36] == 'P' &&
                match.captures[0].empty(), "undo restores en passant capture");
        match.reset(fixture("r6k/1P6/8/8/8/8/8/4K3"));
        apply("b7a8q");
        match.undo(Color::white);
        require(match.position.board[49] == 'P' && match.position.board[56] == 'r' &&
                match.captures[0].empty(), "undo restores promotion capture");
        match.reset();
        require(match.history.empty() && match.past.empty() && match.captures[0].empty(),
                "new match clears undo history");
        auto start = initialPosition();
        require(perft(start, 1) == 20 && perft(start, 2) == 400 &&
                perft(start, 3) == 8902 && perft(start, 4) == 197281, "initial perft");
        auto kiwipete = fixture("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R",
                                 Color::white, 15);
        require(perft(kiwipete, 1) == 48 && perft(kiwipete, 2) == 2039 &&
                perft(kiwipete, 3) == 97862, "Kiwipete perft");

        auto castle = fixture("4k3/8/8/8/8/8/8/R3K2R", Color::white, 3);
        require(allows(castle, "e1c1") && allows(castle, "e1g1"), "both castles");
        auto castled = play(castle, "e1g1");
        require(castled.board[5] == 'R' && castled.board[7] == '.' &&
                castled.castling == 0, "castling relocates rook and clears rights");
        auto attacked = fixture("4kr2/8/8/8/8/8/8/4K2R", Color::white, whiteShort);
        require(!allows(attacked, "e1g1"), "cannot castle through attack");
        require(!allows(fixture("4r1k1/8/8/8/8/8/8/4K2R", Color::white, whiteShort), "e1g1"),
                "cannot castle out of check");
        require(!allows(fixture("4k1r1/8/8/8/8/8/8/4K2R", Color::white, whiteShort), "e1g1"),
                "cannot castle into check");
        require(!allows(initialPosition(), "e1g1"), "cannot castle through pieces");
        require(!allows(fixture("4k3/8/8/8/8/8/8/4K2R"), "e1g1"), "cannot castle without rights");
        for (auto input : {"e8g8", "e8c8"}) {
            auto blackCastle = fixture("r3k2r/8/8/8/8/8/8/4K3", Color::black, blackShort | blackLong);
            require(allows(blackCastle, input), "both black castles");
            const auto next = play(blackCastle, input);
            require(next.board[input[2] == 'g' ? 61 : 59] == 'r', "black castle moves rook");
        }
        auto lostRook = fixture("r3k3/8/8/8/8/8/8/R3K3", Color::black, whiteLong | blackLong);
        require(play(lostRook, "a8a1").castling == 0, "rook capture and move clear rights");

        auto ep = fixture("4k3/8/8/3pP3/8/8/8/4K3", Color::white, 0, 43);
        auto captured = play(ep, "e5d6");
        require(capturedPiece(ep, *parseMove("e5d6")) == 'p', "en passant captured piece");
        require(capturedPiece(initialPosition(), *parseMove("e2e4")) == '.', "quiet move captures nothing");
        require(capturedPiece(castle, *parseMove("e1g1")) == '.', "castling captures nothing");
        auto promotionCapture = fixture("r6k/1P6/8/8/8/8/8/4K3");
        require(capturedPiece(promotionCapture, *parseMove("b7a8q")) == 'r', "promotion capture records rook");
        require(captured.board[35] == '.' && captured.board[43] == 'P', "en passant capture");
        auto pinned = fixture("k3r3/8/8/3pP3/8/8/8/4K3", Color::white, 0, 43);
        require(!allows(pinned, "e5d6"), "pinned en passant");
        auto withoutEp = pinned;
        withoutEp.enPassant = -1;
        require(repetitionKey(pinned, legalMoves(pinned)) ==
                repetitionKey(withoutEp, legalMoves(withoutEp)), "irrelevant en passant in repetition key");
        withoutEp = ep;
        withoutEp.enPassant = -1;
        require(repetitionKey(ep, legalMoves(ep)) != repetitionKey(withoutEp, legalMoves(withoutEp)),
                "available en passant changes repetition key");

        auto promotion = fixture("7k/P7/8/8/8/8/8/4K3");
        for (auto input : {"a7a8q", "a7a8r", "a7a8b", "a7a8n"})
            require(allows(promotion, input), "four promotion options");
        require(play(promotion, "a7a8n").board[56] == 'N', "underpromotion applies");
        require(!parseMove("a9b2") && !parseMove("a7a8k") && !parseMove("a1a1"), "invalid notation");

        auto mate = fixture("7k/8/5KQ1/8/8/8/8/8");
        for (int depth : {1, 2, 4}) {
            const auto best = chooseMove(mate, depth);
            require(best.has_value(), "AI finds a move");
            const auto next = play(mate, notation(*best));
            require(result(next, legalMoves(next), 1) == Result::checkmate, "AI prefers mate in one");
            require(!chooseMove(next, depth), "AI returns no move after mate");
        }
        const auto mateInThree = fixture("7k/8/8/4K3/8/8/8/3Q4");
        for (Color attacker : {Color::white, Color::black}) {
            Position puzzle = mateInThree;
            if (attacker == Color::black) {
                puzzle.turn = Color::black;
                for (int square = 0; square < 64; ++square) {
                    const char piece = mateInThree.board[63 - square];
                    puzzle.board[square] = piece == '.' ? '.' :
                        piece >= 'a' && piece <= 'z' ? piece - 'a' + 'A' : piece - 'A' + 'a';
                }
            }
            const auto best = chooseMove(puzzle, 4);
            require(best.has_value() && allows(puzzle, notation(*best)), "hard AI returns a legal move");
            require(forcesMate(afterMove(puzzle, *best), attacker, 4),
                    "hard AI must find a forced mate in three");
        }
        const auto began = std::chrono::steady_clock::now();
        const auto opening = chooseMove(initialPosition(), 4);
        const auto elapsed = std::chrono::steady_clock::now() - began;
        require(opening.has_value() && allows(initialPosition(), notation(*opening)),
                "hard AI returns a legal completed iteration under its time budget");
        require(elapsed < std::chrono::seconds(5), "hard search exceeded responsiveness budget");
        auto stale = fixture("7k/5Q2/6K1/8/8/8/8/8", Color::black);
        require(result(stale, legalMoves(stale), 1) == Result::stalemate, "stalemate");
        auto minor = fixture("4k3/8/8/8/8/8/8/4KB2");
        require(result(minor, legalMoves(minor), 1) == Result::materialDraw, "king and bishop draw");
        require(result(start, legalMoves(start), 3) == Result::repetitionDraw, "threefold draw");
        start.halfmoves = 100;
        require(result(start, legalMoves(start), 1) == Result::moveCountDraw, "fifty move draw");
        require(play(start, "e2e4").halfmoves == 0 && play(start, "g1f3").halfmoves == 101,
                "halfmove clock reset and increment");
        std::cout << "Regras, perft e IA: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
