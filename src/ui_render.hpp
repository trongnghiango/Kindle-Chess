#pragma once

#include <gtk/gtk.h>
#include <cairo.h>
#include <array>
#include <string>
#include <vector>

#include "chess.hpp"
#include "config.hpp"
#include "ui_layout.hpp"

namespace kindlechess {

class PieceRenderer {
public:
    PieceRenderer();
    ~PieceRenderer();

    PieceRenderer(const PieceRenderer&) = delete;
    PieceRenderer& operator=(const PieceRenderer&) = delete;

    void load(bool force = false);
    void free();
    bool haveAll();
    bool drawPiece(cairo_t* cr, char p, double x, double y, double cell, bool invert);

private:
    bool loaded = false;
    std::array<GdkPixbuf*, 128> images{};
    std::array<GdkPixbuf*, 128> imagesInverted{};

    GdkPixbuf* tryLoadPiece(char p);
    static GdkPixbuf* makeInverted(GdkPixbuf* src);
};

void drawTextCentered(cairo_t* cr, const std::string& text, double x, double y, double w, double h, double size, bool bold = false);
void drawTextLeft(cairo_t* cr, const std::string& text, double x, double y, double size, bool bold = false);
void drawButton(cairo_t* cr, const RectButton& b, double fontSize);

void drawToolbar(cairo_t* cr, const std::vector<RectButton>& buttons, int fontSize);
void drawBoard(cairo_t* cr, const Layout& L, const Position& displayPos, const AppSettings& settings,
               PieceRenderer& pieceRenderer, int selected, const std::vector<Move>& selectedMoves,
               int lastFrom, int lastTo, int hintFrom, int hintTo);
void drawCoordinates(cairo_t* cr, const Layout& L, const AppSettings& settings);
void drawMovePanel(cairo_t* cr, const Layout& L, const ChessGame& game, const AppSettings& settings);
void drawCapturedPieces(cairo_t* cr, const Layout& L, const ChessGame& game, const AppSettings& settings,
                        PieceRenderer& pieceRenderer, int reviewPly);

void drawConfirmNew(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings);
void drawMoveConfirm(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, int from, int to, char promo, const AppSettings& settings);
void drawGameOver(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const std::string& text, const AppSettings& settings);
void drawReviewControls(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, int ply, int total, const AppSettings& settings);
void drawPromotion(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, bool promoteWhite, PieceRenderer& pieceRenderer, const AppSettings& settings);
void drawSettingsMain(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings, bool locked);
void drawSettingsSaveLoad(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings);

} // namespace kindlechess
