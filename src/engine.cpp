#include "engine.hpp"
#include "config.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

namespace kindlechess {

UciEngine::UciEngine() = default;

UciEngine::~UciEngine() {
    stop();
}

long long UciEngine::nowMs() {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<long long>(tv.tv_sec) * 1000LL + tv.tv_usec / 1000LL;
}

void UciEngine::log(const std::string& msg) {
    ensure_data_dir();
    std::string path = engine_log_path();
    struct stat st{};
    // Rotate/truncate if log file exceeds 512 KB to protect Kindle storage
    if (stat(path.c_str(), &st) == 0 && st.st_size > 512 * 1024) {
        truncate(path.c_str(), 0);
    }
    std::ofstream f(path.c_str(), std::ios::app);
    if (!f) return;
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    f << "[" << tv.tv_sec << "." << tv.tv_usec << "] " << msg << "\n";
}

bool UciEngine::available(const std::string& path) const {
    return access(path.c_str(), X_OK) == 0;
}

bool UciEngine::ensureStarted(const std::string& path, std::string& err) {
    if (child > 0) return true;
    log("--- engine start requested ---");
    log("path=" + path);
    if (!available(path)) {
        err = "No executable UCI engine found at " + path;
        log(err);
        return false;
    }

    int inpipe[2] = {-1, -1};
    int outpipe[2] = {-1, -1};
    if (pipe(inpipe) != 0) {
        err = "pipe(inpipe) failed";
        log(err);
        return false;
    }
    if (pipe(outpipe) != 0) {
        close(inpipe[0]);
        close(inpipe[1]);
        err = "pipe(outpipe) failed";
        log(err);
        return false;
    }

    child = fork();
    if (child == 0) {
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        dup2(outpipe[1], STDERR_FILENO);
        close(inpipe[0]);
        close(inpipe[1]);
        close(outpipe[0]);
        close(outpipe[1]);
        execl(path.c_str(), path.c_str(), static_cast<char*>(nullptr));
        dprintf(STDERR_FILENO, "exec failed for %s: errno=%d %s\n", path.c_str(), errno, strerror(errno));
        _exit(127);
    }

    close(inpipe[0]);
    close(outpipe[1]);

    if (child < 0) {
        close(inpipe[1]);
        close(outpipe[0]);
        err = "fork() failed";
        log(err);
        return false;
    }

    write_fd = inpipe[1];
    read_fd = outpipe[0];
    read_buffer.clear();
    log("child pid=" + std::to_string(static_cast<long long>(child)));

    sendLine("uci");
    if (!waitForToken("uciok", 20000)) {
        err = "Engine did not answer uciok. See data/engine.log.";
        log(err);
        stop();
        return false;
    }

    sendLine("setoption name Threads value 1");
    sendLine("setoption name Hash value 16");
    sendLine("setoption name Ponder value false");
    sendLine("setoption name MultiPV value 1");
    sendLine("isready");
    if (!waitForToken("readyok", 15000)) {
        err = "Engine did not answer readyok. See data/engine.log.";
        log(err);
        stop();
        return false;
    }

    sendLine("ucinewgame");
    sendLine("isready");
    waitForToken("readyok", 15000);
    log("engine initialized successfully");
    return true;
}

std::string UciEngine::bestMove(const std::string& path, const std::vector<std::string>& moves,
                               int movetimeMs, int targetElo, std::string& err) {
    if (!ensureStarted(path, err)) return "";
    configureStrength(targetElo);
    std::ostringstream poscmd;
    poscmd << "position startpos";
    if (!moves.empty()) {
        poscmd << " moves";
        for (const auto& m : moves) poscmd << ' ' << m;
    }
    sendLine(poscmd.str());
    sendLine("go movetime " + std::to_string(movetimeMs));

    std::string line;
    int timeout = std::max(3000, movetimeMs + 8000);
    long long start = nowMs();
    while (nowMs() - start < timeout) {
        if (!readLine(line, 250)) continue;
        log("<< " + line);
        if (line.rfind("bestmove ", 0) == 0) {
            std::istringstream ss(line);
            std::string tag, mv;
            ss >> tag >> mv;
            return (mv == "(none)") ? "" : mv;
        }
    }
    err = "Engine timed out waiting for bestmove. See data/engine.log.";
    log(err);
    return "";
}

void UciEngine::stop() {
    if (child > 0) {
        log("stopping engine pid=" + std::to_string(static_cast<long long>(child)));
        sendLine("quit");
        waitForExit(500);
        if (child > 0) {
            kill(child, SIGTERM);
            waitForExit(500);
        }
        if (child > 0) {
            kill(child, SIGKILL);
            waitForExit(500);
        }
    }
    if (write_fd >= 0) close(write_fd);
    if (read_fd >= 0) close(read_fd);
    child = -1;
    write_fd = -1;
    read_fd = -1;
    read_buffer.clear();
}

void UciEngine::configureStrength(int targetElo) {
    int elo = std::max(250, std::min(3000, targetElo));
    int skill = std::max(0, std::min(20, (elo - 250) * 20 / 2750));
    int uciElo = std::max(1320, std::min(3190, elo));
    sendLine("setoption name Skill Level value " + std::to_string(skill));
    sendLine("setoption name UCI_LimitStrength value true");
    sendLine("setoption name UCI_Elo value " + std::to_string(uciElo));
    sendLine("isready");
    waitForToken("readyok", 5000);
}

void UciEngine::waitForExit(int timeoutMs) {
    if (child <= 0) return;
    long long end = nowMs() + timeoutMs;
    while (nowMs() < end) {
        int status = 0;
        pid_t r = waitpid(child, &status, WNOHANG);
        if (r == child) {
            log("engine exited status=" + std::to_string(status));
            child = -1;
            return;
        }
        usleep(50000);
    }
}

bool UciEngine::sendLine(const std::string& s) {
    if (write_fd < 0) return false;
    log(">> " + s);
    std::string out = s + "\n";
    return write(write_fd, out.c_str(), out.size()) == static_cast<ssize_t>(out.size());
}

bool UciEngine::readLine(std::string& line, int timeoutMs) {
    line.clear();
    long long end = nowMs() + timeoutMs;
    while (nowMs() < end) {
        auto nl = read_buffer.find('\n');
        if (nl != std::string::npos) {
            line = read_buffer.substr(0, nl);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            read_buffer.erase(0, nl + 1);
            return true;
        }

        fd_set set;
        FD_ZERO(&set);
        FD_SET(read_fd, &set);
        long long remain = end - nowMs();
        if (remain <= 0) break;
        struct timeval tv{};
        tv.tv_sec = remain / 1000;
        tv.tv_usec = (remain % 1000) * 1000;
        int r = select(read_fd + 1, &set, nullptr, nullptr, &tv);
        if (r <= 0) continue;

        char buf[1024];
        ssize_t n = read(read_fd, buf, sizeof(buf));
        if (n <= 0) {
            log("engine pipe closed while reading");
            return false;
        }
        read_buffer.append(buf, n);
    }
    return false;
}

bool UciEngine::waitForToken(const std::string& token, int timeoutMs) {
    std::string line;
    long long start = nowMs();
    while (nowMs() - start < timeoutMs) {
        if (readLine(line, 250)) {
            log("<< " + line);
            if (line.find(token) != std::string::npos) return true;
        }
    }
    log("timeout waiting for token=" + token);
    return false;
}

} // namespace kindlechess
