#pragma once

#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace kindlechess {

inline bool in_bounds(int r, int f) { return r >= 0 && r < 8 && f >= 0 && f < 8; }
inline int sq(int r, int f) { return r * 8 + f; }
inline int row_of(int s) { return s / 8; }
inline int file_of(int s) { return s % 8; }
inline bool is_white_piece(char p) { return p >= 'A' && p <= 'Z'; }
inline bool is_black_piece(char p) { return p >= 'a' && p <= 'z'; }
inline bool is_empty(char p) { return p == '.'; }
inline bool same_color(char a, char b) {
    if (is_empty(a) || is_empty(b)) return false;
    return is_white_piece(a) == is_white_piece(b);
}
inline char piece_type(char p) { return static_cast<char>(std::tolower(static_cast<unsigned char>(p))); }

std::string square_name(int s);
int parse_square(const std::string& s);

struct Move {
    int from = -1;
    int to = -1;
    char promotion = 0;      // lowercase q/r/b/n, 0 otherwise
    bool en_passant = false;
    bool castle = false;

    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && promotion == o.promotion &&
               en_passant == o.en_passant && castle == o.castle;
    }

    std::string uci() const {
        std::string res = square_name(from) + square_name(to);
        if (promotion) res.push_back(promotion);
        return res;
    }
};

struct Position {
    std::array<char, 64> b{};
    bool white_to_move = true;
    bool castle_wk = true, castle_wq = true, castle_bk = true, castle_bq = true;
    int ep_square = -1;
    int halfmove_clock = 0;
    int fullmove_number = 1;

    std::string key() const;
};

struct HistoryEntry {
    Position pos;
    char captured = '.';
    std::string uci;
    std::string san;
};

class ChessGame {
public:
    ChessGame();

    void reset();

    const Position& state() const { return pos; }
    const std::string& status() const { return message; }
    const std::vector<std::string>& uciMoves() const { return uci_moves; }
    const std::vector<std::string>& sanMoves() const { return san_moves; }
    bool whiteToMove() const { return pos.white_to_move; }
    char pieceAt(int s) const { return pos.b[s]; }

    std::string fen() const;

    std::vector<Move> legalMoves() const;
    std::vector<Move> legalMovesFrom(int from) const;
    bool needsPromotionChoice(int from, int to) const;

    bool makeMove(int from, int to, char promotion = 0);
    bool makeUciMove(const std::string& uci);
    bool undo();

    void saveTo(const std::string& path) const;
    bool loadFrom(const std::string& path);

    Position positionAfterPly(int ply) const;
    std::string sanForUci(const std::string& uci) const;
    std::vector<std::string> sanMoveList() const;

    std::pair<std::vector<char>, std::vector<char>> capturedPieces() const;
    std::pair<std::vector<char>, std::vector<char>> capturedPiecesAtPly(int ply) const;

    bool isInsufficientMaterial() const;
    bool isFiftyMoveDraw() const { return pos.halfmove_clock >= 100; }
    bool isThreefoldRepetition() const;
    bool isCheckmate() const;
    bool isStalemate() const;
    bool isInCheck() const;

    std::string gameOverMessage() const;
    std::string resultString() const;

private:
    Position pos;
    std::vector<HistoryEntry> history;
    std::vector<std::string> uci_moves;
    std::vector<std::string> san_moves;
    std::vector<char> captured_by_white;
    std::vector<char> captured_by_black;
    std::string message;

    static int pieceSortValue(char p);
    static void sortCaptured(std::vector<char>& v);
    static int clampIntLocal(int v, int lo, int hi);

    char capturedPieceForMove(const Move& m) const;
    std::string sanForMove(const Move& m) const;

    bool enemyAt(int s, bool white) const;
    int kingSquare(bool white) const;
    static int kingSquareOn(const char* board, char kingChar);
    bool isSquareAttacked(int target, bool byWhite) const;
    static bool isSquareAttackedOn(const char* board, int target, bool byWhite);

    void addMove(std::vector<Move>& out, int from, int to, char promo = 0, bool ep = false, bool castle = false) const;
    void addSlide(std::vector<Move>& out, int from, const int dirs[][2], int n, bool white) const;
    void generatePseudo(std::vector<Move>& out) const;

    bool isMoveLegal(const Move& m, bool movingWhite) const;
    void applyMoveInternal(const Move& m);
    void updateStatus();
};

} // namespace kindlechess
