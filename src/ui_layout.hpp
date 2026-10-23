#pragma once

#include "config.hpp"
#include <string>
#include <vector>

namespace kindlechess {

struct RectButton {
    std::string label;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    char tag = 0; // for promotion pieces or specific actions

    bool contains(int px, int py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct Layout {
    int W = 0, H = 0;
    int margin = 8;
    int topH = 64;
    int statusH = 76;
    int coordMargin = 0;
    int board = 0, boardX = 0, boardY = 0, cell = 0;
    bool hasPanel = false;
    int panelX = 0, panelY = 0, panelW = 0, panelH = 0;
    int capturedH = 0;

    int squareFromXY(int x, int y, bool flipped) const;
};

Layout computeLayout(int width, int height, const AppSettings& settings);

std::vector<RectButton> computeToolbarButtons(const Layout& L, const AppSettings& settings);
std::vector<RectButton> computeConfirmNewButtons(const Layout& L, const AppSettings& settings);
std::vector<RectButton> computeMoveConfirmButtons(const Layout& L, const AppSettings& settings);
std::vector<RectButton> computeGameOverButtons(const Layout& L, const AppSettings& settings);
std::vector<RectButton> computeReviewButtons(const Layout& L, const AppSettings& settings);
std::vector<RectButton> computePromotionButtons(const Layout& L);

std::vector<RectButton> computeSettingsMainButtons(const Layout& L, const AppSettings& settings, bool locked);
std::vector<RectButton> computeSettingsSaveLoadButtons(const Layout& L, const AppSettings& settings);

} // namespace kindlechess
