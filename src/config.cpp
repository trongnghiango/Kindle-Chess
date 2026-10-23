#include "config.hpp"
#include "chess.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace kindlechess {

static int clampInt(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

int AppSettings::currentEngineElo() const {
    int idx = clampInt(engineEloIndex, 0, static_cast<int>(engineElos.size()) - 1);
    return engineElos[idx];
}

int AppSettings::engineMovetime() const {
    return clampInt(currentEngineElo(), 250, 3000);
}

int AppSettings::settingsFontSize() const {
    return clampInt(uiFontSize + 20, 28, 64);
}

std::string AppSettings::darkPieceModeLabel() const {
    if (darkPieceMode == 1) return "Off";
    if (darkPieceMode == 2) return "On";
    return "Auto";
}

std::string AppSettings::engineModeLabel() const {
    if (engineMode == EngineMode::Off) return "Engine: Off";
    if (engineMode == EngineMode::Black) return "Engine: Black";
    return "Engine: White";
}

bool AppSettings::shouldInvertPieces() const {
    if (darkPieceMode == 1) return false;
    if (darkPieceMode == 2) return true;
    return detectKindleDarkMode();
}

std::string home_dir() {
    const char* h = std::getenv("KINDLECHESS_HOME");
    if (h && *h) return h;
    return "/mnt/us/extensions/kindlechess";
}

std::string save_path() { return home_dir() + "/data/save.txt"; }
std::string settings_path() { return home_dir() + "/data/settings.txt"; }
std::string engine_log_path() { return home_dir() + "/data/engine.log"; }
std::string games_dir() { return home_dir() + "/data/games"; }
std::string slots_dir() { return home_dir() + "/data/slots"; }
std::string slot_path(int n) { return slots_dir() + "/slot" + std::to_string(n) + ".txt"; }
std::string pieces_custom_dir() { return home_dir() + "/pieces/custom"; }
std::string pieces_default_dir() { return home_dir() + "/pieces/default"; }

std::string engine_path() {
    const char* e = std::getenv("KINDLECHESS_ENGINE");
    if (e && *e) return e;
    return home_dir() + "/bin/stockfish";
}

void ensure_data_dir() {
    std::string dir = home_dir() + "/data";
    mkdir(dir.c_str(), 0755);
}

void ensure_games_dir() {
    ensure_data_dir();
    mkdir(games_dir().c_str(), 0755);
}

void ensure_slots_dir() {
    ensure_data_dir();
    mkdir(slots_dir().c_str(), 0755);
}

static std::string nowTimestampForFile() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char buf[64];
    if (tm) std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", tm);
    else std::snprintf(buf, sizeof(buf), "%ld", static_cast<long>(t));
    return buf;
}

static std::string todayForPGN() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    char buf[32];
    if (tm) std::strftime(buf, sizeof(buf), "%Y.%m.%d", tm);
    else std::snprintf(buf, sizeof(buf), "????.??.??");
    return buf;
}

static bool fileContainsDarkModeToken(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        std::string l = line;
        std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if ((l.find("dark") != std::string::npos || l.find("night") != std::string::npos || l.find("invert") != std::string::npos) &&
            (l.find("true") != std::string::npos || l.find("=1") != std::string::npos || l.find(":1") != std::string::npos || l.find("on") != std::string::npos)) {
            return true;
        }
    }
    return false;
}

bool detectKindleDarkMode() {
    const char* env = std::getenv("KINDLECHESS_DARK_MODE");
    if (env && (*env == '1' || std::strcmp(env, "true") == 0 || std::strcmp(env, "on") == 0)) return true;
    std::vector<std::string> paths{
        "/var/local/java/prefs/com.amazon.ebook.framework/prefs",
        "/var/local/java/prefs/com.amazon.ebook.booklet.reader/reader.pref",
        "/var/local/system/darkmode",
        "/var/local/system/nightmode"
    };
    for (const auto& path : paths) {
        if (fileContainsDarkModeToken(path)) return true;
    }
    return false;
}

void saveAppSettings(const AppSettings& s) {
    ensure_data_dir();
    std::ofstream f(settings_path().c_str());
    if (!f) return;
    f << "KINDLECHESS_SETTINGS 1\n";
    f << "ui_font_size=" << s.uiFontSize << "\n";
    f << "show_coordinates=" << (s.showCoordinates ? 1 : 0) << "\n";
    f << "show_move_list=" << (s.showMoveList ? 1 : 0) << "\n";
    f << "engine_mode=" << (s.engineMode == EngineMode::Off ? 0 : (s.engineMode == EngineMode::Black ? 1 : 2)) << "\n";
    f << "engine_elo=" << s.currentEngineElo() << "\n";
    f << "flipped=0\n";
    f << "use_piece_images=" << (s.usePieceImages ? 1 : 0) << "\n";
    f << "confirm_moves=" << (s.confirmMoves ? 1 : 0) << "\n";
    f << "dark_piece_mode=" << s.darkPieceMode << "\n";
}

void loadAppSettings(AppSettings& s) {
    std::ifstream f(settings_path().c_str());
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        int n = std::atoi(val.c_str());
        if (key == "ui_font_size") s.uiFontSize = clampInt(n, 14, 60);
        else if (key == "show_coordinates") s.showCoordinates = (n != 0);
        else if (key == "show_move_list") s.showMoveList = (n != 0);
        else if (key == "engine_mode") s.engineMode = (n == 1 ? EngineMode::Black : (n == 2 ? EngineMode::White : EngineMode::Off));
        else if (key == "engine_elo") {
            int best = 0;
            for (size_t i = 1; i < s.engineElos.size(); ++i) {
                if (std::abs(s.engineElos[i] - n) < std::abs(s.engineElos[best] - n)) best = static_cast<int>(i);
            }
            s.engineEloIndex = best;
        }
        else if (key == "flipped") s.flipped = false;
        else if (key == "use_piece_images") s.usePieceImages = (n != 0);
        else if (key == "confirm_moves") s.confirmMoves = (n != 0);
        else if (key == "dark_piece_mode") s.darkPieceMode = clampInt(n, 0, 2);
    }
}

std::string exportPGN(const ChessGame& game) {
    ensure_games_dir();
    std::string file = games_dir() + "/game-" + nowTimestampForFile() + ".pgn";
    std::ofstream f(file.c_str());
    if (!f) return "Could not export PGN.";

    std::string result = game.resultString();
    f << "[Event \"KindleChess\"]\n";
    f << "[Site \"Kindle Paperwhite\"]\n";
    f << "[Date \"" << todayForPGN() << "\"]\n";
    f << "[Round \"-\"]\n";
    f << "[White \"White\"]\n";
    f << "[Black \"Black\"]\n";
    f << "[Result \"" << result << "\"]\n\n";

    auto sans = game.sanMoveList();
    int chars = 0;
    for (size_t i = 0; i < sans.size(); ++i) {
        std::ostringstream token;
        if (i % 2 == 0) token << (i / 2 + 1) << ". ";
        token << sans[i] << ' ';
        std::string t = token.str();
        if (chars + static_cast<int>(t.size()) > 78) { f << "\n"; chars = 0; }
        f << t;
        chars += static_cast<int>(t.size());
    }
    if (chars + static_cast<int>(result.size()) + 1 > 78) f << "\n";
    f << result << "\n";
    return "PGN exported: " + file;
}

std::string slotSummary(int n) {
    std::string path = slot_path(n);
    std::ifstream f(path.c_str());
    if (!f) return "Slot " + std::to_string(n) + ": Empty";
    ChessGame tmp;
    if (!tmp.loadFrom(path)) return "Slot " + std::to_string(n) + ": Invalid";
    std::ostringstream os;
    os << "Slot " << n << ": " << tmp.uciMoves().size() << " ply, "
       << (tmp.whiteToMove() ? "White" : "Black") << " to move";
    std::string over = tmp.gameOverMessage();
    if (!over.empty()) os << ", finished";
    return os.str();
}

bool saveSlot(int n, const ChessGame& game) {
    ensure_slots_dir();
    game.saveTo(slot_path(n));
    return true;
}

bool loadSlot(int n, ChessGame& game) {
    ChessGame tmp;
    if (!tmp.loadFrom(slot_path(n))) return false;
    game = tmp;
    game.saveTo(save_path());
    return true;
}

bool deleteSlot(int n) {
    return unlink(slot_path(n).c_str()) == 0;
}

} // namespace kindlechess
