// Scan a directory of log files, one task per file, merge the counts.
// Generates sample logs on the first run if the directory is missing.

#include "threadpool.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct Counts {
    long lines  = 0;
    long errors = 0;
    long warns  = 0;

    void add(const Counts& other) {
        lines  += other.lines;
        errors += other.errors;
        warns  += other.warns;
    }
};

static Counts scanFile(const std::string& path) {
    Counts c;
    std::ifstream in(path);
    std::string line;

    while (std::getline(in, line)) {
        c.lines++;
        if (line.find("ERROR") != std::string::npos) {
            c.errors++;
        } else if (line.find("WARN") != std::string::npos) {
            c.warns++;
        }
    }
    return c;
}

static void makeLogFiles(const std::string& dir, int fileCount, int linesPerFile) {
    fs::create_directories(dir);

    const char* levels[] = {"INFO", "INFO", "INFO", "DEBUG", "WARN", "ERROR"};

    for (int f = 0; f < fileCount; f++) {
        std::ofstream out(dir + "/app_" + std::to_string(f) + ".log");
        for (int i = 0; i < linesPerFile; i++) {
            const char* lvl = levels[(i * 7 + f * 3) % 6];
            out << "2026-10-12 19:00:" << std::setw(2) << std::setfill('0') << (i % 60)
                << " [" << lvl << "] [T" << (i % 8) << "] "
                << "handling request id=" << (f * linesPerFile + i)
                << " took " << (i % 250) << "ms\n";
        }
    }
}

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "logs";

    if (!fs::exists(dir)) {
        std::cout << "generating sample logs in ./" << dir << " ...\n";
        makeLogFiles(dir, 200, 20000);
    }

    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path().string());
        }
    }

    if (files.empty()) {
        std::cout << "no files found in " << dir << "\n";
        return 1;
    }

    std::cout << "scanning " << files.size() << " files\n\n";

    // one at a time
    auto t0 = std::chrono::steady_clock::now();
    Counts single;
    for (const std::string& f : files) {
        single.add(scanFile(f));
    }
    auto t1 = std::chrono::steady_clock::now();
    double singleSec = std::chrono::duration<double>(t1 - t0).count();

    // through the pool
    unsigned cores = std::thread::hardware_concurrency();
    if (cores == 0) {
        cores = 4;
    }

    auto t2 = std::chrono::steady_clock::now();
    Counts pooled;
    {
        Threadpool pool(cores, 64);

        std::vector<std::future<Counts>> results;
        results.reserve(files.size());
        for (const std::string& f : files) {
            results.push_back(pool.executeTask(scanFile, f));
        }

        for (auto& r : results) {
            pooled.add(r.get());
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    double poolSec = std::chrono::duration<double>(t3 - t2).count();

    std::cout << "lines  : " << pooled.lines  << "\n";
    std::cout << "errors : " << pooled.errors << "\n";
    std::cout << "warns  : " << pooled.warns  << "\n\n";

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "one file at a time : " << singleSec << " s\n";
    std::cout << "with " << cores << " threads     : " << poolSec << " s\n";
    std::cout << "speedup            : " << (singleSec / poolSec) << "x\n";

    if (single.lines != pooled.lines || single.errors != pooled.errors) {
        std::cout << "\nMISMATCH between the two runs\n";
        return 1;
    }
    std::cout << "\nboth runs agree on the counts\n";
    return 0;
}
