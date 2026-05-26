#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace IBS {

// ============================================================
//  AuditLogger — append-only structured log
//  Thread-safe; every bank event gets a line in audit.log
// ============================================================
class AuditLogger {
public:
    enum class Level { INFO, WARN, ERROR, SECURITY, TRANSACTION };

    static AuditLogger& instance() {
        static AuditLogger inst;
        return inst;
    }

    void open(const std::string& path = "data/audit.log") {
        std::lock_guard<std::mutex> lk(mtx_);
        if (file_.is_open()) file_.close();
        file_.open(path, std::ios::app);
        if (!file_.is_open())
            std::cerr << "[AuditLogger] WARNING: Cannot open log file: " << path << "\n";
        log_path_ = path;
    }

    void log(Level lvl, const std::string& actor,
             const std::string& action, const std::string& detail = "") {
        std::lock_guard<std::mutex> lk(mtx_);
        std::string line = format_line(lvl, actor, action, detail);
        if (file_.is_open()) { file_ << line; file_.flush(); }
        if (echo_to_console_) std::cout << line;
    }

    void info    (const std::string& a, const std::string& b, const std::string& c="") { log(Level::INFO,        a,b,c); }
    void warn    (const std::string& a, const std::string& b, const std::string& c="") { log(Level::WARN,        a,b,c); }
    void error   (const std::string& a, const std::string& b, const std::string& c="") { log(Level::ERROR,       a,b,c); }
    void security(const std::string& a, const std::string& b, const std::string& c="") { log(Level::SECURITY,    a,b,c); }
    void txn     (const std::string& a, const std::string& b, const std::string& c="") { log(Level::TRANSACTION, a,b,c); }

    void set_echo(bool v) { echo_to_console_ = v; }
    const std::string& path() const { return log_path_; }

private:
    AuditLogger() = default;
    ~AuditLogger() { if (file_.is_open()) file_.close(); }
    AuditLogger(const AuditLogger&) = delete;
    AuditLogger& operator=(const AuditLogger&) = delete;

    static std::string now_str() {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    static std::string level_str(Level l) {
        switch (l) {
            case Level::INFO:        return "INFO ";
            case Level::WARN:        return "WARN ";
            case Level::ERROR:       return "ERROR";
            case Level::SECURITY:    return "SEC  ";
            case Level::TRANSACTION: return "TXN  ";
        }
        return "?    ";
    }

    std::string format_line(Level l, const std::string& actor,
                            const std::string& action, const std::string& detail) {
        std::ostringstream ss;
        ss << "[" << now_str() << "] "
           << "[" << level_str(l) << "] "
           << std::left << std::setw(20) << actor << " | "
           << std::setw(30) << action;
        if (!detail.empty()) ss << " | " << detail;
        ss << "\n";
        return ss.str();
    }

    std::ofstream file_;
    std::mutex    mtx_;
    std::string   log_path_;
    bool          echo_to_console_ = false;
};

// Convenience macro
#define AUDIT_INFO(actor, action, ...)     IBS::AuditLogger::instance().info    (actor, action, ##__VA_ARGS__)
#define AUDIT_WARN(actor, action, ...)     IBS::AuditLogger::instance().warn    (actor, action, ##__VA_ARGS__)
#define AUDIT_ERROR(actor, action, ...)    IBS::AuditLogger::instance().error   (actor, action, ##__VA_ARGS__)
#define AUDIT_SECURITY(actor, action, ...) IBS::AuditLogger::instance().security(actor, action, ##__VA_ARGS__)
#define AUDIT_TXN(actor, action, ...)      IBS::AuditLogger::instance().txn     (actor, action, ##__VA_ARGS__)

} // namespace IBS
