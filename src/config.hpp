#pragma once

#include <string>
#include <vector>

namespace kindlechess {

class ChessGame;

enum class EngineMode { Off, Black, White };

struct AppSettings {
    int uiFontSize = 40;
    bool showCoordinates = true;
    bool showMoveList = true;
    EngineMode engineMode = EngineMode::Off;
    int engineEloIndex = 5;
    std::vector<int> engineElos{250, 500, 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000};
    bool flipped = false;
    bool usePieceImages = true;
    bool confirmMoves = false;
    // 0 = Auto, 1 = Off, 2 = On
    int darkPieceMode = 0;

    int currentEngineElo() const;
    int engineMovetime() const;
    int settingsFontSize() const;
    std::string darkPieceModeLabel() const;
    std::string engineModeLabel() const;
    bool shouldInvertPieces() const;
};

// Paths
std::string home_dir();
std::string save_path();
std::string settings_path();
std::string engine_log_path();
std::string games_dir();
std::string slots_dir();
std::string slot_path(int n);
std::string pieces_custom_dir();
std::string pieces_default_dir();
std::string engine_path();

// Directory creation helpers
void ensure_data_dir();
void ensure_games_dir();
void ensure_slots_dir();

// Settings & File operations
void saveAppSettings(const AppSettings& settings);
void loadAppSettings(AppSettings& settings);

std::string exportPGN(const ChessGame& game);
std::string slotSummary(int n);
bool saveSlot(int n, const ChessGame& game);
bool loadSlot(int n, ChessGame& game);
bool deleteSlot(int n);

// Kindle system checks
bool detectKindleDarkMode();

} // namespace kindlechess
