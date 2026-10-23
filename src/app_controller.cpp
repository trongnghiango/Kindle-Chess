#include "app_controller.hpp"

#include <algorithm>
#include <sstream>
#include <unistd.h>

namespace kindlechess {

static int clampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

AppController::AppController() = default;

AppController::~AppController() {
    handleExit();
}

void AppController::init(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--reset") {
            unlink(save_path().c_str());
        }
    }
    loadAppSettings(settings);
    settings.flipped = false;
    game.loadFrom(save_path());
    updateGameOverPopup();
}

void AppController::setupGtk(GtkWidget* win, GtkWidget* ar) {
    window = win;
    area = ar;
}

void AppController::queueRedraw() {
    if (area) {
        gtk_widget_queue_draw(area);
    }
}

void AppController::flushGui() {
    queueRedraw();
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

bool AppController::gameSetupLocked() const {
    return !game.uciMoves().empty();
}

bool AppController::engineShouldMove() const {
    if (reviewMode || showMoveConfirm || resigned || showGameOver) return false;
    if (settings.engineMode == EngineMode::Off) return false;
    if (game.whiteToMove() && settings.engineMode == EngineMode::White) return true;
    if (!game.whiteToMove() && settings.engineMode == EngineMode::Black) return true;
    return false;
}

bool AppController::updateGameOverPopup() {
    std::string msg = game.gameOverMessage();
    if (!msg.empty()) {
        showGameOver = true;
        gameOverText = msg;
        selected = -1;
        pendingFrom = pendingTo = -1;
        showMoveConfirm = false;
        return true;
    }
    showGameOver = false;
    gameOverText.clear();
    return false;
}

void AppController::maybeEngineMove() {
    if (!engineShouldMove()) return;
    auto legal = game.legalMoves();
    if (legal.empty()) return;

    int elo = settings.currentEngineElo();
    int mt = settings.engineMovetime();
    uiStatus = "Engine thinking at Elo " + std::to_string(elo) + "...";
    flushGui();

    std::string err;
    std::string mv = engine.bestMove(engine_path(), game.uciMoves(), mt, elo, err);
    uiStatus.clear();
    if (mv.empty()) {
        uiStatus = err.empty() ? "Engine produced no move." : err;
    } else if (!game.makeUciMove(mv)) {
        uiStatus = "Engine sent illegal move: " + mv;
    } else {
        game.saveTo(save_path());
        updateGameOverPopup();
    }
    queueRedraw();
}

void AppController::requestHint() {
    if (reviewMode) {
        uiStatus = "Exit review mode before requesting a hint.";
        return;
    }
    if (game.legalMoves().empty()) {
        uiStatus = "No legal moves available.";
        return;
    }
    int elo = settings.currentEngineElo();
    uiStatus = "Stockfish calculating hint...";
    flushGui();

    std::string err;
    std::string mv = engine.bestMove(engine_path(), game.uciMoves(), std::max(750, settings.engineMovetime()), elo, err);
    if (mv.size() >= 4) {
        hintFrom = parse_square(mv.substr(0, 2));
        hintTo = parse_square(mv.substr(2, 2));
        uiStatus = "Hint: " + mv.substr(0, 2) + "-" + mv.substr(2, 2);
    } else {
        hintFrom = hintTo = -1;
        uiStatus = err.empty() ? "No hint available." : err;
    }
}

void AppController::startNewGame() {
    uiStatus.clear();
    resigned = false;
    resignedMessage.clear();
    showGameOver = false;
    gameOverText.clear();
    confirmNew = false;
    showSettings = false;
    settingsPage = 0;
    reviewMode = false;
    reviewPly = 0;
    hintFrom = hintTo = -1;
    clearMoveConfirm();
    game.reset();
    selected = -1;
    pendingFrom = pendingTo = -1;
    game.saveTo(save_path());
    maybeEngineMove();
}

void AppController::clearMoveConfirm() {
    showMoveConfirm = false;
    confirmFrom = confirmTo = -1;
    confirmPromotion = 0;
}

void AppController::beginMoveConfirm(int from, int to, char promo) {
    confirmFrom = from;
    confirmTo = to;
    confirmPromotion = promo;
    showMoveConfirm = true;
    selected = -1;
    pendingFrom = pendingTo = -1;
}

void AppController::commitHumanMove(int from, int to, char promo) {
    resigned = false;
    resignedMessage.clear();
    showGameOver = false;
    gameOverText.clear();
    hintFrom = hintTo = -1;
    clearMoveConfirm();
    pendingFrom = -1;
    pendingTo = -1;

    if (game.makeMove(from, to, promo)) {
        game.saveTo(save_path());
        selected = -1;
        queueRedraw();
        if (!updateGameOverPopup()) {
            maybeEngineMove();
        }
    }
}

void AppController::handleExit() {
    game.saveTo(save_path());
    saveAppSettings(settings);
    engine.stop();
    pieceRenderer.free();
}

void AppController::lastMoveSquares(int& from, int& to) const {
    from = -1;
    to = -1;
    const auto& moves = game.uciMoves();
    if (moves.empty()) return;
    const std::string& u = moves.back();
    if (u.size() < 4) return;
    from = parse_square(u.substr(0, 2));
    to = parse_square(u.substr(2, 2));
}

std::string AppController::moveSummaryLine() const {
    const auto& moves = game.uciMoves();
    std::ostringstream os;
    os << "Move " << (moves.size() / 2 + 1);
    if (!moves.empty()) os << "  Last: " << moves.back();
    os << "  " << settings.engineModeLabel();
    if (settings.engineMode != EngineMode::Off) os << "  Elo: " << settings.currentEngineElo();
    return os.str();
}

void AppController::handleDraw(GtkWidget* widget, cairo_t* cr) {
    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);
    Layout L = computeLayout(a.width, a.height, settings);

    cairo_set_source_rgb(cr, 0.92, 0.92, 0.90);
    cairo_paint(cr);

    auto toolbarBtns = computeToolbarButtons(L, settings);
    drawToolbar(cr, toolbarBtns, settings.uiFontSize);

    Position displayPos = reviewMode ? game.positionAfterPly(reviewPly) : game.state();
    std::vector<Move> selectedMoves;
    if (!reviewMode && selected >= 0) {
        selectedMoves = game.legalMovesFrom(selected);
    }

    int lastFrom = -1, lastTo = -1;
    if (reviewMode) {
        const auto& moves = game.uciMoves();
        if (reviewPly > 0 && reviewPly <= static_cast<int>(moves.size())) {
            const std::string& u = moves[reviewPly - 1];
            if (u.size() >= 4) {
                lastFrom = parse_square(u.substr(0, 2));
                lastTo = parse_square(u.substr(2, 2));
            }
        }
    } else {
        lastMoveSquares(lastFrom, lastTo);
    }

    drawBoard(cr, L, displayPos, settings, pieceRenderer, selected, selectedMoves,
              lastFrom, lastTo, hintFrom, hintTo);
    drawMovePanel(cr, L, game, settings);
    drawCapturedPieces(cr, L, game, settings, pieceRenderer, reviewMode ? reviewPly : -1);

    std::string status = reviewMode
        ? ("Review mode. " + std::to_string(reviewPly) + " / " + std::to_string(game.uciMoves().size()) + " plies")
        : (uiStatus.empty() ? game.status() : uiStatus);
    if (!reviewMode && uiStatus.empty() && hintFrom >= 0 && hintTo >= 0) {
        status = "Hint: " + square_name(hintFrom) + "-" + square_name(hintTo);
    }

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    drawTextCentered(cr, status, L.margin, L.H - L.statusH + 6, L.W - 2 * L.margin,
                     L.statusH * 0.58, settings.uiFontSize + 2, true);
    drawTextCentered(cr, moveSummaryLine(), L.margin, L.H - L.statusH + L.statusH * 0.58,
                     L.W - 2 * L.margin, L.statusH * 0.36, std::max(12, settings.uiFontSize - 7), false);

    if (pendingFrom >= 0) {
        auto promoBtns = computePromotionButtons(L);
        bool promoteWhite = is_white_piece(game.pieceAt(pendingFrom));
        drawPromotion(cr, L, promoBtns, promoteWhite, pieceRenderer, settings);
    }

    if (reviewMode) {
        auto reviewBtns = computeReviewButtons(L, settings);
        drawReviewControls(cr, L, reviewBtns, reviewPly, static_cast<int>(game.uciMoves().size()), settings);
    }

    if (showGameOver) {
        auto goBtns = computeGameOverButtons(L, settings);
        drawGameOver(cr, L, goBtns, gameOverText, settings);
    } else if (confirmNew) {
        auto newBtns = computeConfirmNewButtons(L, settings);
        drawConfirmNew(cr, L, newBtns, settings);
    } else if (showMoveConfirm) {
        auto mcBtns = computeMoveConfirmButtons(L, settings);
        drawMoveConfirm(cr, L, mcBtns, confirmFrom, confirmTo, confirmPromotion, settings);
    } else if (showSettings) {
        if (settingsPage == 1) {
            auto slBtns = computeSettingsSaveLoadButtons(L, settings);
            drawSettingsSaveLoad(cr, L, slBtns, settings);
        } else {
            auto sBtns = computeSettingsMainButtons(L, settings, gameSetupLocked());
            drawSettingsMain(cr, L, sBtns, settings, gameSetupLocked());
        }
    }
}

bool AppController::handleSettingsClick(const RectButton& b) {
    bool locked = gameSetupLocked();
    if (settingsPage == 1) {
        if (b.label == "Back") {
            settingsPage = 0;
        } else if (b.label == "Close Settings") {
            showSettings = false;
            settingsPage = 0;
        } else if (b.tag >= 1 && b.tag <= 5) {
            int slot = static_cast<int>(b.tag);
            if (b.label.rfind("Save ", 0) == 0) {
                saveSlot(slot, game);
                uiStatus = "Saved to slot " + std::to_string(slot) + ".";
            } else if (b.label.rfind("Load ", 0) == 0) {
                if (loadSlot(slot, game)) {
                    selected = -1;
                    pendingFrom = pendingTo = -1;
                    clearMoveConfirm();
                    hintFrom = hintTo = -1;
                    reviewMode = false;
                    reviewPly = 0;
                    uiStatus = "Loaded slot " + std::to_string(slot) + ".";
                    updateGameOverPopup();
                } else {
                    uiStatus = "Slot " + std::to_string(slot) + " is empty or invalid.";
                }
            } else if (b.label.rfind("Del ", 0) == 0) {
                deleteSlot(slot);
                uiStatus = "Deleted slot " + std::to_string(slot) + ".";
            }
        }
        saveAppSettings(settings);
        queueRedraw();
        return true;
    }

    if (b.label.rfind("Coordinates", 0) == 0) {
        settings.showCoordinates = !settings.showCoordinates;
    } else if (b.label.rfind("Move List", 0) == 0) {
        settings.showMoveList = !settings.showMoveList;
    } else if (b.label == "Font -") {
        settings.uiFontSize = clampInt(settings.uiFontSize - 2, 14, 60);
    } else if (b.label == "Font +") {
        settings.uiFontSize = clampInt(settings.uiFontSize + 2, 14, 60);
    } else if (b.label.rfind("Engine", 0) == 0 && !locked) {
        if (settings.engineMode == EngineMode::Off) settings.engineMode = EngineMode::Black;
        else if (settings.engineMode == EngineMode::Black) settings.engineMode = EngineMode::White;
        else settings.engineMode = EngineMode::Off;
    } else if (b.label == "Elo -250" && !locked) {
        settings.engineEloIndex = clampInt(settings.engineEloIndex - 1, 0, static_cast<int>(settings.engineElos.size()) - 1);
    } else if (b.label == "Elo +250" && !locked) {
        settings.engineEloIndex = clampInt(settings.engineEloIndex + 1, 0, static_cast<int>(settings.engineElos.size()) - 1);
    } else if (b.label == "Hint") {
        showSettings = false;
        requestHint();
    } else if (b.label == "Restart Engine") {
        engine.stop();
        uiStatus = "Engine restarted.";
    } else if (b.label == "Export PGN") {
        uiStatus = exportPGN(game);
    } else if (b.label.rfind("Confirm Moves", 0) == 0) {
        settings.confirmMoves = !settings.confirmMoves;
    } else if (b.label == "Save / Load") {
        settingsPage = 1;
    } else if (b.label == "Review Game") {
        reviewMode = true;
        reviewPly = static_cast<int>(game.uciMoves().size());
        showSettings = false;
        selected = -1;
        hintFrom = hintTo = -1;
        uiStatus = "Review mode.";
    } else if (b.label.rfind("Piece PNGs", 0) == 0) {
        settings.usePieceImages = !settings.usePieceImages;
    } else if (b.label.rfind("Dark Piece Fix", 0) == 0) {
        settings.darkPieceMode = (settings.darkPieceMode + 1) % 3;
        uiStatus = std::string("Dark Piece Fix: ") + settings.darkPieceModeLabel();
    } else if (b.label == "Reload PNGs") {
        pieceRenderer.free();
        pieceRenderer.load(true);
        uiStatus = pieceRenderer.haveAll() ? "Piece PNGs loaded." : "Some piece PNGs are missing; using fallback letters.";
    } else if (b.label == "Close Settings") {
        showSettings = false;
        settingsPage = 0;
    }
    saveAppSettings(settings);
    queueRedraw();
    return true;
}

void AppController::clickToolbar(const std::string& label) {
    uiStatus.clear();
    if (label == "New") {
        confirmNew = true;
        settingsPage = 0;
        showSettings = false;
        showGameOver = false;
    } else if (label == "Undo") {
        if (reviewMode) {
            uiStatus = "Exit review mode before undo.";
            queueRedraw();
            return;
        }
        resigned = false;
        resignedMessage.clear();
        showGameOver = false;
        gameOverText.clear();
        hintFrom = hintTo = -1;
        game.undo();
        game.saveTo(save_path());
        selected = -1;
        if (engineShouldMove()) {
            game.undo();
            game.saveTo(save_path());
        }
    } else if (label == "Settings") {
        showSettings = true;
        settingsPage = 0;
        confirmNew = false;
        showGameOver = false;
    } else if (label == "Exit") {
        handleExit();
        gtk_main_quit();
        return;
    }
    queueRedraw();
}

bool AppController::handleButtonPress(GtkWidget* widget, int x, int y) {
    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);
    Layout L = computeLayout(a.width, a.height, settings);

    // 1. Game over overlay
    if (showGameOver) {
        auto goBtns = computeGameOverButtons(L, settings);
        for (const auto& b : goBtns) {
            if (b.contains(x, y)) {
                if (b.label == "New Game") startNewGame();
                queueRedraw();
                return true;
            }
        }
        return true;
    }

    // 2. Confirm new game overlay
    if (confirmNew) {
        auto newBtns = computeConfirmNewButtons(L, settings);
        for (const auto& b : newBtns) {
            if (b.contains(x, y)) {
                if (b.label == "Cancel") confirmNew = false;
                else if (b.label == "New Game") startNewGame();
                queueRedraw();
                return true;
            }
        }
        return true;
    }

    // 3. Move confirmation overlay
    if (showMoveConfirm) {
        auto mcBtns = computeMoveConfirmButtons(L, settings);
        for (const auto& b : mcBtns) {
            if (b.contains(x, y)) {
                if (b.label == "Cancel") clearMoveConfirm();
                else if (b.label == "Confirm") commitHumanMove(confirmFrom, confirmTo, confirmPromotion);
                queueRedraw();
                return true;
            }
        }
        return true;
    }

    // 4. Settings overlay
    if (showSettings) {
        auto btns = (settingsPage == 1) ? computeSettingsSaveLoadButtons(L, settings)
                                        : computeSettingsMainButtons(L, settings, gameSetupLocked());
        for (const auto& b : btns) {
            if (b.contains(x, y)) {
                return handleSettingsClick(b);
            }
        }
        return true;
    }

    // 5. Promotion choice overlay
    if (pendingFrom >= 0) {
        auto promoBtns = computePromotionButtons(L);
        bool clickedButton = false;
        for (const auto& b : promoBtns) {
            if (b.contains(x, y)) {
                clickedButton = true;
                if (settings.confirmMoves) {
                    beginMoveConfirm(pendingFrom, pendingTo, b.tag);
                } else {
                    commitHumanMove(pendingFrom, pendingTo, b.tag);
                }
                queueRedraw();
                return true;
            }
        }
        if (!clickedButton) {
            // Tapped outside promotion buttons: cancel promotion cleanly
            pendingFrom = pendingTo = -1;
            selected = -1;
            queueRedraw();
            return true;
        }
    }

    // 6. Review controls
    if (reviewMode) {
        auto reviewBtns = computeReviewButtons(L, settings);
        for (const auto& b : reviewBtns) {
            if (b.contains(x, y)) {
                int total = static_cast<int>(game.uciMoves().size());
                if (b.label == "Prev") reviewPly = clampInt(reviewPly - 1, 0, total);
                else if (b.label == "Next") reviewPly = clampInt(reviewPly + 1, 0, total);
                else if (b.label == "Exit Review") { reviewMode = false; reviewPly = 0; }
                queueRedraw();
                return true;
            }
        }
    }

    // 7. Toolbar buttons
    auto toolbarBtns = computeToolbarButtons(L, settings);
    for (const auto& b : toolbarBtns) {
        if (b.contains(x, y)) {
            clickToolbar(b.label);
            return true;
        }
    }

    if (reviewMode || resigned || showGameOver || engineShouldMove()) {
        return true;
    }

    // 8. Board clicks
    int s = L.squareFromXY(x, y, settings.flipped);
    if (s < 0) return false;

    char p = game.pieceAt(s);
    bool sideWhite = game.whiteToMove();

    if (selected < 0) {
        if (p != '.' && is_white_piece(p) == sideWhite) {
            selected = s;
        }
    } else {
        if (s == selected) {
            selected = -1;
        } else if (p != '.' && is_white_piece(p) == sideWhite) {
            selected = s;
        } else {
            if (game.needsPromotionChoice(selected, s)) {
                pendingFrom = selected;
                pendingTo = s;
            } else {
                bool legalDest = false;
                for (const auto& m : game.legalMovesFrom(selected)) {
                    if (m.to == s && !m.promotion) {
                        legalDest = true;
                        break;
                    }
                }
                if (!legalDest) {
                    uiStatus = "Illegal move.";
                } else if (settings.confirmMoves) {
                    beginMoveConfirm(selected, s, 0);
                } else {
                    commitHumanMove(selected, s, 0);
                }
            }
        }
    }
    queueRedraw();
    return true;
}

} // namespace kindlechess
