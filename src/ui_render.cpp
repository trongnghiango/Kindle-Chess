#include "ui_render.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <sys/stat.h>

namespace kindlechess {

static std::string pieceShortName(char p) {
    bool white = is_white_piece(p);
    char t = piece_type(p);
    std::string out;
    out.push_back(white ? 'w' : 'b');
    out.push_back(t);
    return out;
}

static std::string pieceLongName(char p) {
    std::string color = is_white_piece(p) ? "white_" : "black_";
    switch (piece_type(p)) {
        case 'k': return color + "king";
        case 'q': return color + "queen";
        case 'r': return color + "rook";
        case 'b': return color + "bishop";
        case 'n': return color + "knight";
        case 'p': return color + "pawn";
        default: return "";
    }
}

static std::string pieceLetter(char p) {
    switch (piece_type(p)) {
        case 'k': return "K";
        case 'q': return "Q";
        case 'r': return "R";
        case 'b': return "B";
        case 'n': return "N";
        case 'p': return "P";
        default: return "";
    }
}

static bool fileExists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

static GdkPixbuf* loadPieceFile(const std::string& path) {
    if (!fileExists(path)) return nullptr;
    GError* err = nullptr;
    GdkPixbuf* pix = gdk_pixbuf_new_from_file(path.c_str(), &err);
    if (err) g_error_free(err);
    return pix;
}

PieceRenderer::PieceRenderer() = default;

PieceRenderer::~PieceRenderer() {
    free();
}

void PieceRenderer::free() {
    for (auto& pix : images) {
        if (pix) {
            g_object_unref(G_OBJECT(pix));
            pix = nullptr;
        }
    }
    for (auto& pix : imagesInverted) {
        if (pix) {
            g_object_unref(G_OBJECT(pix));
            pix = nullptr;
        }
    }
    loaded = false;
}

GdkPixbuf* PieceRenderer::makeInverted(GdkPixbuf* src) {
    if (!src) return nullptr;
    GdkPixbuf* dst = gdk_pixbuf_copy(src);
    if (!dst) return nullptr;
    int w = gdk_pixbuf_get_width(dst);
    int h = gdk_pixbuf_get_height(dst);
    int stride = gdk_pixbuf_get_rowstride(dst);
    int channels = gdk_pixbuf_get_n_channels(dst);
    guchar* pixels = gdk_pixbuf_get_pixels(dst);
    if (!pixels || channels < 3) return dst;
    for (int y = 0; y < h; ++y) {
        guchar* row = pixels + y * stride;
        for (int x = 0; x < w; ++x) {
            guchar* px = row + x * channels;
            px[0] = 255 - px[0];
            px[1] = 255 - px[1];
            px[2] = 255 - px[2];
        }
    }
    return dst;
}

GdkPixbuf* PieceRenderer::tryLoadPiece(char p) {
    if (p == '.') return nullptr;
    std::vector<std::string> dirs{pieces_custom_dir(), pieces_default_dir()};
    std::vector<std::string> names{
        pieceShortName(p) + ".png",
        pieceLongName(p) + ".png"
    };
    for (const auto& dir : dirs) {
        for (const auto& name : names) {
            GdkPixbuf* pix = loadPieceFile(dir + "/" + name);
            if (pix) return pix;
        }
    }
    return nullptr;
}

void PieceRenderer::load(bool force) {
    if (loaded && !force) return;
    free();
    const char pieces[] = {'K','Q','R','B','N','P','k','q','r','b','n','p'};
    for (char p : pieces) {
        images[static_cast<unsigned char>(p)] = tryLoadPiece(p);
        if (images[static_cast<unsigned char>(p)]) {
            imagesInverted[static_cast<unsigned char>(p)] = makeInverted(images[static_cast<unsigned char>(p)]);
        }
    }
    loaded = true;
}

bool PieceRenderer::haveAll() {
    if (!loaded) load();
    const char pieces[] = {'K','Q','R','B','N','P','k','q','r','b','n','p'};
    for (char p : pieces) {
        if (!images[static_cast<unsigned char>(p)]) return false;
    }
    return true;
}

bool PieceRenderer::drawPiece(cairo_t* cr, char p, double x, double y, double cell, bool invert) {
    if (!loaded) load();
    GdkPixbuf* pix = invert ? imagesInverted[static_cast<unsigned char>(p)] : images[static_cast<unsigned char>(p)];
    if (!pix) pix = images[static_cast<unsigned char>(p)];
    if (!pix) return false;

    int pw = gdk_pixbuf_get_width(pix);
    int ph = gdk_pixbuf_get_height(pix);
    if (pw <= 0 || ph <= 0) return false;

    double target = cell * 0.86;
    double scale = target / std::max(pw, ph);
    double dx = x + (cell - pw * scale) / 2.0;
    double dy = y + (cell - ph * scale) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, dx, dy);
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, pix, 0, 0);
    cairo_paint(cr);
    cairo_restore(cr);
    return true;
}

void drawTextCentered(cairo_t* cr, const std::string& text, double x, double y, double w, double h, double size, bool bold) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);

    double fitted = std::max(8.0, size);
    cairo_text_extents_t ext{};
    cairo_set_font_size(cr, fitted);
    cairo_text_extents(cr, text.c_str(), &ext);

    double maxW = std::max(4.0, w - 10.0);
    double maxH = std::max(4.0, h - 6.0);
    if (ext.width > maxW || ext.height > maxH) {
        double scaleW = maxW / std::max(1.0, ext.width);
        double scaleH = maxH / std::max(1.0, ext.height);
        fitted = std::max(8.0, fitted * std::min(scaleW, scaleH));
        cairo_set_font_size(cr, fitted);
        cairo_text_extents(cr, text.c_str(), &ext);
    }

    double tx = x + (w - ext.width) / 2.0 - ext.x_bearing;
    double ty = y + (h - ext.height) / 2.0 - ext.y_bearing;
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, text.c_str());
}

void drawTextLeft(cairo_t* cr, const std::string& text, double x, double y, double size, bool bold) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text.c_str());
}

void drawButton(cairo_t* cr, const RectButton& b, double fontSize) {
    cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
    cairo_rectangle(cr, b.x, b.y, b.w, b.h);
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    drawTextCentered(cr, b.label, b.x, b.y, b.w, b.h, fontSize, true);
}

void drawToolbar(cairo_t* cr, const std::vector<RectButton>& buttons, int fontSize) {
    for (const auto& b : buttons) {
        drawButton(cr, b, std::max(14, fontSize - 4));
    }
}

void drawBoard(cairo_t* cr, const Layout& L, const Position& displayPos, const AppSettings& settings,
               PieceRenderer& pieceRenderer, int selected, const std::vector<Move>& selectedMoves,
               int lastFrom, int lastTo, int hintFrom, int hintTo) {
    bool invertPieces = settings.shouldInvertPieces();

    for (int vr = 0; vr < 8; ++vr) {
        for (int vf = 0; vf < 8; ++vf) {
            int br = settings.flipped ? 7 - vr : vr;
            int bf = settings.flipped ? 7 - vf : vf;
            int s = sq(br, bf);
            bool light = ((br + bf) % 2 == 0);
            if (light) cairo_set_source_rgb(cr, 0.82, 0.82, 0.78);
            else cairo_set_source_rgb(cr, 0.55, 0.55, 0.52);

            cairo_rectangle(cr, L.boardX + vf * L.cell, L.boardY + vr * L.cell, L.cell, L.cell);
            cairo_fill(cr);

            if (s == lastFrom || s == lastTo) {
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
                cairo_rectangle(cr, L.boardX + vf * L.cell + 4, L.boardY + vr * L.cell + 4, L.cell - 8, L.cell - 8);
                cairo_fill(cr);
            }

            if (s == hintFrom || s == hintTo) {
                cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.42);
                cairo_rectangle(cr, L.boardX + vf * L.cell + 8, L.boardY + vr * L.cell + 8, L.cell - 16, L.cell - 16);
                cairo_fill(cr);
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                cairo_set_line_width(cr, 4.0);
                cairo_rectangle(cr, L.boardX + vf * L.cell + 8, L.boardY + vr * L.cell + 8, L.cell - 16, L.cell - 16);
                cairo_stroke(cr);
                cairo_set_line_width(cr, 1.0);
            }

            if (s == selected) {
                cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                cairo_set_line_width(cr, 4.0);
                cairo_rectangle(cr, L.boardX + vf * L.cell + 3, L.boardY + vr * L.cell + 3, L.cell - 6, L.cell - 6);
                cairo_stroke(cr);
                cairo_set_line_width(cr, 1.0);
            }

            bool isTarget = false;
            for (const auto& m : selectedMoves) {
                if (m.to == s) { isTarget = true; break; }
            }
            if (isTarget) {
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.58);
                if (displayPos.b[s] == '.') {
                    cairo_arc(cr, L.boardX + vf * L.cell + L.cell / 2.0, L.boardY + vr * L.cell + L.cell / 2.0, L.cell * 0.12, 0, 6.28318);
                    cairo_fill(cr);
                } else {
                    cairo_set_line_width(cr, 5.0);
                    cairo_rectangle(cr, L.boardX + vf * L.cell + 5, L.boardY + vr * L.cell + 5, L.cell - 10, L.cell - 10);
                    cairo_stroke(cr);
                    cairo_set_line_width(cr, 1.0);
                }
            }

            char p = displayPos.b[s];
            if (p != '.') {
                bool drawn = settings.usePieceImages && pieceRenderer.drawPiece(cr, p, L.boardX + vf * L.cell, L.boardY + vr * L.cell, L.cell, invertPieces);
                if (!drawn) {
                    double cx = L.boardX + vf * L.cell + L.cell / 2.0;
                    double cy = L.boardY + vr * L.cell + L.cell / 2.0;
                    double rad = L.cell * 0.36;
                    bool drawAsWhite = is_white_piece(p);
                    if (invertPieces) drawAsWhite = !drawAsWhite;
                    if (drawAsWhite) {
                        cairo_set_source_rgb(cr, 0.96, 0.96, 0.92);
                        cairo_arc(cr, cx, cy, rad, 0, 6.28318);
                        cairo_fill_preserve(cr);
                        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                        cairo_set_line_width(cr, 2.0);
                        cairo_stroke(cr);
                        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
                    } else {
                        cairo_set_source_rgb(cr, 0.03, 0.03, 0.03);
                        cairo_arc(cr, cx, cy, rad, 0, 6.28318);
                        cairo_fill(cr);
                        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
                    }
                    drawTextCentered(cr, pieceLetter(p), L.boardX + vf * L.cell, L.boardY + vr * L.cell + 2, L.cell, L.cell, L.cell * 0.46, true);
                }
            }
        }
    }

    drawCoordinates(cr, L, settings);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, L.boardX, L.boardY, L.board, L.board);
    cairo_stroke(cr);
}

void drawCoordinates(cairo_t* cr, const Layout& L, const AppSettings& settings) {
    if (!settings.showCoordinates || L.coordMargin <= 0 || L.cell < 20) return;
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    int fs = std::max(12, std::min(L.coordMargin - 4, settings.uiFontSize - 6));

    // Vertical ranks (1-8) outside on the left
    for (int vr = 0; vr < 8; ++vr) {
        int br = settings.flipped ? 7 - vr : vr;
        std::string rank(1, static_cast<char>('8' - br));
        drawTextCentered(cr, rank, L.boardX - L.coordMargin, L.boardY + vr * L.cell,
                         L.coordMargin - 4, L.cell, fs, true);
    }

    // Horizontal files (a-h) outside at the bottom
    for (int vf = 0; vf < 8; ++vf) {
        int bf = settings.flipped ? 7 - vf : vf;
        std::string file(1, static_cast<char>('a' + bf));
        drawTextCentered(cr, file, L.boardX + vf * L.cell, L.boardY + L.board + 2,
                         L.cell, L.coordMargin - 2, fs, true);
    }
}

static std::vector<std::string> moveHistoryLines(const std::vector<std::string>& m, int maxLines) {
    std::vector<std::string> lines;
    for (size_t i = 0; i < m.size(); i += 2) {
        std::ostringstream os;
        os << (i / 2 + 1) << ". " << m[i];
        if (i + 1 < m.size()) os << "  " << m[i + 1];
        lines.push_back(os.str());
    }
    if (static_cast<int>(lines.size()) > maxLines) {
        lines.erase(lines.begin(), lines.begin() + (static_cast<int>(lines.size()) - maxLines));
    }
    return lines;
}

void drawMovePanel(cairo_t* cr, const Layout& L, const ChessGame& game, const AppSettings& settings) {
    if (!L.hasPanel) return;
    cairo_set_source_rgb(cr, 0.88, 0.88, 0.84);
    cairo_rectangle(cr, L.panelX, L.panelY, L.panelW, L.panelH);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
    drawTextCentered(cr, "Moves", L.panelX, L.panelY + 6, L.panelW, 30, std::max(16, std::min(26, settings.uiFontSize - 2)), true);
    int lineH = std::max(20, std::min(34, settings.uiFontSize + 4));
    int capturedReserve = std::max(170, std::min(250, settings.uiFontSize * 3 + 100));
    int maxLines = std::max(1, (L.panelH - 48 - capturedReserve) / lineH);
    auto lines = moveHistoryLines(game.uciMoves(), maxLines);
    int y = L.panelY + 48;
    for (const auto& line : lines) {
        drawTextLeft(cr, line, L.panelX + 12, y, std::max(13, std::min(24, settings.uiFontSize - 5)), false);
        y += lineH;
    }
}

static int capturedLabelWidth(int fontSize) {
    return std::max(260, std::min(430, fontSize * 8));
}

static int fitCapturedCell(int availableW, int requestedCell, int maxPieces, int fontSize) {
    if (maxPieces <= 0) return requestedCell;
    int labelW = capturedLabelWidth(fontSize);
    int usable = std::max(40, availableW - labelW - 18);
    int fit = (usable - std::max(0, maxPieces - 1) * 3) / maxPieces;
    return std::max(20, std::min(requestedCell, fit));
}

static void drawCapturedBox(cairo_t* cr, int x, int y, int w, int h) {
    cairo_set_source_rgb(cr, 0.78, 0.78, 0.74);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
}

static void drawCapturedRow(cairo_t* cr, const std::string& label, const std::vector<char>& pieces,
                            int x, int y, int w, int cell, int fs, PieceRenderer& pieceRenderer, bool invertPieces) {
    drawTextLeft(cr, label, x, y + fs + 3, fs, true);
    int px = x + capturedLabelWidth(fs + 2);
    int py = y;
    int maxX = x + w - cell - 4;
    for (char p : pieces) {
        if (px > maxX) break;
        if (!pieceRenderer.drawPiece(cr, p, px, py, cell, invertPieces)) {
            drawTextCentered(cr, pieceLetter(p), px, py, cell, cell, fs + 3, true);
        }
        px += cell + 3;
    }
}

void drawCapturedPieces(cairo_t* cr, const Layout& L, const ChessGame& game, const AppSettings& settings,
                        PieceRenderer& pieceRenderer, int reviewPly) {
    auto caps = (reviewPly >= 0) ? game.capturedPiecesAtPly(reviewPly) : game.capturedPieces();
    int baseCell = std::max(32, std::min(58, settings.uiFontSize + 14));
    int fs = std::max(22, std::min(38, settings.uiFontSize - 2));
    int maxPieces = std::max(static_cast<int>(caps.first.size()), static_cast<int>(caps.second.size()));
    bool invertPieces = settings.shouldInvertPieces();

    if (L.hasPanel) {
        int boxH = std::max(152, std::min(220, baseCell * 2 + fs + 42));
        int y = L.panelY + L.panelH - boxH - 8;
        if (y < L.panelY + 86) return;
        int cell = fitCapturedCell(L.panelW - 24, baseCell, maxPieces, settings.uiFontSize);
        int rowGap = 10;
        drawCapturedBox(cr, L.panelX + 4, y, L.panelW - 8, boxH);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        drawTextCentered(cr, "Captured", L.panelX + 8, y + 8, L.panelW - 16, fs + 8, fs, true);
        int rowY = y + fs + 24;
        drawCapturedRow(cr, "White captured", caps.first, L.panelX + 14, rowY, L.panelW - 28, cell, fs, pieceRenderer, invertPieces);
        drawCapturedRow(cr, "Black captured", caps.second, L.panelX + 14, rowY + cell + rowGap, L.panelW - 28, cell, fs, pieceRenderer, invertPieces);
    } else {
        int areaTop = L.boardY + L.board + L.coordMargin + 8;
        int areaBottom = L.H - L.statusH - 8;
        int available = areaBottom - areaTop;
        if (available <= 70) return;

        int rowGap = 10;
        int cell = std::min(baseCell, std::max(22, (available - fs - rowGap - 32) / 2));
        cell = fitCapturedCell(L.board - 24, cell, maxPieces, settings.uiFontSize);
        int boxH = std::min(available, std::max(132, fs + cell * 2 + rowGap + 34));
        int boxY = areaTop + std::max(0, (available - boxH) / 2);

        drawCapturedBox(cr, L.boardX, boxY, L.board, boxH);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        int y = boxY + fs + 12;
        drawCapturedRow(cr, "White captured", caps.first, L.boardX + 12, y, L.board - 24, cell, fs, pieceRenderer, invertPieces);
        drawCapturedRow(cr, "Black captured", caps.second, L.boardX + 12, y + cell + rowGap, L.board - 24, cell, fs, pieceRenderer, invertPieces);
    }
}

void drawConfirmNew(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.84, 620.0);
    double oh = 230;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 4.0);
    cairo_stroke(cr);
    drawTextCentered(cr, "Start a new game?", ox, oy + 20, ow, 42, settings.uiFontSize + 2, true);
    drawTextCentered(cr, "This will replace the current saved game.", ox + 20, oy + 76, ow - 40, 36, std::max(15, settings.uiFontSize - 4), false);
    for (const auto& b : buttons) {
        drawButton(cr, b, std::max(16, settings.uiFontSize - 2));
    }
}

void drawMoveConfirm(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, int from, int to, char promo, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.84, 620.0);
    double oh = 230;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 4.0);
    cairo_stroke(cr);
    std::string text = "Confirm move " + square_name(from) + "-" + square_name(to);
    if (promo) {
        text += "=";
        text.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(promo))));
    }
    text += "?";
    drawTextCentered(cr, text, ox + 20, oy + 24, ow - 40, 60, std::min(34, settings.uiFontSize + 3), true);
    for (const auto& b : buttons) {
        drawButton(cr, b, std::max(16, std::min(24, settings.uiFontSize - 2)));
    }
}

void drawGameOver(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const std::string& text, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.84, 650.0);
    double oh = 250;
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 4.0);
    cairo_stroke(cr);
    drawTextCentered(cr, text.empty() ? "Game Over" : text, ox + 20, oy + 28, ow - 40, 76, settings.uiFontSize + 5, true);
    for (const auto& b : buttons) {
        drawButton(cr, b, std::max(17, settings.uiFontSize - 1));
    }
}

void drawReviewControls(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, int ply, int total, const AppSettings& settings) {
    int w = std::min(L.W - 40, 680);
    int h = 92;
    int x = (L.W - w) / 2;
    int y = L.H - L.statusH - h - 8;
    if (y < L.boardY + L.board - h) y = L.boardY + L.board - h - 10;
    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, x, y, w, h);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
    drawTextCentered(cr, "Review: move " + std::to_string(ply) + " / " + std::to_string(total), x, y + 4, w, 30, std::max(13, std::min(22, settings.uiFontSize - 5)), true);
    for (const auto& b : buttons) {
        drawButton(cr, b, std::max(13, std::min(20, settings.uiFontSize - 6)));
    }
}

void drawPromotion(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, bool promoteWhite, PieceRenderer& pieceRenderer, const AppSettings& settings) {
    double ox = L.boardX + L.board * 0.1;
    double oy = L.boardY + L.board * 0.34;
    double ow = L.board * 0.8;
    double oh = std::max(230.0, L.board * 0.34);
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);
    drawTextCentered(cr, "Promote to", ox, oy + 10, ow, 46, settings.uiFontSize, true);

    bool invertPieces = settings.shouldInvertPieces();
    for (const auto& b : buttons) {
        cairo_set_source_rgb(cr, 0.96, 0.96, 0.94);
        cairo_rectangle(cr, b.x, b.y, b.w, b.h);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_stroke(cr);

        char displayPiece = promoteWhite ? static_cast<char>(std::toupper(static_cast<unsigned char>(b.tag))) : b.tag;
        double pieceSize = std::min(b.w, b.h) * 0.86;
        double px = b.x + (b.w - pieceSize) / 2.0;
        double py = b.y + (b.h - pieceSize) / 2.0;
        if (!pieceRenderer.drawPiece(cr, displayPiece, px, py, pieceSize, invertPieces)) {
            drawTextCentered(cr, b.label, b.x, b.y, b.w, b.h, std::max(30, settings.uiFontSize + 8), true);
        }
    }
}

void drawSettingsMain(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings, bool locked) {
    double ow = std::min(static_cast<double>(L.W) * 0.96, 1040.0);
    double oh = std::min(static_cast<double>(L.H) * 0.94, 1180.0);
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;

    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 4.0);
    cairo_stroke(cr);

    int sfs = settings.settingsFontSize();
    drawTextCentered(cr, "Settings", ox, oy + 12, ow, std::max(56, sfs + 14), sfs + 6, true);
    drawTextCentered(cr, locked ? "Engine side and Elo are locked after the first move." : "Set engine side and Elo before making the first move.",
                     ox + 22, oy + 70, ow - 44, std::max(38, sfs - 8), std::max(22, sfs - 18), false);

    for (const auto& b : buttons) {
        drawButton(cr, b, sfs);
    }
}

void drawSettingsSaveLoad(cairo_t* cr, const Layout& L, const std::vector<RectButton>& buttons, const AppSettings& settings) {
    double ow = std::min(static_cast<double>(L.W) * 0.96, 1040.0);
    double oh = std::min(static_cast<double>(L.H) * 0.94, 1180.0);
    double ox = (L.W - ow) / 2.0;
    double oy = (L.H - oh) / 2.0;

    cairo_set_source_rgb(cr, 0.94, 0.94, 0.90);
    cairo_rectangle(cr, ox, oy, ow, oh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 4.0);
    cairo_stroke(cr);

    int sfs = settings.settingsFontSize();
    drawTextCentered(cr, "Save / Load Games", ox, oy + 12, ow, std::max(56, sfs + 14), sfs + 4, true);
    drawTextCentered(cr, "Five local save slots. Loading replaces the current board.", ox + 20, oy + 72, ow - 40, std::max(38, sfs - 8), std::max(22, sfs - 18), false);

    int left = static_cast<int>(ox + 30);
    int y = static_cast<int>(oy + 126);
    int rowH = std::max(92, sfs + 48);
    int bh = std::max(62, std::min(82, sfs + 20));
    double fs = std::max(24, std::min(48, sfs - 8));

    for (int i = 1; i <= 5; ++i) {
        drawTextLeft(cr, slotSummary(i), left, y + std::max(38, bh - 12), fs, false);
        y += rowH;
    }

    for (const auto& b : buttons) {
        drawButton(cr, b, fs);
    }
}

} // namespace kindlechess
