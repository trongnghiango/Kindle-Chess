#include "chess.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace kindlechess {

std::string square_name(int s) {
    std::string out;
    out.push_back(static_cast<char>('a' + file_of(s)));
    out.push_back(static_cast<char>('8' - row_of(s)));
    return out;
}

int parse_square(const std::string& s) {
    if (s.size() < 2) return -1;
    int f = s[0] - 'a';
    int r = '8' - s[1];
    if (!in_bounds(r, f)) return -1;
    return sq(r, f);
}

std::string Position::key() const {
    std::string k;
    k.reserve(72);
    for (char c : b) k.push_back(c);
    k.push_back(white_to_move ? 'w' : 'b');
    k.push_back(castle_wk ? 'K' : '-');
    k.push_back(castle_wq ? 'Q' : '-');
    k.push_back(castle_bk ? 'k' : '-');
    k.push_back(castle_bq ? 'q' : '-');
    if (ep_square >= 0) k += square_name(ep_square);
    else k += "--";
    return k;
}

ChessGame::ChessGame() {
    reset();
}

void ChessGame::reset() {
    pos = Position();
    const char* start = "rnbqkbnrpppppppp................................PPPPPPPPRNBQKBNR";
    for (int i = 0; i < 64; ++i) pos.b[i] = start[i];
    history.clear();
    uci_moves.clear();
    san_moves.clear();
    captured_by_white.clear();
    captured_by_black.clear();
    message = "New game. White to move.";
}

std::string ChessGame::fen() const {
    std::ostringstream os;
    for (int r = 0; r < 8; ++r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            char p = pos.b[sq(r, f)];
            if (p == '.') {
                empty++;
            } else {
                if (empty) { os << empty; empty = 0; }
                os << p;
            }
        }
        if (empty) os << empty;
        if (r != 7) os << '/';
    }
    os << (pos.white_to_move ? " w " : " b ");
    std::string c;
    if (pos.castle_wk) c += 'K';
    if (pos.castle_wq) c += 'Q';
    if (pos.castle_bk) c += 'k';
    if (pos.castle_bq) c += 'q';
    os << (c.empty() ? "-" : c) << ' ';
    os << (pos.ep_square >= 0 ? square_name(pos.ep_square) : "-");
    os << ' ' << pos.halfmove_clock << ' ' << pos.fullmove_number;
    return os.str();
}

int ChessGame::pieceSortValue(char p) {
    switch (piece_type(p)) {
        case 'q': return 0;
        case 'r': return 1;
        case 'b': return 2;
        case 'n': return 3;
        case 'p': return 4;
        default: return 5;
    }
}

void ChessGame::sortCaptured(std::vector<char>& v) {
    std::sort(v.begin(), v.end(), [](char a, char b) {
        int va = pieceSortValue(a), vb = pieceSortValue(b);
        if (va != vb) return va < vb;
        return a < b;
    });
}

int ChessGame::clampIntLocal(int v, int lo, int hi) {
    return std::max(lo, std::min(hi, v));
}

char ChessGame::capturedPieceForMove(const Move& m) const {
    char moving = pos.b[m.from];
    if (m.en_passant) {
        int cap = is_white_piece(moving) ? m.to + 8 : m.to - 8;
        if (cap >= 0 && cap < 64) return pos.b[cap];
        return '.';
    }
    return pos.b[m.to];
}

bool ChessGame::enemyAt(int s, bool white) const {
    char p = pos.b[s];
    if (p == '.') return false;
    return is_white_piece(p) != white;
}

int ChessGame::kingSquare(bool white) const {
    char k = white ? 'K' : 'k';
    return kingSquareOn(pos.b.data(), k);
}

int ChessGame::kingSquareOn(const char* board, char kingChar) {
    for (int i = 0; i < 64; ++i) {
        if (board[i] == kingChar) return i;
    }
    return -1;
}

bool ChessGame::isSquareAttacked(int target, bool byWhite) const {
    return isSquareAttackedOn(pos.b.data(), target, byWhite);
}

bool ChessGame::isSquareAttackedOn(const char* board, int target, bool byWhite) {
    int r = row_of(target), f = file_of(target);

    // Pawns
    if (byWhite) {
        int pr = r + 1;
        if (pr < 8) {
            if (f > 0 && board[sq(pr, f - 1)] == 'P') return true;
            if (f < 7 && board[sq(pr, f + 1)] == 'P') return true;
        }
    } else {
        int pr = r - 1;
        if (pr >= 0) {
            if (f > 0 && board[sq(pr, f - 1)] == 'p') return true;
            if (f < 7 && board[sq(pr, f + 1)] == 'p') return true;
        }
    }

    // Knights
    static const int knight[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (auto& d : knight) {
        int rr = r + d[0], ff = f + d[1];
        if (!in_bounds(rr, ff)) continue;
        char p = board[sq(rr, ff)];
        if (p == (byWhite ? 'N' : 'n')) return true;
    }

    // Kings
    static const int king[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
    for (auto& d : king) {
        int rr = r + d[0], ff = f + d[1];
        if (!in_bounds(rr, ff)) continue;
        char p = board[sq(rr, ff)];
        if (p == (byWhite ? 'K' : 'k')) return true;
    }

    // Bishops & Queens
    static const int bishop[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
    for (auto& d : bishop) {
        int rr = r + d[0], ff = f + d[1];
        while (in_bounds(rr, ff)) {
            char p = board[sq(rr, ff)];
            if (p != '.') {
                if (is_white_piece(p) == byWhite && (piece_type(p) == 'b' || piece_type(p) == 'q')) return true;
                break;
            }
            rr += d[0]; ff += d[1];
        }
    }

    // Rooks & Queens
    static const int rook[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
    for (auto& d : rook) {
        int rr = r + d[0], ff = f + d[1];
        while (in_bounds(rr, ff)) {
            char p = board[sq(rr, ff)];
            if (p != '.') {
                if (is_white_piece(p) == byWhite && (piece_type(p) == 'r' || piece_type(p) == 'q')) return true;
                break;
            }
            rr += d[0]; ff += d[1];
        }
    }

    return false;
}

void ChessGame::addMove(std::vector<Move>& out, int from, int to, char promo, bool ep, bool castle) const {
    Move m;
    m.from = from;
    m.to = to;
    m.promotion = promo;
    m.en_passant = ep;
    m.castle = castle;
    out.push_back(m);
}

void ChessGame::addSlide(std::vector<Move>& out, int from, const int dirs[][2], int n, bool white) const {
    int r = row_of(from), f = file_of(from);
    for (int i = 0; i < n; ++i) {
        int rr = r + dirs[i][0], ff = f + dirs[i][1];
        while (in_bounds(rr, ff)) {
            int to = sq(rr, ff);
            char p = pos.b[to];
            if (p == '.') {
                addMove(out, from, to);
            } else {
                if (is_white_piece(p) != white) addMove(out, from, to);
                break;
            }
            rr += dirs[i][0]; ff += dirs[i][1];
        }
    }
}

void ChessGame::generatePseudo(std::vector<Move>& out) const {
    bool white = pos.white_to_move;
    for (int from = 0; from < 64; ++from) {
        char p = pos.b[from];
        if (p == '.' || is_white_piece(p) != white) continue;
        int r = row_of(from), f = file_of(from);
        char t = piece_type(p);
        if (t == 'p') {
            int dir = white ? -1 : 1;
            int startRow = white ? 6 : 1;
            int promRow = white ? 0 : 7;
            int oneR = r + dir;
            if (in_bounds(oneR, f) && pos.b[sq(oneR, f)] == '.') {
                int to = sq(oneR, f);
                if (oneR == promRow) {
                    addMove(out, from, to, 'q'); addMove(out, from, to, 'r');
                    addMove(out, from, to, 'b'); addMove(out, from, to, 'n');
                } else {
                    addMove(out, from, to);
                    int twoR = r + 2 * dir;
                    if (r == startRow && in_bounds(twoR, f) && pos.b[sq(twoR, f)] == '.') {
                        addMove(out, from, sq(twoR, f));
                    }
                }
            }
            for (int df : {-1, 1}) {
                int cr = r + dir, cf = f + df;
                if (!in_bounds(cr, cf)) continue;
                int to = sq(cr, cf);
                if (enemyAt(to, white) || to == pos.ep_square) {
                    bool ep = (to == pos.ep_square && pos.b[to] == '.');
                    if (cr == promRow) {
                        addMove(out, from, to, 'q', ep); addMove(out, from, to, 'r', ep);
                        addMove(out, from, to, 'b', ep); addMove(out, from, to, 'n', ep);
                    } else {
                        addMove(out, from, to, 0, ep);
                    }
                }
            }
        } else if (t == 'n') {
            static const int k[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
            for (auto& d : k) {
                int rr = r + d[0], ff = f + d[1];
                if (!in_bounds(rr, ff)) continue;
                int to = sq(rr, ff);
                if (pos.b[to] == '.' || is_white_piece(pos.b[to]) != white) addMove(out, from, to);
            }
        } else if (t == 'b') {
            static const int d[4][2] = {{-1,-1},{-1,1},{1,-1},{1,1}};
            addSlide(out, from, d, 4, white);
        } else if (t == 'r') {
            static const int d[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
            addSlide(out, from, d, 4, white);
        } else if (t == 'q') {
            static const int d[8][2] = {{-1,-1},{-1,1},{1,-1},{1,1},{-1,0},{1,0},{0,-1},{0,1}};
            addSlide(out, from, d, 8, white);
        } else if (t == 'k') {
            static const int k[8][2] = {{-1,-1},{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0},{1,1}};
            for (auto& d : k) {
                int rr = r + d[0], ff = f + d[1];
                if (!in_bounds(rr, ff)) continue;
                int to = sq(rr, ff);
                if (pos.b[to] == '.' || is_white_piece(pos.b[to]) != white) addMove(out, from, to);
            }
            if (white) {
                int e1 = sq(7,4), f1 = sq(7,5), g1 = sq(7,6), d1 = sq(7,3), c1 = sq(7,2), b1 = sq(7,1);
                if (from == e1 && pos.castle_wk && pos.b[f1] == '.' && pos.b[g1] == '.' &&
                    !isSquareAttacked(e1, false) && !isSquareAttacked(f1, false) && !isSquareAttacked(g1, false)) {
                    addMove(out, from, g1, 0, false, true);
                }
                if (from == e1 && pos.castle_wq && pos.b[d1] == '.' && pos.b[c1] == '.' && pos.b[b1] == '.' &&
                    !isSquareAttacked(e1, false) && !isSquareAttacked(d1, false) && !isSquareAttacked(c1, false)) {
                    addMove(out, from, c1, 0, false, true);
                }
            } else {
                int e8 = sq(0,4), f8 = sq(0,5), g8 = sq(0,6), d8 = sq(0,3), c8 = sq(0,2), b8 = sq(0,1);
                if (from == e8 && pos.castle_bk && pos.b[f8] == '.' && pos.b[g8] == '.' &&
                    !isSquareAttacked(e8, true) && !isSquareAttacked(f8, true) && !isSquareAttacked(g8, true)) {
                    addMove(out, from, g8, 0, false, true);
                }
                if (from == e8 && pos.castle_bq && pos.b[d8] == '.' && pos.b[c8] == '.' && pos.b[b8] == '.' &&
                    !isSquareAttacked(e8, true) && !isSquareAttacked(d8, true) && !isSquareAttacked(c8, true)) {
                    addMove(out, from, c8, 0, false, true);
                }
            }
        }
    }
}

bool ChessGame::isMoveLegal(const Move& m, bool movingWhite) const {
    char board[64];
    std::memcpy(board, pos.b.data(), 64);

    char moving = board[m.from];
    board[m.to] = moving;
    board[m.from] = '.';

    if (m.en_passant) {
        int cap = movingWhite ? m.to + 8 : m.to - 8;
        if (cap >= 0 && cap < 64) board[cap] = '.';
    }

    if (m.castle) {
        if (moving == 'K' && m.to == sq(7,6)) { board[sq(7,5)] = 'R'; board[sq(7,7)] = '.'; }
        if (moving == 'K' && m.to == sq(7,2)) { board[sq(7,3)] = 'R'; board[sq(7,0)] = '.'; }
        if (moving == 'k' && m.to == sq(0,6)) { board[sq(0,5)] = 'r'; board[sq(0,7)] = '.'; }
        if (moving == 'k' && m.to == sq(0,2)) { board[sq(0,3)] = 'r'; board[sq(0,0)] = '.'; }
    }

    char kingChar = movingWhite ? 'K' : 'k';
    int kingSq = (moving == kingChar) ? m.to : kingSquareOn(board, kingChar);
    if (kingSq < 0) return false;

    return !isSquareAttackedOn(board, kingSq, !movingWhite);
}

std::vector<Move> ChessGame::legalMoves() const {
    std::vector<Move> pseudos;
    pseudos.reserve(48);
    generatePseudo(pseudos);

    std::vector<Move> legal;
    legal.reserve(pseudos.size());
    bool movingWhite = pos.white_to_move;

    for (const Move& m : pseudos) {
        if (isMoveLegal(m, movingWhite)) {
            legal.push_back(m);
        }
    }
    return legal;
}

std::vector<Move> ChessGame::legalMovesFrom(int from) const {
    std::vector<Move> out;
    for (const auto& m : legalMoves()) {
        if (m.from == from) out.push_back(m);
    }
    return out;
}

bool ChessGame::needsPromotionChoice(int from, int to) const {
    for (const auto& m : legalMoves()) {
        if (m.from == from && m.to == to && m.promotion) return true;
    }
    return false;
}

void ChessGame::applyMoveInternal(const Move& m) {
    char moving = pos.b[m.from];
    char captured = pos.b[m.to];
    bool white = is_white_piece(moving);

    pos.ep_square = -1;
    if (piece_type(moving) == 'p' || captured != '.' || m.en_passant) pos.halfmove_clock = 0;
    else pos.halfmove_clock++;

    if (moving == 'K') { pos.castle_wk = false; pos.castle_wq = false; }
    if (moving == 'k') { pos.castle_bk = false; pos.castle_bq = false; }
    if (m.from == sq(7,0) || m.to == sq(7,0)) pos.castle_wq = false;
    if (m.from == sq(7,7) || m.to == sq(7,7)) pos.castle_wk = false;
    if (m.from == sq(0,0) || m.to == sq(0,0)) pos.castle_bq = false;
    if (m.from == sq(0,7) || m.to == sq(0,7)) pos.castle_bk = false;

    pos.b[m.to] = moving;
    pos.b[m.from] = '.';

    if (m.en_passant) {
        int cap = white ? m.to + 8 : m.to - 8;
        if (cap >= 0 && cap < 64) pos.b[cap] = '.';
    }

    if (m.castle) {
        if (moving == 'K' && m.to == sq(7,6)) { pos.b[sq(7,5)] = 'R'; pos.b[sq(7,7)] = '.'; }
        if (moving == 'K' && m.to == sq(7,2)) { pos.b[sq(7,3)] = 'R'; pos.b[sq(7,0)] = '.'; }
        if (moving == 'k' && m.to == sq(0,6)) { pos.b[sq(0,5)] = 'r'; pos.b[sq(0,7)] = '.'; }
        if (moving == 'k' && m.to == sq(0,2)) { pos.b[sq(0,3)] = 'r'; pos.b[sq(0,0)] = '.'; }
    }

    if (m.promotion) pos.b[m.to] = white ? static_cast<char>(std::toupper(m.promotion)) : m.promotion;

    if (piece_type(moving) == 'p' && std::abs(row_of(m.to) - row_of(m.from)) == 2) {
        pos.ep_square = sq((row_of(m.to) + row_of(m.from)) / 2, file_of(m.from));
    }

    if (!pos.white_to_move) pos.fullmove_number++;
    pos.white_to_move = !pos.white_to_move;
}

std::string ChessGame::sanForMove(const Move& m) const {
    char moving = pos.b[m.from];
    bool white = is_white_piece(moving);
    char t = piece_type(moving);
    if (t == 'k' && m.castle) return m.to > m.from ? "O-O" : "O-O-O";

    char cap = capturedPieceForMove(m);
    bool capture = cap != '.';
    std::string san;
    if (t != 'p') {
        char letter = ' ';
        if (t == 'n') letter = 'N';
        else if (t == 'b') letter = 'B';
        else if (t == 'r') letter = 'R';
        else if (t == 'q') letter = 'Q';
        else if (t == 'k') letter = 'K';
        san.push_back(letter);

        bool sameFile = false, sameRank = false, needDisambig = false;
        for (const auto& other : legalMoves()) {
            if (other.from == m.from || other.to != m.to) continue;
            char op = pos.b[other.from];
            if (op == '.' || is_white_piece(op) != white || piece_type(op) != t) continue;
            needDisambig = true;
            if (file_of(other.from) == file_of(m.from)) sameFile = true;
            if (row_of(other.from) == row_of(m.from)) sameRank = true;
        }
        if (needDisambig) {
            if (!sameFile) san.push_back(static_cast<char>('a' + file_of(m.from)));
            else if (!sameRank) san.push_back(static_cast<char>('8' - row_of(m.from)));
            else {
                san.push_back(static_cast<char>('a' + file_of(m.from)));
                san.push_back(static_cast<char>('8' - row_of(m.from)));
            }
        }
    } else if (capture) {
        san.push_back(static_cast<char>('a' + file_of(m.from)));
    }

    if (capture) san.push_back('x');
    san += square_name(m.to);
    if (m.promotion) {
        san += '=';
        san.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(m.promotion))));
    }

    // Check / checkmate notation check
    ChessGame after = *this;
    after.applyMoveInternal(m);
    int k = after.kingSquare(after.pos.white_to_move);
    bool inCheck = k >= 0 && after.isSquareAttacked(k, !after.pos.white_to_move);
    if (inCheck) {
        san += after.legalMoves().empty() ? "#" : "+";
    }
    return san;
}

bool ChessGame::makeMove(int from, int to, char promotion) {
    promotion = static_cast<char>(std::tolower(static_cast<unsigned char>(promotion)));
    auto moves = legalMoves();
    for (const auto& m : moves) {
        if (m.from != from || m.to != to) continue;
        if (m.promotion) {
            char want = promotion ? promotion : 'q';
            if (m.promotion != want) continue;
        }

        std::string uci = m.uci();
        std::string san = sanForMove(m);
        char cap = capturedPieceForMove(m);

        HistoryEntry entry;
        entry.pos = pos;
        entry.captured = cap;
        entry.uci = uci;
        entry.san = san;
        history.push_back(entry);

        if (cap != '.') {
            if (is_white_piece(pos.b[from])) {
                captured_by_white.push_back(cap);
                sortCaptured(captured_by_white);
            } else {
                captured_by_black.push_back(cap);
                sortCaptured(captured_by_black);
            }
        }

        applyMoveInternal(m);
        uci_moves.push_back(uci);
        san_moves.push_back(san);
        updateStatus();
        return true;
    }
    message = "Illegal move.";
    return false;
}

bool ChessGame::makeUciMove(const std::string& uci) {
    if (uci.size() < 4) return false;
    int from = parse_square(uci.substr(0, 2));
    int to = parse_square(uci.substr(2, 2));
    char promo = uci.size() >= 5 ? uci[4] : 0;
    return makeMove(from, to, promo);
}

bool ChessGame::undo() {
    if (history.empty()) {
        message = "Nothing to undo.";
        return false;
    }
    const auto& last = history.back();
    pos = last.pos;
    if (last.captured != '.') {
        // In last.pos, white_to_move is the side that made the move!
        if (last.pos.white_to_move) {
            auto it = std::find(captured_by_white.begin(), captured_by_white.end(), last.captured);
            if (it != captured_by_white.end()) captured_by_white.erase(it);
        } else {
            auto it = std::find(captured_by_black.begin(), captured_by_black.end(), last.captured);
            if (it != captured_by_black.end()) captured_by_black.erase(it);
        }
    }
    history.pop_back();
    if (!uci_moves.empty()) uci_moves.pop_back();
    if (!san_moves.empty()) san_moves.pop_back();
    updateStatus();
    return true;
}

void ChessGame::saveTo(const std::string& path) const {
    std::ofstream f(path.c_str());
    if (!f) return;
    f << "KINDLECHESS 1\n";
    f << "moves=";
    for (size_t i = 0; i < uci_moves.size(); ++i) {
        if (i) f << ' ';
        f << uci_moves[i];
    }
    f << "\n";
}

bool ChessGame::loadFrom(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f) return false;
    std::string line, movesLine;
    std::getline(f, line);
    while (std::getline(f, line)) {
        if (line.rfind("moves=", 0) == 0) movesLine = line.substr(6);
    }
    reset();
    std::istringstream ss(movesLine);
    std::string mv;
    bool any = false;
    while (ss >> mv) {
        if (!makeUciMove(mv)) {
            message = "Save file had an invalid move. Started a new game.";
            reset();
            return false;
        }
        any = true;
    }
    if (any) message = "Resumed saved game.";
    return any;
}

Position ChessGame::positionAfterPly(int ply) const {
    int total = static_cast<int>(uci_moves.size());
    int target = clampIntLocal(ply, 0, total);
    if (target == total) return pos;
    if (target < static_cast<int>(history.size())) {
        return history[target].pos;
    }
    // Fallback if history was cleared: replay
    ChessGame tmp;
    for (int i = 0; i < target; ++i) tmp.makeUciMove(uci_moves[i]);
    return tmp.pos;
}

std::string ChessGame::sanForUci(const std::string& uci) const {
    if (uci.size() < 4) return uci;
    int from = parse_square(uci.substr(0, 2));
    int to = parse_square(uci.substr(2, 2));
    char promo = uci.size() >= 5 ? static_cast<char>(std::tolower(static_cast<unsigned char>(uci[4]))) : 0;
    for (const auto& m : legalMoves()) {
        if (m.from != from || m.to != to) continue;
        if (m.promotion && promo && m.promotion != promo) continue;
        if (m.promotion && !promo) continue;
        return sanForMove(m);
    }
    return uci;
}

std::vector<std::string> ChessGame::sanMoveList() const {
    if (san_moves.size() == uci_moves.size()) {
        return san_moves;
    }
    std::vector<std::string> out;
    ChessGame tmp;
    for (const auto& u : uci_moves) {
        out.push_back(tmp.sanForUci(u));
        tmp.makeUciMove(u);
    }
    return out;
}

std::pair<std::vector<char>, std::vector<char>> ChessGame::capturedPieces() const {
    return {captured_by_white, captured_by_black};
}

std::pair<std::vector<char>, std::vector<char>> ChessGame::capturedPiecesAtPly(int ply) const {
    int total = static_cast<int>(uci_moves.size());
    int target = clampIntLocal(ply, 0, total);
    if (target == total) return capturedPieces();

    std::vector<char> byWhite, byBlack;
    for (int i = 0; i < target && i < static_cast<int>(history.size()); ++i) {
        char cap = history[i].captured;
        if (cap != '.') {
            if (history[i].pos.white_to_move) byWhite.push_back(cap);
            else byBlack.push_back(cap);
        }
    }
    sortCaptured(byWhite);
    sortCaptured(byBlack);
    return {byWhite, byBlack};
}

bool ChessGame::isInsufficientMaterial() const {
    std::vector<std::pair<char,int>> pieces;
    for (int i = 0; i < 64; ++i) {
        char p = pos.b[i];
        if (p == '.' || piece_type(p) == 'k') continue;
        pieces.push_back({p, i});
    }
    if (pieces.empty()) return true;
    if (pieces.size() == 1) {
        char t = piece_type(pieces[0].first);
        return t == 'b' || t == 'n';
    }
    bool allBishops = true;
    int color = -1;
    for (auto& pi : pieces) {
        if (piece_type(pi.first) != 'b') { allBishops = false; break; }
        int c = (row_of(pi.second) + file_of(pi.second)) & 1;
        if (color < 0) color = c;
        else if (c != color) allBishops = false;
    }
    return allBishops;
}

bool ChessGame::isThreefoldRepetition() const {
    if (history.empty()) return false;
    std::string currentKey = pos.key();
    int count = 1;

    // Scan backwards from history up to halfmove_clock limit
    int limit = std::min(pos.halfmove_clock, static_cast<int>(history.size()));
    for (int i = 0; i < limit; ++i) {
        int idx = static_cast<int>(history.size()) - 1 - i;
        if (history[idx].pos.key() == currentKey) {
            count++;
            if (count >= 3) return true;
        }
    }
    return false;
}

bool ChessGame::isInCheck() const {
    int k = kingSquare(pos.white_to_move);
    return k >= 0 && isSquareAttacked(k, !pos.white_to_move);
}

bool ChessGame::isCheckmate() const {
    return legalMoves().empty() && isInCheck();
}

bool ChessGame::isStalemate() const {
    return legalMoves().empty() && !isInCheck();
}

std::string ChessGame::gameOverMessage() const {
    if (isCheckmate()) return std::string(pos.white_to_move ? "Black" : "White") + " wins by checkmate.";
    if (isStalemate()) return "Draw by stalemate.";
    if (isInsufficientMaterial()) return "Draw by insufficient material.";
    if (isFiftyMoveDraw()) return "Draw by fifty-move rule.";
    if (isThreefoldRepetition()) return "Draw by threefold repetition.";
    return "";
}

std::string ChessGame::resultString() const {
    if (isCheckmate()) return pos.white_to_move ? "0-1" : "1-0";
    if (isStalemate() || isInsufficientMaterial() || isFiftyMoveDraw() || isThreefoldRepetition()) return "1/2-1/2";
    return "*";
}

void ChessGame::updateStatus() {
    auto moves = legalMoves();
    int k = kingSquare(pos.white_to_move);
    bool inCheck = k >= 0 && isSquareAttacked(k, !pos.white_to_move);
    if (moves.empty()) {
        if (inCheck) message = std::string(pos.white_to_move ? "White" : "Black") + " is checkmated.";
        else message = "Stalemate.";
    } else if (inCheck) {
        message = std::string(pos.white_to_move ? "White" : "Black") + " to move. Check.";
    } else {
        message = std::string(pos.white_to_move ? "White" : "Black") + " to move.";
    }
}

} // namespace kindlechess
