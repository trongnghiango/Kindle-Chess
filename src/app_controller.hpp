#pragma once

#include <gtk/gtk.h>
#include <cairo.h>
#include <string>

#include "chess.hpp"
#include "config.hpp"
#include "engine.hpp"
#include "ui_layout.hpp"
#include "ui_render.hpp"

namespace kindlechess {

class AppController {
public:
    AppController();
    ~AppController();

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    void init(int argc, char** argv);
    void setupGtk(GtkWidget* window, GtkWidget* area);

    void handleDraw(GtkWidget* widget, cairo_t* cr);
    bool handleButtonPress(GtkWidget* widget, int x, int y);
    void handleExit();

    void queueRedraw();
    void flushGui();

private:
    ChessGame game;
    UciEngine engine;
    AppSettings settings;
    PieceRenderer pieceRenderer;

    GtkWidget* window = nullptr;
    GtkWidget* area = nullptr;

    int selected = -1;
    std::string uiStatus;
    int pendingFrom = -1;
    int pendingTo = -1;

    bool showSettings = false;
    int settingsPage = 0; // 0: main, 1: save/load
    bool confirmNew = false;
    bool showGameOver = false;
    std::string gameOverText;
    bool resigned = false;
    std::string resignedMessage;

    bool showMoveConfirm = false;
    int confirmFrom = -1;
    int confirmTo = -1;
    char confirmPromotion = 0;

    bool reviewMode = false;
    int reviewPly = 0;

    int hintFrom = -1;
    int hintTo = -1;

    bool gameSetupLocked() const;
    bool engineShouldMove() const;
    bool updateGameOverPopup();
    void maybeEngineMove();
    void requestHint();

    void startNewGame();
    void clearMoveConfirm();
    void beginMoveConfirm(int from, int to, char promo);
    void commitHumanMove(int from, int to, char promo);

    void clickToolbar(const std::string& label);
    bool handleSettingsClick(const RectButton& b);

    std::string moveSummaryLine() const;
    void lastMoveSquares(int& from, int& to) const;
};

} // namespace kindlechess
