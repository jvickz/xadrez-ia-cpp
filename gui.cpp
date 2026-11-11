#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include "chess.hpp"
#include "match.hpp"
#include "pieces.hpp"
#include "app_icon.hpp"
#include "sounds.hpp"
#include "savegame.hpp"
#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <future>
#include <unordered_map>

namespace {
using namespace chess;

constexpr wchar_t windowClass[] = L"XadrezContraIA";
constexpr int newGameButton = 101;
constexpr UINT_PTR searchTimer = 1;
constexpr COLORREF background = RGB(48, 46, 43);
constexpr COLORREF panelColor = RGB(38, 37, 34);
constexpr COLORREF textColor = RGB(242, 241, 226);
constexpr COLORREF muted = RGB(174, 172, 167);
constexpr COLORREF accent = RGB(129, 182, 76);
constexpr COLORREF lightSquare = RGB(238, 238, 210);
constexpr COLORREF darkSquare = RGB(118, 150, 86);
constexpr COLORREF fieldColor = RGB(61, 59, 55);

void rounded(HDC dc, RECT rect, COLORREF color, int radius = 16) {
    const auto brush = CreateSolidBrush(color);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

struct Layout {
    RECT board;
    RECT panel;
    int cell;
};

Layout layout(HWND window) {
    RECT bounds{};
    GetClientRect(window, &bounds);
    const int cell = std::max(1L, std::min(bounds.right - 350, bounds.bottom - 235) / 8);
    const int width = cell * 8 + 306;
    const int x = std::max(34L, (bounds.right - width) / 2);
    const int y = 132 + std::max(0L, (bounds.bottom - 235 - cell * 8) / 2);
    return {{x, y, x + cell * 8, y + cell * 8},
            {x + cell * 8 + 30, y - 66, x + cell * 8 + 306, y + cell * 8 + 84}, cell};
}

void fill(HDC dc, RECT area, COLORREF color) {
    const HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &area, brush);
    DeleteObject(brush);
}

HFONT font(int size, int weight = FW_NORMAL, const wchar_t* face = L"Segoe UI") {
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

void label(HDC dc, HFONT type, std::wstring_view text, RECT area, COLORREF color,
           UINT align = DT_LEFT) {
    const auto old = SelectObject(dc, type);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &area,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | align);
    SelectObject(dc, old);
}

void dot(HDC dc, int x, int y, int radius, COLORREF color) {
    const auto brush = CreateSolidBrush(color);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, x - radius, y - radius, x + radius, y + radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

bool whitePiece(char piece) {
    return piece >= 'A' && piece <= 'Z';
}

struct Game : Match {
    HWND window{};
    HWND colorChoice{}, levelChoice{}, newButton{}, undoButton{}, soundButton{}, saveButton{}, continueButton{};
    ChessAudio audio;
    std::optional<SoundCue> pendingSound;
    std::filesystem::path saveFile;
    std::wstring saveNotice;
    bool saveFailed = false;
    explicit Game(ChessAudio::Playback playback = PlaySoundW,
                  std::filesystem::path file = defaultSavePath()) : audio(playback), saveFile(std::move(file)) {}
    HFONT regular = font(16);
    HFONT small = font(14);
    HFONT bold = font(21, FW_SEMIBOLD);
    HFONT title = font(25, FW_BOLD);
    Color human = Color::white;
    int depth = 2;
    int selected = -1;
    int cursor = -1;
    int pressedSquare = -1;
    int previousSelection = -1;
    bool dragging = false;
    bool dragCandidate = false;
    POINT pressPoint{}, mousePoint{};
    std::vector<Move> legal;
    Result outcome = Result::ongoing;
    bool check = false;
    bool failedSearch = false;
    unsigned generation = 0, searchGeneration = 0;
    std::future<std::optional<Move>> search;

    ~Game() {
        for (HFONT type : {regular, small, bold, title}) DeleteObject(type);
    }

    void controls();
    void placeControls() const;
    void reset(bool persist = false);
    void refresh();
    void play(Move move);
    void undoTurn();
    bool saveMatch(bool automatic = false);
    bool continueMatch();
    void requestContinue();
    void toggleSound();
    void finishSound();
    void tickAnimation();
    void drawAnimation(HDC dc, const Layout& l) const;
    RECT squareRect(int square, const Layout& l) const;
    bool moving = false;
    Position animationBefore = initialPosition();
    Move animationMove{0, 0};
    std::chrono::steady_clock::time_point animationStart{}, resultStart{};
    double resultSeconds = 0;
    void startSearch();
    void finishSearch();
    void click(int square);
    int hitTest(POINT point) const;
    void pointerDown(POINT point);
    void pointerMove(POINT point);
    void pointerUp(POINT point);
    void cancelDrag();
    void drawControl(const DRAWITEMSTRUCT& item) const;
    void keyboard(WPARAM key);
    void paint() const;
    void drawBoard(HDC dc, const Layout& l) const;
    void drawPanel(HDC dc, const Layout& l) const;
    void drawPlayer(HDC dc, const Layout& l, bool opponent) const;
    const wchar_t* status() const;

    bool humanTurn() const { return position.turn == human; }
    int squareAt(int row, int col) const {
        return human == Color::white ? (7 - row) * 8 + col : row * 8 + 7 - col;
    }
};

void Game::controls() {
    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
    const auto combo = [&](int id) {
        return CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL, 0, 0, 1, 160, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    };
    colorChoice = combo(102);
    levelChoice = combo(103);
    newButton = CreateWindowExW(0, L"BUTTON", L"Nova partida", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(newGameButton)), instance, nullptr);
    undoButton = CreateWindowExW(0, L"BUTTON", L"Desfazer", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(104)), instance, nullptr);
    soundButton = CreateWindowExW(0, L"BUTTON", L"Som: ligado", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(105)), instance, nullptr);
    saveButton = CreateWindowExW(0, L"BUTTON", L"Salvar partida", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(106)), instance, nullptr);
    continueButton = CreateWindowExW(0, L"BUTTON", L"Continuar", WS_CHILD | WS_VISIBLE |
        WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1, window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(107)), instance, nullptr);
    for (const auto item : {L"Brancas", L"Pretas"})
        SendMessageW(colorChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    for (const auto item : {L"Fácil", L"Médio", L"Difícil"})
        SendMessageW(levelChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    SendMessageW(colorChoice, CB_SETCURSEL, 0, 0);
    SendMessageW(levelChoice, CB_SETCURSEL, 1, 0);
    for (HWND control : {colorChoice, levelChoice, newButton, undoButton, soundButton, saveButton, continueButton})
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(regular), TRUE);
    for (HWND control : {colorChoice, levelChoice}) {
        SendMessageW(control, CB_SETITEMHEIGHT, -1, 26);
        SendMessageW(control, CB_SETITEMHEIGHT, 0, 28);
    }
    placeControls();
    reset();
    if (IsWindowEnabled(continueButton)) saveNotice = L"Há uma partida salva. Continue!";
}

void Game::placeControls() const {
    const RECT p = layout(window).panel;
    MoveWindow(colorChoice, p.left + 20, p.top + 133, 110, 140, TRUE);
    MoveWindow(levelChoice, p.left + 146, p.top + 133, 110, 140, TRUE);
    MoveWindow(newButton, p.left + 20, p.top + 174, 236, 38, TRUE);
    MoveWindow(undoButton, p.left + 20, p.top + 220, 236, 34, TRUE);
    MoveWindow(soundButton, p.right - 134, 9, 134, 32, TRUE);
    MoveWindow(saveButton, p.left + 20, p.top + 262, 114, 34, TRUE);
    MoveWindow(continueButton, p.left + 142, p.top + 262, 114, 34, TRUE);
}

void Game::reset(bool persist) {
    saveNotice.clear();
    saveFailed = false;
    pendingSound.reset();
    audio.stop();
    cancelDrag();
    ++generation;
    human = SendMessageW(colorChoice, CB_GETCURSEL, 0, 0) == 1 ? Color::black : Color::white;
    const auto level = SendMessageW(levelChoice, CB_GETCURSEL, 0, 0);
    depth = level == 0 ? 1 : level == 2 ? 4 : 2;
    Match::reset();
    moving = false;
    KillTimer(window, 2);
    cursor = -1;
    failedSearch = false;
    if (persist) saveMatch(true);
    refresh();
}

int Game::hitTest(POINT point) const {
    const auto l = layout(window);
    if (!PtInRect(&l.board, point)) return -1;
    return squareAt((point.y - l.board.top) / l.cell, (point.x - l.board.left) / l.cell);
}

void Game::cancelDrag() {
    pressedSquare = -1;
    dragging = dragCandidate = false;
    if (GetCapture() == window) ReleaseCapture();
    InvalidateRect(window, nullptr, FALSE);
}

void Game::pointerDown(POINT point) {
    SetFocus(window);
    cursor = -1;
    if (!humanTurn() || outcome != Result::ongoing || moving) return;
    pressedSquare = hitTest(point);
    if (pressedSquare < 0) return;
    pressPoint = mousePoint = point;
    previousSelection = selected;
    const char piece = position.board[pressedSquare];
    dragCandidate = piece != '.' && whitePiece(piece) == (human == Color::white);
    if (dragCandidate) selected = pressedSquare;
    SetCapture(window);
    InvalidateRect(window, nullptr, FALSE);
}

void Game::pointerMove(POINT point) {
    mousePoint = point;
    if (pressedSquare < 0 || !dragCandidate) return;
    if (std::abs(point.x - pressPoint.x) >= GetSystemMetrics(SM_CXDRAG) ||
        std::abs(point.y - pressPoint.y) >= GetSystemMetrics(SM_CYDRAG)) dragging = true;
    if (dragging) {
        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32649)));
        InvalidateRect(window, nullptr, FALSE);
    }
}

void Game::pointerUp(POINT point) {
    if (pressedSquare < 0) return;
    const int origin = pressedSquare;
    const int target = hitTest(point);
    const int before = previousSelection;
    const bool wasDragging = dragging;
    cancelDrag();
    if (wasDragging) {
        selected = origin;
        const bool valid = std::any_of(legal.begin(), legal.end(), [&](Move move) {
            return move.from == origin && move.to == target;
        });
        if (valid) click(target);
    } else {
        selected = before;
        if (target == origin) click(target);
    }
    InvalidateRect(window, nullptr, FALSE);
}

void Game::drawControl(const DRAWITEMSTRUCT& item) const {
    RECT rect = item.rcItem;
    if (item.CtlID == 105) {
        fill(item.hDC, rect, panelColor);
        rounded(item.hDC, rect, fieldColor, 10);
        label(item.hDC, small, audio.enabled() ? L"Som: ligado" : L"Som: desligado",
              rect, audio.enabled() ? accent : muted, DT_CENTER);
    } else if (item.CtlID == 106 || item.CtlID == 107) {
        fill(item.hDC, rect, panelColor);
        rounded(item.hDC, rect, fieldColor, 10);
        label(item.hDC, small, item.CtlID == 106 ? L"Salvar partida" : L"Continuar", rect,
              item.itemState & ODS_DISABLED ? muted : accent, DT_CENTER);
    } else if (item.CtlID == newGameButton || item.CtlID == 104) {
        fill(item.hDC, rect, panelColor);
        const bool undo = item.CtlID == 104;
        rounded(item.hDC, rect, undo ? fieldColor :
                item.itemState & ODS_SELECTED ? RGB(148, 190, 126) : accent, 10);
        label(item.hDC, regular, undo ? L"Desfazer" : L"Nova partida", rect,
              item.itemState & ODS_DISABLED ? muted : undo ? textColor : background, DT_CENTER);
    } else {
        fill(item.hDC, rect, item.itemState & ODS_SELECTED ? RGB(67, 88, 71) : fieldColor);
        const auto index = static_cast<int>(item.itemID);
        const auto count = SendMessageW(item.hwndItem, CB_GETCOUNT, 0, 0);
        if (index >= 0 && index < count) {
            wchar_t text[32]{};
            SendMessageW(item.hwndItem, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text));
            rect.left += 9;
            label(item.hDC, regular, text, rect, textColor);
        }
    }
    if (item.itemState & ODS_FOCUS) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -3, -3);
        DrawFocusRect(item.hDC, &focus);
    }
}

void Game::refresh() {
    selected = -1;
    legal = legalMoves(position);
    check = inCheck(position, position.turn);
    outcome = result(position, legal, repetitions[repetitionKey(position, legal)]);
    EnableWindow(undoButton, undoTarget(human).has_value());
    std::error_code saveError;
    EnableWindow(continueButton, std::filesystem::is_regular_file(saveFile, saveError));
    if (outcome != Result::ongoing) {
        resultStart = std::chrono::steady_clock::now();
        resultSeconds = 0;
        SetTimer(window, 2, 16, nullptr);
    }
    InvalidateRect(window, nullptr, FALSE);
    startSearch();
}

void Game::play(Move move) {
    saveNotice.clear();
    const bool capture = capturedPiece(position, move) != '.';
    animationBefore = position;
    animationMove = move;
    animationStart = std::chrono::steady_clock::now();
    moving = true;
    Match::apply(move);
    saveMatch(true);
    if (!SetTimer(window, 2, 16, nullptr)) moving = false;
    refresh();
    pendingSound = outcome == Result::checkmate ?
        (humanTurn() ? SoundCue::defeat : SoundCue::victory) :
        outcome != Result::ongoing ? SoundCue::draw :
        check ? SoundCue::check : capture ? SoundCue::capture : SoundCue::move;
    if (!moving) finishSound();
}

void Game::finishSound() {
    if (pendingSound) audio.play(*pendingSound);
    pendingSound.reset();
}

void Game::toggleSound() {
    audio.toggle();
    if (!audio.enabled()) pendingSound.reset();
    SetWindowTextW(soundButton, audio.enabled() ? L"Som: ligado" : L"Som: desligado");
    InvalidateRect(soundButton, nullptr, FALSE);
}

void Game::undoTurn() {
    if (!undoTarget(human)) return;
    saveNotice.clear();
    pendingSound.reset();
    audio.stop();
    cancelDrag();
    ++generation; // Results from an earlier background search must never be applied.
    moving = false;
    KillTimer(window, 2);
    Match::undo(human);
    saveMatch(true);
    cursor = -1;
    failedSearch = false;
    refresh();
}

bool Game::saveMatch(bool automatic) {
    try {
        saveGameFile(saveFile, {static_cast<const Match&>(*this), human, depth, audio.enabled()});
        saveFailed = false;
        saveNotice = automatic ? L"Salvo automaticamente." : L"Partida salva neste computador.";
        EnableWindow(continueButton, TRUE);
    } catch (const std::exception&) {
        saveFailed = true;
        saveNotice = automatic ? L"Falha no salvamento automático." : L"Não foi possível salvar a partida.";
        InvalidateRect(window, nullptr, FALSE);
        return false;
    }
    InvalidateRect(window, nullptr, FALSE);
    return true;
}

bool Game::continueMatch() {
    SavedGame saved;
    try { saved = loadGameFile(saveFile); }
    catch (const std::exception&) {
        saveNotice = L"Arquivo ausente, inválido ou ilegível.";
        saveFailed = true;
        InvalidateRect(window, nullptr, FALSE);
        return false;
    }
    cancelDrag();
    ++generation;
    pendingSound.reset();
    audio.stop();
    moving = false;
    KillTimer(window, 2);
    static_cast<Match&>(*this) = std::move(saved.match);
    human = saved.human;
    depth = saved.depth;
    if (audio.enabled() != saved.sound) toggleSound();
    SendMessageW(colorChoice, CB_SETCURSEL, human == Color::white ? 0 : 1, 0);
    SendMessageW(levelChoice, CB_SETCURSEL, depth == 1 ? 0 : depth == 2 ? 1 : 2, 0);
    cursor = -1;
    failedSearch = false;
    saveNotice = L"Partida retomada.";
    saveFailed = false;
    refresh();
    return true;
}

void Game::requestContinue() {
    if (!history.empty() && MessageBoxW(window,
        L"Continuar a partida salva? Os lances atuais que não foram salvos serão substituídos.",
        L"Continuar partida", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    continueMatch();
}

RECT Game::squareRect(int square, const Layout& l) const {
    const int row = human == Color::white ? 7 - square / 8 : square / 8;
    const int col = human == Color::white ? square % 8 : 7 - square % 8;
    return {l.board.left + col * l.cell, l.board.top + row * l.cell,
            l.board.left + (col + 1) * l.cell, l.board.top + (row + 1) * l.cell};
}

void Game::tickAnimation() {
    const auto now = std::chrono::steady_clock::now();
    if (moving && now - animationStart >= std::chrono::milliseconds(190)) {
        moving = false;
        finishSound();
        if (outcome != Result::ongoing) resultStart = now;
        startSearch();
    }
    if (!moving && outcome != Result::ongoing)
        resultSeconds = std::chrono::duration<double>(now - resultStart).count();
    if (!moving && (outcome == Result::ongoing || resultSeconds >= 2.4)) KillTimer(window, 2);
    InvalidateRect(window, nullptr, FALSE);
}

void Game::drawAnimation(HDC dc, const Layout& l) const {
    if (moving) {
        double progress = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - animationStart).count() / 0.19;
        progress = std::clamp(progress, 0.0, 1.0);
        progress = progress * progress * (3 - 2 * progress);
        const auto slide = [&](int from, int to, char piece) {
            const auto source = squareRect(from, l);
            const auto target = squareRect(to, l);
            RECT face = source;
            OffsetRect(&face, static_cast<int>((target.left - source.left) * progress),
                             static_cast<int>((target.top - source.top) * progress));
            drawChessPiece(dc, piece, face, true);
        };
        const char piece = animationBefore.board[animationMove.from];
        slide(animationMove.from, animationMove.to, piece);
        if (std::towlower(piece) == 'k' && std::abs(animationMove.to - animationMove.from) == 2) {
            const int direction = animationMove.to > animationMove.from ? 1 : -1;
            const int rook = animationMove.from + (direction == 1 ? 3 : -4);
            slide(rook, animationMove.from + direction, animationBefore.board[rook]);
        }
        return;
    }
    if (outcome == Result::ongoing) return;
    const bool win = outcome == Result::checkmate && !humanTurn();
    const bool loss = outcome == Result::checkmate && humanTurn();
    const COLORREF color = win ? RGB(232, 192, 86) : loss ? RGB(228, 133, 120) : accent;
    if (win && resultSeconds < 2.4) {
        for (int i = 0; i < 36; ++i) {
            const double t = resultSeconds;
            const int x = l.board.left + (i * 79 % (l.cell * 8)) + static_cast<int>(8 * std::sin(i + t * 3));
            const int y = l.board.top + static_cast<int>(std::fmod(i * 23 + t * (65 + i % 5 * 12), l.cell * 8.0));
            fill(dc, {x, y, x + 4, y + 8}, i % 2 ? color : accent);
        }
    }
    const int width = std::min(400, l.cell * 8 - 40);
    const int left = (l.board.left + l.board.right - width) / 2;
    const int entrance = static_cast<int>(24 * (1 - std::min(resultSeconds / 0.35, 1.0)));
    const int top = (l.board.top + l.board.bottom) / 2 - 88 + entrance;
    rounded(dc, {left + 4, top + 6, left + width + 4, top + 182}, RGB(25, 24, 22), 20);
    rounded(dc, {left, top, left + width, top + 176}, color, 18);
    rounded(dc, {left + 2, top + 2, left + width - 2, top + 174}, panelColor, 16);
    label(dc, small, win ? L"VITÓRIA" : loss ? L"DERROTA" : L"FIM DA PARTIDA",
          {left, top + 18, left + width, top + 42}, color, DT_CENTER);
    const HFONT message = font(34, FW_BOLD);
    label(dc, message, win ? L"Você venceu!" : loss ? L"Você perdeu" : L"Empate",
          {left, top + 48, left + width, top + 96}, textColor, DT_CENTER);
    DeleteObject(message);
    label(dc, small, outcome == Result::checkmate ? L"Xeque-mate" : status(),
          {left + 10, top + 101, left + width - 10, top + 125}, muted, DT_CENTER);
    label(dc, small, L"Desfaça o lance ou comece outra partida.",
          {left + 8, top + 136, left + width - 8, top + 160}, muted, DT_CENTER);
}

void Game::startSearch() {
    if (moving || humanTurn() || outcome != Result::ongoing || search.valid() || failedSearch) return;
    searchGeneration = generation;
    if (!SetTimer(window, searchTimer, 25, nullptr)) {
        failedSearch = true;
        return;
    }
    try {
        search = std::async(std::launch::async, [snapshot = position, level = depth] {
            return chooseMove(snapshot, level);
        });
    } catch (...) {
        KillTimer(window, searchTimer);
        failedSearch = true;
    }
}

void Game::finishSearch() {
    if (!search.valid() || search.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    KillTimer(window, searchTimer);
    std::optional<Move> move;
    try {
        move = search.get();
    } catch (...) {
        // A failed search leaves the current position available for inspection.
    }
    if (searchGeneration != generation) {
        startSearch();
    } else if (move) {
        play(*move);
    } else {
        failedSearch = true;
    }
    InvalidateRect(window, nullptr, FALSE);
}

void Game::click(int square) {
    if (outcome != Result::ongoing || !humanTurn() || moving) return;
    if (selected == square) {
        selected = -1;
    } else {
        const auto move = std::find_if(legal.begin(), legal.end(), [&](Move candidate) {
            return candidate.from == selected && candidate.to == square;
        });
        if (move != legal.end()) {
            if (move->promotion == '.') {
                play(*move);
                return;
            }
            const auto menu = CreatePopupMenu();
            if (!menu) return;
            AppendMenuW(menu, MF_STRING, 1, L"Dama");
            AppendMenuW(menu, MF_STRING, 2, L"Torre");
            AppendMenuW(menu, MF_STRING, 3, L"Bispo");
            AppendMenuW(menu, MF_STRING, 4, L"Cavalo");
            const auto l = layout(window);
            const int row = human == Color::white ? 7 - square / 8 : square / 8;
            const int col = human == Color::white ? square % 8 : 7 - square % 8;
            POINT point{l.board.left + col * l.cell + l.cell / 2,
                        l.board.top + row * l.cell + l.cell / 2};
            ClientToScreen(window, &point);
            const auto choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                               point.x, point.y, 0, window, nullptr);
            DestroyMenu(menu);
            if (choice >= 1 && choice <= 4) play({selected, square, "qrbn"[choice - 1]});
            return;
        }
        const char piece = position.board[square];
        selected = piece != '.' && whitePiece(piece) == (human == Color::white) ? square : -1;
    }
    InvalidateRect(window, nullptr, FALSE);
}

void Game::keyboard(WPARAM key) {
    if (key == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        saveMatch();
    } else if (key == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        undoTurn();
    } else if (key == VK_ESCAPE) {
        cancelDrag();
        selected = cursor = -1;
    } else if (pressedSquare >= 0) {
        return;
    } else if (key == VK_SPACE || key == VK_RETURN) {
        if (cursor >= 0) click(cursor);
    } else if (key >= VK_LEFT && key <= VK_DOWN) {
        if (cursor < 0) cursor = human == Color::white ? 12 : 52;
        else {
            const int direction = human == Color::white ? 1 : -1;
            const int file = std::clamp(cursor % 8 + direction *
                ((key == VK_RIGHT) - (key == VK_LEFT)), 0, 7);
            const int rank = std::clamp(cursor / 8 + direction *
                ((key == VK_UP) - (key == VK_DOWN)), 0, 7);
            cursor = rank * 8 + file;
        }
    } else {
        return;
    }
    InvalidateRect(window, nullptr, FALSE);
}

const wchar_t* Game::status() const {
    switch (outcome) {
        case Result::checkmate: return humanTurn() ? L"Xeque-mate. A IA venceu." : L"Xeque-mate. Você venceu!";
        case Result::stalemate: return L"Empate por afogamento.";
        case Result::materialDraw: return L"Empate por material insuficiente.";
        case Result::repetitionDraw: return L"Empate por repetição.";
        case Result::moveCountDraw: return L"Empate pela regra dos 50 lances.";
        case Result::ongoing: break;
    }
    if (failedSearch) return L"IA indisponível. Reinicie a partida.";
    if (!humanTurn()) return check ? L"IA em xeque. Calculando…" : L"IA calculando…";
    return check ? L"Sua vez. Você está em xeque." : L"Sua vez";
}

void Game::drawBoard(HDC dc, const Layout& l) const {
    const auto b = l.board;
    const int cell = l.cell;
    RECT frame = b;
    InflateRect(&frame, 7, 7);
    OffsetRect(&frame, 0, 3);
    rounded(dc, frame, RGB(9, 16, 13), 12);
    OffsetRect(&frame, 0, -3);
    rounded(dc, frame, RGB(30, 29, 26), 8);
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int square = squareAt(row, col);
            char piece = moving ? animationBefore.board[square] : position.board[square];
            if (moving && (square == animationMove.from ||
                (std::towlower(animationBefore.board[animationMove.from]) == 'k' &&
                 std::abs(animationMove.to - animationMove.from) == 2 &&
                 square == animationMove.from + (animationMove.to > animationMove.from ? 3 : -4))))
                piece = '.';
            const RECT rect{b.left + col * cell, b.top + row * cell,
                            b.left + (col + 1) * cell, b.top + (row + 1) * cell};
            const bool latest = !history.empty() &&
                (history.back().from == square || history.back().to == square);
            const bool checkedKing = check && piece == (position.turn == Color::white ? 'K' : 'k');
            COLORREF tile = (row + col) % 2 == 0 ? lightSquare : darkSquare;
            if (latest) tile = RGB(219, 192, 115);
            if (checkedKing) tile = RGB(213, 127, 112);
            if (square == selected) tile = RGB(169, 207, 129);
            fill(dc, rect, tile);

            const bool target = selected >= 0 && std::any_of(legal.begin(), legal.end(), [&](Move move) {
                return move.from == selected && move.to == square;
            });
            if (target && piece != '.') {
                const auto brush = CreateSolidBrush(RGB(53, 91, 61));
                FrameRect(dc, &rect, brush);
                RECT inner = rect;
                InflateRect(&inner, -2, -2);
                FrameRect(dc, &inner, brush);
                DeleteObject(brush);
            }
            if (target && piece == '.')
                dot(dc, rect.left + cell / 2, rect.top + cell / 2, std::max(4, cell / 10), RGB(62, 100, 65));
            if (target && selected >= 0 && std::towlower(position.board[selected]) == 'k' &&
                std::abs(square - selected) == 2)
                label(dc, small, L"Roque", {rect.left, rect.bottom - 21, rect.right, rect.bottom - 2},
                      RGB(37, 57, 28), DT_CENTER);

            if (dragging && target && square == hitTest(mousePoint)) {
                RECT inset = rect;
                InflateRect(&inset, -3, -3);
                const auto brush = CreateSolidBrush(RGB(39, 75, 46));
                FrameRect(dc, &inset, brush);
                DeleteObject(brush);
            }
            if (piece != '.' && !(dragging && square == pressedSquare)) {
                RECT face = rect;
                OffsetRect(&face, 0, -cell / 30);
                drawChessPiece(dc, piece, face);
            }
            if (square == cursor && GetFocus() == window) {
                RECT inset = rect;
                InflateRect(&inset, -4, -4);
                DrawFocusRect(dc, &inset);
            }
        }
        const wchar_t rank = static_cast<wchar_t>(L'1' + squareAt(row, 0) / 8);
        label(dc, small, {&rank, 1}, {b.left - 23, b.top + row * cell, b.left - 5,
              b.top + (row + 1) * cell}, muted, DT_CENTER);
    }
    for (int col = 0; col < 8; ++col) {
        const wchar_t file = static_cast<wchar_t>(L'a' + squareAt(7, col) % 8);
        label(dc, small, {&file, 1}, {b.left + col * cell, b.bottom + 4, b.left + (col + 1) * cell,
              b.bottom + 28}, muted, DT_CENTER);
    }
}

void Game::drawPanel(HDC dc, const Layout& l) const {
    const auto p = l.panel;
    const int x = p.left + 20, y = p.top;
    rounded(dc, p, panelColor);
    dot(dc, x + 4, y + 27, 4, humanTurn() ? accent : muted);
    label(dc, small, L"PARTIDA", {x + 17, y + 16, p.right - 20, y + 39}, muted);
    label(dc, outcome == Result::ongoing && history.empty() && humanTurn() ? bold : regular, status(),
          {x, y + 39, p.right - 16, y + 71}, accent);
    label(dc, small, human == Color::white ? L"Você joga de brancas" : L"Você joga de pretas",
          {x, y + 70, p.right - 20, y + 94}, muted);
    label(dc, small, L"Sua cor", {x, y + 107, x + 110, y + 131}, textColor);
    label(dc, small, L"Dificuldade", {x + 126, y + 107, p.right - 20, y + 131}, textColor);
    label(dc, small, saveNotice.empty() ? L"Salvamento automático ativo." : saveNotice,
          {x, y + 308, p.right - 12, y + 333}, saveFailed ? RGB(244, 125, 125) : saveNotice.empty() ? muted : accent);
    fill(dc, {x, y + 343, p.right - 20, y + 344}, RGB(68, 66, 61));
    label(dc, bold, L"Lances", {x, y + 352, p.right - 20, y + 384}, textColor);
    if (history.empty()) {
        label(dc, small, L"Faça seu primeiro lance.", {x, y + 393, p.right - 14, y + 418}, muted);
        return;
    }
    label(dc, small, L"Brancas", {x + 42, y + 388, x + 120, y + 412}, muted);
    label(dc, small, L"Pretas", {x + 135, y + 388, p.right - 20, y + 412}, muted);
    const auto writeMove = [&](std::size_t index, int left, int top) {
        if (index >= history.size()) return;
        const auto m = history[index];
        // History keeps coordinate notation; castling is highlighted on the board.
        const auto move = notation(m);
        label(dc, regular, std::wstring(move.begin(), move.end()),
              {left, top, left + 72, top + 25}, index + 1 == history.size() ? accent : textColor);
    };
    const auto rounds = (history.size() + 1) / 2;
    const auto rows = static_cast<std::size_t>(std::max(0L, (p.bottom - y - 433) / 30));
    const auto first = rounds > rows ? rounds - rows : 0;
    for (std::size_t round = first; round < rounds; ++round) {
        const int top = y + 418 + static_cast<int>(round - first) * 30;
        if (round % 2 == 0)
            rounded(dc, {x - 7, top, p.right - 13, top + 27}, RGB(51, 49, 45), 6);
        label(dc, small, std::to_wstring(round + 1) + L".", {x, top, x + 37, top + 25}, muted);
        writeMove(round * 2, x + 42, top);
        writeMove(round * 2 + 1, x + 135, top);
    }
}

void Game::drawPlayer(HDC dc, const Layout& l, bool opponent) const {
    const int y = opponent ? l.board.top - 64 : l.board.bottom + 30;
    const int x = l.board.left;
    const Color color = opponent ? (human == Color::white ? Color::black : Color::white) : human;
    const bool white = color == Color::white;
    rounded(dc, {x, y, x + 44, y + 44}, white ? lightSquare : RGB(76, 74, 69), 8);
    drawChessPiece(dc, white ? 'K' : 'k', {x + 2, y + 1, x + 42, y + 42});
    std::wstring name = opponent ? L"IA" : L"Você";
    name += white ? L" · Brancas" : L" · Pretas";
    if (opponent) name += depth >= 4 ? L" · Difícil" : depth == 2 ? L" · Médio" : L" · Fácil";
    label(dc, regular, name, {x + 56, y - 1, l.board.right, y + 23}, textColor);
    const auto& taken = captures[white ? 0 : 1];
    label(dc, small, L"Capturadas", {x + 56, y + 24, x + 130, y + 49}, muted);
    if (taken.empty()) {
        label(dc, small, L"—", {x + 138, y + 24, x + 170, y + 49}, muted);
        return;
    }
    auto sorted = taken;
    constexpr std::string_view order = "pPnNbBrRqQ";
    std::stable_sort(sorted.begin(), sorted.end(), [&](char a, char b) {
        return order.find(a) < order.find(b);
    });
    const int space = l.board.right - x - 145;
    const int step = std::min(24, std::max(1, (space - 30) / static_cast<int>(sorted.size())));
    rounded(dc, {x + 135, y + 22, x + 143 + step * static_cast<int>(sorted.size()) + 24, y + 53},
            RGB(174, 171, 160), 6);
    int left = x + 139;
    for (char piece : sorted) {
        drawChessPiece(dc, piece, {left, y + 22, left + 30, y + 53});
        left += step;
    }
}

void Game::paint() const {
    PAINTSTRUCT paintInfo{};
    HDC screen = BeginPaint(window, &paintInfo);
    RECT bounds{};
    GetClientRect(window, &bounds);
    HDC buffer = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, bounds.right, bounds.bottom);
    const bool buffered = buffer && bitmap;
    const auto previous = buffered ? SelectObject(buffer, bitmap) : nullptr;
    HDC dc = buffered ? buffer : screen;
    const auto l = layout(window);
    fill(dc, bounds, background);
    fill(dc, {0, 0, bounds.right, 51}, panelColor);
    label(dc, title, L"Xadrez", {l.board.left, 6, l.board.left + 130, 43}, textColor);
    label(dc, small, L"Arraste as peças ou jogue por clique.",
          {l.board.left + 140, 10, l.panel.right - 144, 43}, muted);
    drawPlayer(dc, l, true);
    drawPlayer(dc, l, false);
    drawBoard(dc, l);
    drawPanel(dc, l);
    if (dragging && pressedSquare >= 0) {
        const RECT floating{mousePoint.x - l.cell / 2, mousePoint.y - l.cell / 2,
                            mousePoint.x - l.cell / 2 + l.cell, mousePoint.y - l.cell / 2 + l.cell};
        drawChessPiece(dc, position.board[pressedSquare], floating, true);
    }
    drawAnimation(dc, l);
    if (buffered) {
        BitBlt(screen, 0, 0, bounds.right, bounds.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (buffer) DeleteDC(buffer);
    EndPaint(window, &paintInfo);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* game = reinterpret_cast<Game*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        game = static_cast<Game*>(create->lpCreateParams);
        game->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(game));
    }
    if (!game) return DefWindowProcW(window, message, wparam, lparam);
    switch (message) {
        case WM_CREATE:
            setChessWindowIcons(window);
            game->controls();
            return 0;
        case WM_SIZE:
            game->cancelDrag();
            game->placeControls();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            auto* sizes = reinterpret_cast<MINMAXINFO*>(lparam);
            sizes->ptMinTrackSize = {790, 700};
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_SETFOCUS:
            InvalidateRect(window, nullptr, FALSE); return 0;
        case WM_KILLFOCUS:
        case WM_CANCELMODE: game->cancelDrag(); return 0;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lparam) != window) game->cancelDrag();
            return 0;
        case WM_MEASUREITEM:
            reinterpret_cast<MEASUREITEMSTRUCT*>(lparam)->itemHeight = 28;
            return TRUE;
        case WM_DRAWITEM:
            game->drawControl(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
            return TRUE;
        case WM_PAINT: game->paint(); return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == 106 && HIWORD(wparam) == BN_CLICKED) {
                game->saveMatch();
                SetFocus(window);
            }
            if (LOWORD(wparam) == 107 && HIWORD(wparam) == BN_CLICKED) {
                game->requestContinue();
                SetFocus(window);
            }
            if (LOWORD(wparam) == newGameButton && HIWORD(wparam) == BN_CLICKED) {
                game->reset(true);
                SetFocus(window);
            }
            if (LOWORD(wparam) == 104 && HIWORD(wparam) == BN_CLICKED) {
                game->undoTurn();
                SetFocus(window);
            }
            if (LOWORD(wparam) == 105 && HIWORD(wparam) == BN_CLICKED) {
                game->toggleSound();
                SetFocus(window);
            }
            return 0;
        case WM_LBUTTONDOWN: {
            game->pointerDown({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            return 0;
        }
        case WM_MOUSEMOVE:
            game->pointerMove({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;
        case WM_LBUTTONUP:
            game->pointerUp({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}); return 0;
        case WM_KEYDOWN: game->keyboard(wparam); return 0;
        case WM_TIMER:
            if (wparam == searchTimer) game->finishSearch();
            if (wparam == 2) game->tickAnimation();
            return 0;
        case WM_DESTROY:
            game->pendingSound.reset();
            game->audio.stop();
            KillTimer(window, searchTimer);
            KillTimer(window, 2);
            PostQuitMessage(0);
            return 0;
        default: return DefWindowProcW(window, message, wparam, lparam);
    }
}
}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    Gdiplus::GdiplusStartupInput graphicsInput;
    ULONG_PTR graphicsToken = 0;
    if (Gdiplus::GdiplusStartup(&graphicsToken, &graphicsInput, nullptr) != Gdiplus::Ok) return 1;
    struct GraphicsLifetime {
        ULONG_PTR token;
        ~GraphicsLifetime() { Gdiplus::GdiplusShutdown(token); }
    } graphicsLifetime{graphicsToken};
    Game game;
    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = instance;
    cls.hIcon = chessAppIcon(instance, false);
    cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    cls.lpszClassName = windowClass;
    if (!RegisterClassW(&cls)) return 1;
    RECT desktop{0, 0, 1080, 850};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    const int width = std::min(1080L, desktop.right - desktop.left);
    const int height = std::min(850L, desktop.bottom - desktop.top);
    const HWND window = CreateWindowExW(0, windowClass, L"Xadrez contra a IA",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, desktop.left + (desktop.right - desktop.left - width) / 2,
        desktop.top + (desktop.bottom - desktop.top - height) / 2, width, height,
        nullptr, nullptr, instance, &game);
    if (!window) return 1;
    ShowWindow(window, show);
    SetFocus(window);
    MSG message{};
    BOOL received;
    while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_TAB &&
            IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return received < 0 ? 1 : static_cast<int>(message.wParam);
}
