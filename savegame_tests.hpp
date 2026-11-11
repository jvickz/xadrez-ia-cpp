#pragma once
#include <fstream>

inline chess::SavedGame savedFixture() {
    chess::SavedGame saved;
    saved.depth = 4;
    saved.sound = false;
    for (const auto text : {"e2e4", "d7d5", "e4d5", "d8d5", "b1c3", "d5d8"})
        saved.match.apply(*chess::parseMove(text));
    return saved;
}

inline void verifySavedFixture(chess::SavedGame saved) {
    using namespace chess;
    verify(saved.human == Color::white && saved.depth == 4 && !saved.sound, "saved preferences");
    verify(saved.match.history.size() == 6 && saved.match.position.turn == Color::white &&
           saved.match.position.board[18] == 'N' && saved.match.position.board[59] == 'q' &&
           saved.match.position.board[12] == '.' && saved.match.position.board[51] == '.',
           "saved board and side to move");
    verify(saved.match.captures[0] == std::vector<char>{'p'} &&
           saved.match.captures[1] == std::vector<char>{'P'}, "saved captures for both sides");
    verify(saved.match.undo(Color::white) && saved.match.history.size() == 4 &&
           saved.match.position.board[1] == 'N' && saved.match.position.board[35] == 'q',
           "undo after reopening restores both plies");
}

inline void storageTests(const std::filesystem::path& file) {
    using namespace chess;
    const auto roundTrip = [](const SavedGame& saved) { return decodeSavedGame(encodeSavedGame(saved)); };
    saveGameFile(file, savedFixture());
    verifySavedFixture(loadGameFile(file));

    const HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    verify(lock != INVALID_HANDLE_VALUE, "lock previous save for atomic-write test");
    auto changed = savedFixture(); changed.depth = 1;
    bool failed = false;
    try { saveGameFile(file, changed); } catch (const std::exception&) { failed = true; }
    CloseHandle(lock);
    verify(failed && loadGameFile(file).depth == 4, "failed replacement must preserve previous save");
    saveGameFile(file, changed);
    verify(loadGameFile(file).depth == 1, "successful save replaces previous slot");

    SavedGame special;
    for (const auto text : {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6", "e1g1"})
        special.match.apply(*parseMove(text));
    special = roundTrip(special);
    verify(special.match.position.board[6] == 'K' && special.match.position.board[5] == 'R' &&
           !(special.match.position.castling & (whiteShort | whiteLong)), "castling preserved after load");
    verify(special.match.undo(Color::white) && (special.match.position.castling & whiteShort),
           "undo loaded castling restores rights");
    special.match.reset();
    for (const auto text : {"e2e4", "a7a6", "e4e5", "d7d5"}) special.match.apply(*parseMove(text));
    special = roundTrip(special);
    verify(special.match.position.enPassant == 43, "en passant target survives save");
    special.match.apply(*parseMove("e5d6"));
    special = roundTrip(special);
    verify(special.match.captures[0] == std::vector<char>{'p'} && special.match.position.board[35] == '.',
           "loaded en passant records its captured pawn");
    verify(special.match.undo(Color::white) && special.match.position.enPassant == 43,
           "undo loaded en passant restores opportunity");

    special.match.reset();
    for (const auto text : {"a2a4", "h7h5", "a4a5", "h5h4", "a5a6", "h4h3", "a6b7", "h3g2", "b7a8q", "g2h1q"})
        special.match.apply(*parseMove(text));
    special = roundTrip(special);
    verify(special.match.position.board[56] == 'Q' && special.match.position.board[7] == 'q' &&
           special.match.captures[0].size() == 2 && special.match.captures[1].size() == 2,
           "promotion and promotion captures survive reopening");
    special.match.reset();
    special.human = Color::black;
    for (const auto text : {"g1f3", "g8f6", "f3g1", "f6g8"}) special.match.apply(*parseMove(text));
    special = roundTrip(special);
    const auto key = repetitionKey(special.match.position, legalMoves(special.match.position));
    verify(special.human == Color::black && special.match.repetitions.at(key) == 2 &&
           special.match.position.halfmoves == 4, "orientation, repetitions and halfmove counter survive load");

    const auto encoded = encodeSavedGame(savedFixture());
    for (const auto& bad : {std::string{}, encoded.substr(0, encoded.size() - 5), encoded + "extra",
                           std::string("XADREZ_IA_SAVE 2\n"),
                           std::string("XADREZ_IA_SAVE 1 human 0 depth 4 sound 1 moves 1 e2e5 END"),
                           std::string("XADREZ_IA_SAVE 1 human 0 depth 3 sound 1 moves 0 END"),
                           std::string("XADREZ_IA_SAVE 1 human 0 depth 4 sound 1 moves -1 END"),
                           std::string("XADREZ_IA_SAVE 1 human 0 depth 4 sound 1 moves 999999 END")}) {
        bool rejected = false;
        try { (void)decodeSavedGame(bad); } catch (const std::exception&) { rejected = true; }
        verify(rejected, "damaged, unsupported or illegal save must be rejected");
    }
    std::filesystem::remove(file);
}

inline void autosaveControllerTests(Game& game) {
    using namespace chess;
    game.reset();
    game.depth = 1;
    game.play(*parseMove("e2e4"));
    verify(std::filesystem::exists(game.saveFile), "human move must autosave before animation finishes");
    auto saved = loadGameFile(game.saveFile);
    verify(saved.match.history.size() == 1 && saved.match.position.board[28] == 'P' &&
           saved.match.position.turn == Color::black, "autosave must preserve pending AI turn");
    settle(game);
    saved = loadGameFile(game.saveFile);
    verify(saved.match.history.size() == 2 && saved.match.position.board == game.position.board &&
           saved.match.position.turn == Color::white, "AI response must autosave");
    game.undoTurn();
    verify(loadGameFile(game.saveFile).match.history.empty(), "undo must update autosave");

    const HANDLE lock = CreateFileW(game.saveFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    verify(lock != INVALID_HANDLE_VALUE, "lock autosave for failure test");
    game.play(*parseMove("d2d4"));
    verify(game.history.size() == 1 && game.saveFailed &&
           loadGameFile(game.saveFile).match.history.empty(),
           "failed autosave must keep previous save and allow the move");
    CloseHandle(lock);
    game.undoTurn();
    game.reset();
    game.failedSearch = true; // Play a deterministic mate without starting an AI worker.
    for (const auto text : {"f2f3", "e7e5", "g2g4", "d8h4"}) game.play(*parseMove(text));
    saved = loadGameFile(game.saveFile);
    verify(saved.match.history.size() == 4 &&
           result(saved.match.position, legalMoves(saved.match.position), 1) == Result::checkmate,
           "final move must autosave the result");
    game.reset(); // Startup-style reset must not overwrite a saved match.
    verify(loadGameFile(game.saveFile).match.history.size() == 4, "startup preserves the existing save");
    SendMessageW(game.window, WM_COMMAND, MAKEWPARAM(101, BN_CLICKED), 0);
    verify(loadGameFile(game.saveFile).match.history.empty(), "explicit new game must update autosave");
    std::filesystem::remove(game.saveFile);
    game.refresh();
    heard.clear();
}

inline void persistenceControllerTests(Game& game) {
    using namespace chess;
    verify(!IsWindowEnabled(game.continueButton), "continue disabled before first save");
    static_cast<Match&>(game) = savedFixture().match;
    game.depth = 4;
    if (game.audio.enabled()) game.toggleSound();
    heard.clear();
    SendMessageW(game.window, WM_COMMAND, MAKEWPARAM(106, BN_CLICKED), 0);
    verify(IsWindowEnabled(game.continueButton) && std::filesystem::exists(game.saveFile),
           "save button writes a real file and enables continue");
    game.reset();
    verify(game.history.empty() && IsWindowEnabled(game.continueButton), "new game preserves saved slot");
    SendMessageW(game.window, WM_COMMAND, MAKEWPARAM(107, BN_CLICKED), 0);
    verifySavedFixture({static_cast<Match&>(game), game.human, game.depth, game.audio.enabled()});
    verify(heard.empty(), "loading must not replay old sounds");
    game.undoTurn();
    verify(game.history.size() == 4, "controller undo works after loading");

    game.reset();
    game.depth = 4;
    game.play(*parseMove("e2e4"));
    verify(game.saveMatch(), "save while move animation is active");
    finishMotion(game);
    verify(game.search.valid(), "background AI is active before load");
    verify(game.continueMatch(), "restore while previous AI is calculating");
    settle(game);
    verify(game.history.size() == 2 && game.humanTurn(), "only one fresh AI reply after restoring its turn");
    game.undoTurn();
    verify(game.history.empty(), "undo after AI resumed from a save");

    game.play(*parseMove("d2d4"));
    const auto before = game.position;
    { std::ofstream broken(game.saveFile, std::ios::binary); broken << "damaged"; }
    verify(!game.continueMatch() && game.position.board == before.board && game.history.size() == 1,
           "invalid file must leave active match untouched");
    game.undoTurn();
    game.reset();
    if (!game.audio.enabled()) game.toggleSound();
    std::filesystem::remove(game.saveFile);
    game.refresh();
    heard.clear();
}
