#include "ui_layout.hpp"
#include "chess.hpp"

#include <algorithm>

namespace kindlechess {

static int clampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

static int buttonWidthFor(const std::string& label, int fontSize) {
    int charW = std::max(10, static_cast<int>(fontSize * 0.60));
    int base = 58 + static_cast<int>(label.size()) * charW;
    if (label.rfind("Engine", 0) == 0) base += 26;
    if (label.rfind("Elo", 0) == 0) base += 22;
    return clampInt(base, 92, 260);
}

static int toolbarHeightFor(int W, int fontSize) {
    const std::vector<std::string> labels{"New", "Undo", "Settings", "Exit"};
    int margin = 8;
    int rowH = std::max(38, fontSize + 24);
    int rows = 1;
    int bx = margin;
    for (const auto& label : labels) {
        int bw = buttonWidthFor(label, fontSize);
        if (bx + bw > W - margin && bx > margin) {
            rows++;
            bx = margin;
        }
        bx += bw + 8;
    }
    return margin + rows * rowH + margin;
}

int Layout::squareFromXY(int x, int y, bool flipped) const {
    if (x < boardX || x >= boardX + board || y < boardY || y >= boardY + board) return -1;
    int vf = (x - boardX) / cell;
    int vr = (y - boardY) / cell;
    if (vf < 0 || vf >= 8 || vr < 0 || vr >= 8) return -1;
    int br = flipped ? 7 - vr : vr;
    int bf = flipped ? 7 - vf : vf;
    return sq(br, bf);
}

Layout computeLayout(int width, int height, const AppSettings& settings) {
    Layout L;
    L.W = width;
    L.H = height;
    L.margin = 8;
    L.topH = toolbarHeightFor(L.W, settings.uiFontSize);
    L.statusH = std::max(88, settings.uiFontSize * 3 + 18);

    // Dedicated coordinate gutter (outside the 8x8 chessboard)
    L.coordMargin = settings.showCoordinates ? std::max(22, std::min(40, settings.uiFontSize - 4)) : 0;

    int availableW = L.W - 2 * L.margin;
    int requestedPanelW = settings.showMoveList ? std::max(160, settings.uiFontSize * 8) : 0;
    // On PW2 (758x1024) and PW3 (1072x1448) in portrait, a side panel squishes the board
    // and wastes vertical space. Use side panel only in landscape or on square/wide screens.
    bool isLandscape = L.W > L.H;
    bool isSquareOrWide = (L.W >= 950 && L.H <= L.W * 1.15);
    L.hasPanel = settings.showMoveList && (isLandscape || isSquareOrWide) && (requestedPanelW + 360 < availableW);

    L.capturedH = L.hasPanel ? 0 : std::max(148, std::min(240, settings.uiFontSize * 3 + 62));

    int availableH = L.H - L.topH - L.statusH - 2 * L.margin - L.capturedH;
    if (availableH < 240) availableH = L.H - L.topH - L.statusH - 2 * L.margin;

    // Reserve space for external coordinate gutters:
    // Ranks (1-8) outside on the left: L.coordMargin
    // Files (a-h) outside at the bottom: L.coordMargin
    int boardAreaW = availableW - (L.hasPanel ? requestedPanelW + L.margin : 0) - L.coordMargin;
    int boardAreaH = availableH - L.coordMargin;

    int boardMax = std::min(boardAreaW, boardAreaH);
    L.board = (boardMax / 8) * 8;
    if (L.board < 240) L.board = (boardMax >= 8) ? (boardMax - (boardMax % 8)) : 240;
    L.cell = std::max(1, L.board / 8);

    // Board position: offset by L.coordMargin so numbers sit on the left outside the board
    if (L.hasPanel) {
        L.boardX = L.margin + L.coordMargin;
    } else {
        int totalWidth = L.board + L.coordMargin;
        int leftOffset = (L.W - totalWidth) / 2;
        L.boardX = leftOffset + L.coordMargin;
    }

    int totalHeight = L.board + L.coordMargin;
    int startY = L.topH + ((availableH - totalHeight) / 2);
    if (startY < L.topH + L.margin) startY = L.topH + L.margin;
    L.boardY = startY;

    if (L.hasPanel) {
        L.panelX = L.boardX + L.board + L.margin;
        L.panelY = L.boardY;
        L.panelW = L.W - L.panelX - L.margin;
        L.panelH = L.board + L.coordMargin;
    }
    return L;
}

std::vector<RectButton> computeToolbarButtons(const Layout& L, const AppSettings& settings) {
    std::vector<RectButton> buttons;
    const std::vector<std::string> labels{"New", "Undo", "Settings", "Exit"};
    int margin = 8;
    int rowH = std::max(38, settings.uiFontSize + 24);
    int bx = margin;
    int by = margin;
    for (const auto& label : labels) {
        int bw = buttonWidthFor(label, settings.uiFontSize);
        if (bx + bw > L.W - margin && bx > margin) {
            bx = margin;
            by += rowH;
        }
        buttons.push_back({label, bx, by, bw, rowH - 8, 0});
        bx += bw + 8;
    }
    return buttons;
}

std::vector<RectButton> computeConfirmNewButtons(const Layout& L, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.84, 620.0);
    double oh = 230;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    int bw = std::max(210, std::min(280, static_cast<int>(ow / 2 - 28)));
    int bh = std::max(60, std::min(76, settings.uiFontSize + 24));
    return {
        {"Cancel", static_cast<int>(ox + ow / 2 - bw - 12), static_cast<int>(oy + oh - bh - 24), bw, bh, 0},
        {"New Game", static_cast<int>(ox + ow / 2 + 12), static_cast<int>(oy + oh - bh - 24), bw, bh, 0}
    };
}

std::vector<RectButton> computeMoveConfirmButtons(const Layout& L, const AppSettings& settings) {
    (void)settings;
    double ow = std::min(static_cast<double>(L.W) * 0.84, 620.0);
    double oh = 230;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    int bw = 190, bh = 56;
    return {
        {"Cancel", static_cast<int>(ox + ow / 2 - bw - 12), static_cast<int>(oy + oh - bh - 28), bw, bh, 0},
        {"Confirm", static_cast<int>(ox + ow / 2 + 12), static_cast<int>(oy + oh - bh - 28), bw, bh, 0}
    };
}

std::vector<RectButton> computeGameOverButtons(const Layout& L, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.84, 650.0);
    double oh = 250;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    int bw = std::max(230, std::min(320, static_cast<int>(ow - 80)));
    int bh = std::max(62, std::min(78, settings.uiFontSize + 24));
    return {
        {"New Game", static_cast<int>(ox + ow / 2 - bw / 2), static_cast<int>(oy + oh - bh - 24), bw, bh, 0}
    };
}

std::vector<RectButton> computeReviewButtons(const Layout& L, const AppSettings& settings) {
    (void)settings;
    int w = std::min(L.W - 40, 680);
    int h = 92;
    int x = (L.W - w) / 2;
    int y = L.H - L.statusH - h - 8;
    if (y < L.boardY + L.board - h) y = L.boardY + L.board - h - 10;
    int bw = (w - 48) / 3;
    return {
        {"Prev", x + 12, y + 42, bw, 40, 0},
        {"Next", x + 24 + bw, y + 42, bw, 40, 0},
        {"Exit Review", x + 36 + bw * 2, y + 42, bw, 40, 0}
    };
}

std::vector<RectButton> computePromotionButtons(const Layout& L) {
    std::vector<RectButton> buttons;
    double ox = L.boardX + L.board * 0.1;
    double oy = L.boardY + L.board * 0.34;
    double ow = L.board * 0.8;
    double oh = std::max(230.0, L.board * 0.34);
    const char promos[4] = {'q', 'r', 'b', 'n'};
    const char* labels[4] = {"Q", "R", "B", "N"};
    for (int i = 0; i < 4; ++i) {
        int x = static_cast<int>(ox + 18 + i * (ow - 36) / 4.0);
        int y = static_cast<int>(oy + 68);
        int w = static_cast<int>((ow - 52) / 4.0);
        int h = static_cast<int>(oh - 88);
        buttons.push_back({labels[i], x, y, w, h, promos[i]});
    }
    return buttons;
}

std::vector<RectButton> computeSettingsMainButtons(const Layout& L, const AppSettings& settings, bool locked) {
    double ow = std::min(static_cast<double>(L.W) * 0.96, 1040.0);
    double oh = std::min(static_cast<double>(L.H) * 0.94, 1180.0);
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;

    int sfs = settings.settingsFontSize();
    int bw = static_cast<int>((ow - 96) / 2.0);
    int bh = std::max(66, std::min(88, sfs + 22));
    int left = static_cast<int>(ox + 42);
    int right = left + bw + 12;
    int y = static_cast<int>(oy + 118);
    int gap = bh + 10;

    std::string engineLabel = std::string(locked ? "Engine Locked: " : "Engine: ");
    if (settings.engineMode == EngineMode::Off) engineLabel += "Off";
    else if (settings.engineMode == EngineMode::Black) engineLabel += "Black";
    else engineLabel += "White";

    std::vector<RectButton> buttons;
    buttons.push_back({engineLabel, left, y, bw * 2 + 12, bh, 0}); y += gap;
    buttons.push_back({"Elo -250", left, y, bw, bh, 0});
    buttons.push_back({"Elo +250", right, y, bw, bh, 0}); y += gap;
    buttons.push_back({"Elo: " + std::to_string(settings.currentEngineElo()), left, y, bw * 2 + 12, bh, 0}); y += gap;

    buttons.push_back({"Hint", left, y, bw, bh, 0});
    buttons.push_back({"Restart Engine", right, y, bw, bh, 0}); y += gap;
    buttons.push_back({"Export PGN", left, y, bw, bh, 0});
    buttons.push_back({std::string("Confirm Moves: ") + (settings.confirmMoves ? "On" : "Off"), right, y, bw, bh, 0}); y += gap;
    buttons.push_back({"Save / Load", left, y, bw, bh, 0});
    buttons.push_back({"Review Game", right, y, bw, bh, 0}); y += gap;

    buttons.push_back({std::string("Coordinates: ") + (settings.showCoordinates ? "On" : "Off"), left, y, bw, bh, 0});
    buttons.push_back({std::string("Move List: ") + (settings.showMoveList ? "On" : "Off"), right, y, bw, bh, 0}); y += gap;
    buttons.push_back({"Font -", left, y, bw, bh, 0});
    buttons.push_back({"Font +", right, y, bw, bh, 0}); y += gap;
    buttons.push_back({std::string("Piece PNGs: ") + (settings.usePieceImages ? "On" : "Off"), left, y, bw, bh, 0});
    buttons.push_back({"Reload PNGs", right, y, bw, bh, 0}); y += gap;
    buttons.push_back({std::string("Dark Piece Fix: ") + settings.darkPieceModeLabel(), left, y, bw * 2 + 12, bh, 0}); y += gap;
    buttons.push_back({"Close Settings", left, y, bw * 2 + 12, bh, 0});

    return buttons;
}

std::vector<RectButton> computeSettingsSaveLoadButtons(const Layout& L, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.96, 1040.0);
    double oh = std::min(static_cast<double>(L.H) * 0.94, 1180.0);
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;

    int sfs = settings.settingsFontSize();
    int left = static_cast<int>(ox + 30);
    int y = static_cast<int>(oy + 126);
    int rowH = std::max(92, sfs + 48);
    int smallW = std::max(88, std::min(120, sfs * 2));
    int summaryW = static_cast<int>(ow - 70 - (smallW * 3 + 24));
    int bh = std::max(62, std::min(82, sfs + 20));

    std::vector<RectButton> buttons;
    for (int i = 1; i <= 5; ++i) {
        buttons.push_back({"Save " + std::to_string(i), left + summaryW, y, smallW, bh, static_cast<char>(i)});
        buttons.push_back({"Load " + std::to_string(i), left + summaryW + smallW + 8, y, smallW, bh, static_cast<char>(i)});
        buttons.push_back({"Del " + std::to_string(i), left + summaryW + (smallW + 8) * 2, y, smallW, bh, static_cast<char>(i)});
        y += rowH;
    }
    buttons.push_back({"Back", left, static_cast<int>(oy + oh - 78), 190, 62, 0});
    buttons.push_back({"Close Settings", left + 210, static_cast<int>(oy + oh - 78), 320, 62, 0});
    return buttons;
}

} // namespace kindlechess
