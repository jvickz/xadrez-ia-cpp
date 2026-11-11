// Integration tests exercise the same controller and renderer as the desktop entry point.
#define WinMain ChessDesktopEntryPoint
#include "gui.cpp"
#undef WinMain
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstdlib>

void verify(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Replace only the external speaker boundary; controller, selection and PCM are real.
std::vector<std::vector<unsigned char>> heard;
int stops = 0;
BOOL WINAPI recordSound(LPCWSTR memory, HMODULE, DWORD flags) {
    if (!memory) { ++stops; return TRUE; }
    verify((flags & (SND_ASYNC | SND_MEMORY | SND_NODEFAULT)) ==
           (SND_ASYNC | SND_MEMORY | SND_NODEFAULT), "audio must be async with no system fallback");
    const auto* data = reinterpret_cast<const unsigned char*>(memory);
    const unsigned size = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    heard.emplace_back(data, data + size + 8);
    return TRUE;
}

void finishMotion(Game& game) {
    game.animationStart -= std::chrono::seconds(1);
    game.tickAnimation();
}

void settle(Game& game);

void soundTests(Game& game) {
    const auto toggle = [&] { SendMessageW(game.window, WM_COMMAND, MAKEWPARAM(105, BN_CLICKED), 0); };
    heard.clear();
    game.click(12);
    game.click(36); // Illegal e2-e5 must stay silent.
    verify(heard.empty() && game.history.empty(), "invalid move must be silent");
    game.play(*parseMove("e2e4"));
    verify(heard.empty(), "sound waits for the piece to land");
    game.undoTurn();
    game.tickAnimation();
    verify(heard.empty(), "undo cancels pending sound");
    toggle();
    verify(!game.audio.enabled(), "sound button disables audio");
    game.reset();
    verify(!game.audio.enabled(), "new match preserves mute");
    game.depth = 1;
    game.play(*parseMove("e2e4"));
    settle(game);
    verify(heard.empty(), "human and AI moves respect mute");
    toggle();
    game.undoTurn();
    game.play(*parseMove("e2e4"));
    settle(game);
    verify(heard.size() == 2, "one sound per human and AI move");
    const auto moveSound = heard.front();
    const int beforeStop = stops;
    toggle();
    verify(stops > beforeStop, "mute stops audio already playing");
    game.reset();
    toggle();

    const auto cue = [&](Position start, const char* text, SoundCue expected) {
        game.Match::reset(start);
        game.human = expected == SoundCue::victory ? start.turn :
                     start.turn == Color::white ? Color::black : Color::white;
        const Move move = *parseMove(text);
        const auto moves = legalMoves(start);
        verify(std::find(moves.begin(), moves.end(), move) != moves.end(), "sound fixture move must be legal");
        heard.clear();
        game.play(move);
        finishMotion(game);
        if (heard.size() != 1 || heard.front() != soundData(expected))
            throw std::runtime_error(std::string("wrong or duplicate sound for ") + text +
                ", check=" + std::to_string(game.check) + ", outcome=" + std::to_string(static_cast<int>(game.outcome)));
        game.tickAnimation();
        game.refresh();
        verify(heard.size() == 1, "refresh and animation ticks must not repeat sound");
    };
    Position capture = initialPosition();
    capture = afterMove(capture, *parseMove("e2e4"));
    capture = afterMove(capture, *parseMove("d7d5"));
    cue(capture, "e4d5", SoundCue::capture);
    verify(heard.front() != moveSound, "capture must be distinct from move");
    Position ep = initialPosition();
    for (const auto text : {"e2e4", "a7a6", "e4e5", "d7d5"}) ep = afterMove(ep, *parseMove(text));
    cue(ep, "e5d6", SoundCue::capture);
    Position check = initialPosition();
    check.board.fill('.'); check.castling = 0;
    check.board[0] = 'K'; check.board[63] = 'k'; check.board[6] = 'R'; check.board[62] = 'n';
    cue(check, "g1g8", SoundCue::check); // Capture with check: only check sound.
    Position loss = initialPosition();
    for (const auto text : {"f2f3", "e7e5", "g2g4"}) loss = afterMove(loss, *parseMove(text));
    cue(loss, "d8h4", SoundCue::defeat); // Mate overrides check.
    Position win = check;
    win.board.fill('.'); win.board[53] = 'K'; win.board[63] = 'k'; win.board[46] = 'Q';
    cue(win, "g6g7", SoundCue::victory);
    Position draw = check;
    draw.board[6] = '.'; draw.board[63] = '.'; draw.board[62] = 'k'; draw.board[54] = '.';
    draw.board[9] = 'B'; draw.board[18] = 'n';
    cue(draw, "b2c3", SoundCue::draw);
    game.human = Color::white;
    game.reset();
    heard.clear();
}

void settle(Game& game) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (game.moving || game.search.valid()) {
        verify(std::chrono::steady_clock::now() < deadline, "search or animation did not finish");
        game.tickAnimation();
        game.finishSearch();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void render(Game& game, const wchar_t* path) {
    HDC dc = CreateCompatibleDC(nullptr);
    HDC screen = GetDC(nullptr);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 1064, 811);
    ReleaseDC(nullptr, screen);
    const auto previous = SelectObject(dc, bitmap);
    fill(dc, {0, 0, 1064, 811}, background);
    const auto l = layout(game.window);
    game.drawBoard(dc, l);
    game.drawPanel(dc, l);
    game.drawPlayer(dc, l, true);
    game.drawPlayer(dc, l, false);
    game.drawAnimation(dc, l);
    SelectObject(dc, previous);
    Gdiplus::Bitmap image(bitmap, nullptr);
    UINT count = 0, bytes = 0;
    Gdiplus::GetImageEncodersSize(&count, &bytes);
    std::vector<unsigned char> storage(bytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(storage.data());
    Gdiplus::GetImageEncoders(count, bytes, encoders);
    bool saved = false;
    for (UINT i = 0; i < count; ++i)
        if (std::wcscmp(encoders[i].MimeType, L"image/png") == 0)
            saved = image.Save(path, &encoders[i].Clsid, nullptr) == Gdiplus::Ok;
    DeleteObject(bitmap);
    DeleteDC(dc);
    verify(saved, "render preview failed");
}

#include "savegame_tests.hpp"

void pointerTests(Game& game) {
    const auto center = [&](int square) {
        const auto rect = game.squareRect(square, layout(game.window));
        return POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    };
    const auto send = [&](UINT message, POINT point) {
        SendMessageW(game.window, message, message == WM_LBUTTONUP ? 0 : MK_LBUTTON,
                     MAKELPARAM(point.x, point.y));
    };
    const auto drag = [&](int from, POINT target) {
        send(WM_LBUTTONDOWN, center(from));
        send(WM_MOUSEMOVE, target);
        verify(game.dragging, "mouse movement starts drag");
        send(WM_LBUTTONUP, target);
    };
    game.reset();
    game.failedSearch = true;
    drag(12, center(28));
    verify(game.history.size() == 1 && game.position.board[28] == 'P' &&
           loadGameFile(game.saveFile).match.history.size() == 1, "legal drag moves and autosaves");
    game.undoTurn();
    drag(12, center(36));
    verify(game.history.empty() && game.position.board[12] == 'P', "illegal drag restores origin");
    drag(12, {0, 0});
    verify(game.history.empty() && !game.dragging && GetCapture() != game.window,
           "outside drop preserves position and releases capture");
    send(WM_LBUTTONDOWN, center(12));
    send(WM_MOUSEMOVE, center(28));
    SendMessageW(game.window, WM_KEYDOWN, VK_ESCAPE, 0);
    send(WM_LBUTTONUP, center(28));
    verify(game.history.empty() && loadGameFile(game.saveFile).match.history.empty(),
           "Escape cancels drag without changing save");
    // Exercise reversed coordinates with the human playing black.
    SendMessageW(game.colorChoice, CB_SETCURSEL, 1, 0);
    SendMessageW(game.levelChoice, CB_SETCURSEL, 0, 0);
    game.reset();
    settle(game);
    verify(game.history.size() == 1 && game.humanTurn(), "white AI opens for black human");
    drag(52, center(36));
    verify(game.history.size() == 2 && game.position.board[36] == 'p', "drag on rotated board");
    game.undoTurn();
    SendMessageW(game.colorChoice, CB_SETCURSEL, 0, 0);
    SendMessageW(game.levelChoice, CB_SETCURSEL, 1, 0);
    game.reset();
    std::filesystem::remove(game.saveFile);
    game.refresh();
    heard.clear();
}

int main(int argc, char** argv) {
    if (argc == 3 && (std::string(argv[1]) == "--write-save-test" || std::string(argv[1]) == "--read-save-test" ||
                      std::string(argv[1]) == "--read-autosave-test")) {
        try {
            const auto path = std::filesystem::u8path(argv[2]);
            if (std::string(argv[1]) == "--write-save-test") saveGameFile(path, savedFixture());
            else if (std::string(argv[1]) == "--read-autosave-test") {
                const auto saved = loadGameFile(path);
                verify(saved.match.history.size() == 1 && saved.match.position.board[12] == '.' &&
                       saved.match.position.board[28] == 'P' && saved.match.position.turn == Color::black,
                       "move must survive abrupt exit without manual save or close handler");
            } else verifySavedFixture(loadGameFile(path));
            std::cout << "Save persistence across independent processes: OK\n";
            return 0;
        } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (argc == 2 && std::string(argv[1]) == "--audio-smoke") {
        ChessAudio audio;
        for (const auto cue : {SoundCue::move, SoundCue::capture, SoundCue::check,
                               SoundCue::victory, SoundCue::defeat, SoundCue::draw}) {
            const auto& pcm = soundData(cue);
            if (!audio.play(cue)) { std::cerr << "Windows audio playback failed\n"; return 1; }
            std::this_thread::sleep_for(std::chrono::milliseconds((pcm.size() - 44) * 1000 / 44100 + 30));
        }
        std::cout << "Windows accepted all six PCM effects: OK\n";
        return 0;
    }
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    int resultCode = 0;
    try {
        const bool abruptExit = argc == 3 && std::string(argv[1]) == "--write-autosave-test";
        const auto testSave = abruptExit ? std::filesystem::u8path(argv[2]) : std::filesystem::temp_directory_path() /
            (L"xadrez-save-test-" + std::to_wstring(GetCurrentProcessId())) / L"partida-ação.sav";
        if (!abruptExit) storageTests(testSave);
        Game game(recordSound, testSave);
        WNDCLASSW cls{};
        cls.lpfnWndProc = windowProc;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"ChessControllerTests";
        RegisterClassW(&cls);
        HWND window = CreateWindowExW(0, cls.lpszClassName, L"Test", WS_OVERLAPPEDWINDOW,
            0, 0, 1080, 850, nullptr, nullptr, cls.hInstance, &game);
        verify(window != nullptr, "test window creation");
        if (abruptExit) {
            game.click(12);
            game.click(28);
            verify(game.history.size() == 1 && game.moving, "move applied before abrupt exit");
            std::_Exit(0); // No manual save, WM_CLOSE or destructors: next process checks disk.
        }
        verify(GetDlgItem(window, 106) && GetDlgItem(window, 107),
               "save and continue controls must be available");
        for (bool smallIcon : {true, false}) {
            const auto actual = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON,
                smallIcon ? ICON_SMALL : ICON_BIG, 0));
            const auto expected = chessAppIcon(cls.hInstance, smallIcon);
            verify(actual != nullptr && actual == expected, "window must use the embedded chess logo");
            ICONINFO info{};
            verify(GetIconInfo(actual, &info), "embedded icon must be readable");
            BITMAP bitmap{};
            GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap);
            if (info.hbmColor) DeleteObject(info.hbmColor);
            if (info.hbmMask) DeleteObject(info.hbmMask);
            verify(bitmap.bmWidth == GetSystemMetrics(smallIcon ? SM_CXSMICON : SM_CXICON),
                   "window icon must have the requested small or large size");
        }
        verify(GetDlgItem(window, 105) != nullptr, "sound toggle must be available during a match");
        pointerTests(game);
        autosaveControllerTests(game);
        persistenceControllerTests(game);
        soundTests(game);
        game.depth = 4;
        game.play(*parseMove("e2e4"));
        verify(game.moving, "human move animation starts");
        game.undoTurn();
        verify(game.history.empty() && !game.moving, "undo cancels moving piece");
        game.play(*parseMove("e2e4"));
        while (game.moving) {
            game.tickAnimation();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        verify(game.search.valid(), "AI background search started");
        game.undoTurn();
        settle(game);
        verify(game.history.empty() && game.position.board == initialPosition().board,
               "stale AI result changed undone position");
        game.depth = 1;
        game.play(*parseMove("e2e4"));
        settle(game);
        verify(game.history.size() == 2, "AI response and animation complete");
        game.undoTurn();
        verify(game.history.empty() && game.captures[0].empty() && game.captures[1].empty(),
               "undo restores full turn");
        Position mate = initialPosition();
        mate.board.fill('.');
        mate.board[63] = 'k';
        mate.board[53] = 'K';
        mate.board[54] = 'Q'; // Qg7, Kf7, Kh8: checkmate.
        mate.turn = Color::black;
        mate.castling = 0;
        game.Match::reset(mate);
        game.refresh();
        verify(game.outcome == Result::checkmate && !game.humanTurn(), "victory state");
        game.resultSeconds = 0.7;
        render(game, L"build/victory-preview.png");
        game.human = Color::black;
        verify(game.humanTurn(), "defeat state");
        game.resultSeconds = 0.7;
        render(game, L"build/defeat-preview.png");
        game.human = Color::white;
        game.Match::reset();
        for (const auto text : {"f2f3", "e7e5", "g2g4", "d8h4"})
            game.Match::apply(*parseMove(text));
        game.refresh();
        verify(game.outcome == Result::checkmate && game.humanTurn(), "loss after played moves");
        game.undoTurn();
        verify(game.outcome == Result::ongoing && game.history.size() == 2 &&
               game.position.board[14] == 'P', "undo dismisses defeat and restores the last human turn");
        game.reset();
        verify(game.outcome == Result::ongoing && !game.moving, "reset clears result animation");
        DestroyWindow(window);
        std::cout << "Click/drag, rotated board, autosave, save/load, atomic replacement, special moves, icons, undo, stale AI, animations and sound: OK\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        resultCode = 1;
    }
    Gdiplus::GdiplusShutdown(token);
    return resultCode;
}
