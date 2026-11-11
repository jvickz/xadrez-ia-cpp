#include "chess.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using namespace chess;

std::string readCommand(const char* prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return "sair";
    std::istringstream words(line);
    std::string command, extra;
    words >> command;
    if (words >> extra) return line;
    return command;
}

void showBoard(const Position& p, Color player) {
    const bool white = player == Color::white;
    std::cout << '\n';
    for (int row = 0; row < 8; ++row) {
        const int rank = white ? 7 - row : row;
        std::cout << rank + 1 << "  ";
        for (int col = 0; col < 8; ++col) {
            const int file = white ? col : 7 - col;
            std::cout << p.board[8 * rank + file] << ' ';
        }
        std::cout << '\n';
    }
    std::cout << (white ? "   a b c d e f g h\n" : "   h g f e d c b a\n");
}

void help() {
    std::cout << "Digite origem e destino, como e2e4. Para rocar, digite o lance do rei (ex.: e1g1).\n"
                 "Para promover, acrescente q (dama), r (torre), b (bispo) ou n (cavalo), como e7e8q.\n"
                 "Peças: K rei, Q dama, R torre, B bispo, N cavalo, P peão. Maiúsculas são brancas.\n"
                 "Comandos: lances, ajuda, sair.\n";
}

void showResult(Result outcome, bool humanToMove) {
    switch (outcome) {
        case Result::checkmate:
            std::cout << (humanToMove ? "Xeque-mate. A IA venceu.\n" : "Xeque-mate. Você venceu!\n");
            break;
        case Result::stalemate: std::cout << "Empate por afogamento.\n"; break;
        case Result::materialDraw: std::cout << "Empate por material insuficiente.\n"; break;
        case Result::repetitionDraw: std::cout << "Empate por repetição de posição.\n"; break;
        case Result::moveCountDraw: std::cout << "Empate: 50 lances de cada lado sem captura ou movimento de peão.\n"; break;
        case Result::ongoing: break;
    }
}

std::optional<Move> readMove(const std::vector<Move>& legal) {
    for (;;) {
        const auto input = readCommand("Seu lance > ");
        if (input == "sair") return std::nullopt;
        if (input == "ajuda") {
            help();
        } else if (input == "lances") {
            for (Move move : legal) std::cout << notation(move) << ' ';
            std::cout << '\n';
        } else {
            const auto move = parseMove(input);
            if (move && std::find(legal.begin(), legal.end(), *move) != legal.end()) return move;
            std::cout << "Lance inválido. Digite lances para ver as opções.\n";
        }
    }
}

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << "Xadrez contra a IA\n";
    Color player;
    for (;;) {
        const auto choice = readCommand("Cor [b = brancas, p = pretas; Enter = brancas] > ");
        if (choice == "sair") return 0;
        if (choice.empty() || choice == "b" || choice == "p") {
            player = choice == "p" ? Color::black : Color::white;
            break;
        }
        std::cout << "Escolha b ou p.\n";
    }

    int depth = 2;
    for (;;) {
        const auto choice = readCommand("Nível [1 = fácil, 2 = médio, 3 = difícil; Enter = médio] > ");
        if (choice == "sair") return 0;
        if (choice.empty() || choice == "2") break;
        if (choice == "1" || choice == "3") {
            depth = choice == "1" ? 1 : 4;
            break;
        }
        std::cout << "Escolha 1, 2 ou 3.\n";
    }
    help();

    Position p = initialPosition();
    std::unordered_map<std::string, int> seen;
    for (;;) {
        showBoard(p, player);
        const bool humanToMove = p.turn == player;
        const auto legal = legalMoves(p);
        const auto outcome = result(p, legal, ++seen[repetitionKey(p, legal)]);
        if (outcome != Result::ongoing) {
            showResult(outcome, humanToMove);
            break;
        }
        if (inCheck(p, p.turn)) std::cout << (humanToMove ? "Você está em xeque.\n" : "A IA está em xeque.\n");
        const auto move = humanToMove ? readMove(legal) : chooseMove(p, depth);
        if (!move) break;
        if (!humanToMove) std::cout << "IA jogou: " << notation(*move) << '\n';
        p = afterMove(p, *move);
    }
}
