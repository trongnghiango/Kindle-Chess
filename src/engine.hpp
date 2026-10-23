#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace kindlechess {

class UciEngine {
public:
    UciEngine();
    ~UciEngine();

    UciEngine(const UciEngine&) = delete;
    UciEngine& operator=(const UciEngine&) = delete;

    bool available(const std::string& path) const;
    bool ensureStarted(const std::string& path, std::string& err);
    std::string bestMove(const std::string& path, const std::vector<std::string>& moves,
                         int movetimeMs, int targetElo, std::string& err);
    void stop();

    static void log(const std::string& msg);

private:
    pid_t child = -1;
    int write_fd = -1;
    int read_fd = -1;
    std::string read_buffer;

    void configureStrength(int targetElo);
    void waitForExit(int timeoutMs);
    bool sendLine(const std::string& s);
    bool readLine(std::string& line, int timeoutMs);
    bool waitForToken(const std::string& token, int timeoutMs);

    static long long nowMs();
};

} // namespace kindlechess
