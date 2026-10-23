#include "../src/chess.hpp"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace kindlechess;

void test_initial_position() {
    ChessGame game;
    assert(game.whiteToMove() == true);
    auto moves = game.legalMoves();
    assert(moves.size() == 20);
    assert(game.gameOverMessage().empty());
    assert(game.resultString() == "*");
    std::cout << "[PASS] test_initial_position (20 legal moves)\n";
}

void test_scholars_mate() {
    ChessGame game;
    assert(game.makeUciMove("e2e4"));
    assert(game.makeUciMove("e7e5"));
    assert(game.makeUciMove("d1h5"));
    assert(game.makeUciMove("b8c6"));
    assert(game.makeUciMove("f1c4"));
    assert(game.makeUciMove("g8f6"));
    assert(game.makeUciMove("h5f7"));

    assert(game.isCheckmate() == true);
    assert(game.gameOverMessage() == "White wins by checkmate.");
    assert(game.resultString() == "1-0");
    assert(game.sanMoves().back() == "Qxf7#");

    auto caps = game.capturedPieces();
    assert(caps.first.size() == 1);
    assert(caps.first[0] == 'p');
    assert(caps.second.empty());

    assert(game.undo());
    assert(game.isCheckmate() == false);
    caps = game.capturedPieces();
    assert(caps.first.empty());

    std::cout << "[PASS] test_scholars_mate\n";
}

void test_en_passant() {
    ChessGame game;
    assert(game.makeUciMove("e2e4"));
    assert(game.makeUciMove("a7a6"));
    assert(game.makeUciMove("e4e5"));
    assert(game.makeUciMove("d7d5"));
    assert(game.state().ep_square == sq(2, 3));

    auto moves = game.legalMovesFrom(sq(3, 4));
    bool foundEp = false;
    for (const auto& m : moves) {
        if (m.to == sq(2, 3) && m.en_passant) foundEp = true;
    }
    assert(foundEp);

    assert(game.makeUciMove("e5d6"));
    assert(game.pieceAt(sq(3, 3)) == '.');
    assert(game.pieceAt(sq(2, 3)) == 'P');

    auto caps = game.capturedPieces();
    assert(caps.first.size() == 1 && caps.first[0] == 'p');

    assert(game.undo());
    assert(game.pieceAt(sq(3, 3)) == 'p');
    assert(game.pieceAt(sq(3, 4)) == 'P');
    assert(game.pieceAt(sq(2, 3)) == '.');
    std::cout << "[PASS] test_en_passant\n";
}

void test_castling() {
    ChessGame game;
    assert(game.makeUciMove("e2e4"));
    assert(game.makeUciMove("e7e5"));
    assert(game.makeUciMove("g1f3"));
    assert(game.makeUciMove("b8c6"));
    assert(game.makeUciMove("f1c4"));
    assert(game.makeUciMove("f8c5"));

    auto moves = game.legalMovesFrom(sq(7, 4));
    bool canCastle = false;
    for (const auto& m : moves) {
        if (m.to == sq(7, 6) && m.castle) canCastle = true;
    }
    assert(canCastle);

    assert(game.makeUciMove("e1g1"));
    assert(game.sanMoves().back() == "O-O");
    assert(game.pieceAt(sq(7, 6)) == 'K');
    assert(game.pieceAt(sq(7, 5)) == 'R');
    assert(game.pieceAt(sq(7, 4)) == '.');
    assert(game.pieceAt(sq(7, 7)) == '.');
    assert(!game.state().castle_wk);
    assert(!game.state().castle_wq);

    std::cout << "[PASS] test_castling\n";
}

void test_threefold_repetition() {
    ChessGame game;
    assert(game.makeUciMove("g1f3"));
    assert(game.makeUciMove("g8f6"));
    assert(game.makeUciMove("f3g1"));
    assert(game.makeUciMove("f6g8"));

    assert(!game.isThreefoldRepetition());

    assert(game.makeUciMove("g1f3"));
    assert(game.makeUciMove("g8f6"));
    assert(game.makeUciMove("f3g1"));
    assert(game.makeUciMove("f6g8"));

    assert(game.isThreefoldRepetition());
    assert(game.gameOverMessage() == "Draw by threefold repetition.");
    assert(game.resultString() == "1/2-1/2");
    std::cout << "[PASS] test_threefold_repetition\n";
}

void test_promotion() {
    ChessGame game;
    assert(game.makeUciMove("a2a4"));
    assert(game.makeUciMove("b7b5"));
    assert(game.makeUciMove("a4b5"));
    assert(game.makeUciMove("a7a5"));
    assert(game.makeUciMove("b5a6"));
    std::cout << "[PASS] test_promotion & intermediate moves\n";
}

void test_performance_legal_moves() {
    ChessGame game;
    auto start = std::chrono::high_resolution_clock::now();
    int iterations = 10000;
    int totalMoves = 0;
    for (int i = 0; i < iterations; ++i) {
        totalMoves += game.legalMoves().size();
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[PERF] 10,000 legalMoves() calls took " << ms << " ms ("
              << (ms / iterations * 1000.0) << " microseconds/call, total moves=" << totalMoves << ")\n";
}

int main() {
    std::cout << "Starting Chess Engine Unit Tests...\n";
    test_initial_position();
    test_scholars_mate();
    test_en_passant();
    test_castling();
    test_threefold_repetition();
    test_promotion();
    test_performance_legal_moves();
    std::cout << "All Chess Engine Unit Tests Passed!\n";
    return 0;
}
