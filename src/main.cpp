#include "Image.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

    enum class Operation { Grayscale, Invert, GrayscaleAndInvert };

    struct ProcessingState {
        std::queue<fs::path> tasks;
        std::mutex queueMutex;
        std::mutex statisticsMutex;
        std::mutex outputMutex;
        std::size_t success = 0;
        std::size_t failed = 0;
    };

    std::string trim(const std::string& value) {
        const auto first = value.find_first_not_of(" \t\r\n");

        if (first == std::string::npos) {
            return {};
        }

        const auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool readDirectory(const std::string& prompt, fs::path& directory) {
        std::cout << prompt;

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cerr << "Error: failed to read directory path.\n";
            return false;
        }

        input = trim(input);
        if (input.empty()) {
            std::cerr << "Error: directory path cannot be empty.\n";
            return false;
        }

        directory = fs::path(input);
        return true;
    }

    bool isSupportedImage(const fs::path& path) {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp"
                || extension == ".tga";
    }

    bool validateInputDirectory(const fs::path& directory) {
        std::error_code error;
        const bool exists = fs::exists(directory, error);

        if (error || !exists || !fs::is_directory(directory, error) || error) {
            std::cerr << "Error: input directory does not exist or is not a directory: " << directory.string() << '\n';
            return false;
        }

        return true;
    }

    bool prepareOutputDirectory(const fs::path& directory) {
        std::error_code error;
        fs::create_directories(directory, error);

        if (error || !fs::is_directory(directory, error) || error) {
            std::cerr << "Error: cannot create output directory: " << directory.string() << '\n';
            return false;
        }

        return true;
    }

    bool findImages(const fs::path& directory, std::vector<fs::path>& files) {
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                std::error_code error;
                if (entry.is_regular_file(error) && !error && isSupportedImage(entry.path())) {
                    files.push_back(entry.path());
                }
            }
        } catch (const fs::filesystem_error& error) {
            std::cerr << "Error: failed to scan input directory: " << error.what() << '\n';
            return false;
        }

        std::sort(files.begin(), files.end());
        return true;
    }

    bool readChoice(const std::string& prompt, int minimum, int maximum, int& value) {
        std::cout << prompt;

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cerr << "Error: failed to read selection.\n";
            return false;
        }

        std::istringstream stream(input);
        char extra = '\0';

        if (!(stream >> value) || (stream >> extra) || value < minimum || value > maximum) {
            std::cerr << "Error: invalid selection.\n";
            return false;
        }

        return true;
    }

    bool readPositiveInteger(const std::string& prompt, int& value) {
        std::cout << prompt;

        std::string input;
        if (!std::getline(std::cin, input)) {
            std::cerr << "Error: failed to read number of worker threads.\n";
            return false;
        }

        std::istringstream stream(input);
        char extra = '\0';

        if (!(stream >> value) || (stream >> extra) || value <= 0) {
            std::cerr << "Error: number of worker threads must be greater than zero.\n";
            return false;
        }

        return true;
    }

    Operation selectOperation(int selection) {
        switch (selection) {
        case 1:
            return Operation::Grayscale;
        case 2:
            return Operation::Invert;
        default:
            return Operation::GrayscaleAndInvert;
        }
    }

    std::string operationName(Operation operation) {
        switch (operation) {
        case Operation::Grayscale:
            return "Grayscale";
        case Operation::Invert:
            return "Invert";
        case Operation::GrayscaleAndInvert:
            return "Grayscale + Invert";
        }

        return {};
    }

    bool processImage(const fs::path& inputPath, const fs::path& outputDirectory, Operation operation) {
        try {
            Image image;
            if (!image.load(inputPath.string())) {
                return false;
            }

            switch (operation) {
            case Operation::Grayscale:
                image.grayscale();
                break;
            case Operation::Invert:
                image.invert();
                break;
            case Operation::GrayscaleAndInvert:
                image.grayscale();
                image.invert();
                break;
            }

            const fs::path outputPath = outputDirectory / (inputPath.filename().string() + ".processed.png");
            return image.save(outputPath.string());
        } catch (...) {
            return false;
        }
    }

    void printFileResult(std::mutex& outputMutex, std::size_t workerNumber, const fs::path& path, bool successful) {
        std::lock_guard<std::mutex> lock(outputMutex);

        std::cout << "[Thread " << workerNumber << "] " << (successful ? "Completed: " : "Failed: ")
                  << path.filename().string() << '\n';
    }

    void processWorker(
            std::size_t workerNumber, ProcessingState& state, const fs::path& outputDirectory, Operation operation) {
        while (true) {
            fs::path inputPath;

            {
                std::lock_guard<std::mutex> lock(state.queueMutex);

                if (state.tasks.empty()) {
                    return;
                }

                inputPath = state.tasks.front();
                state.tasks.pop();
            }

            const bool successful = processImage(inputPath, outputDirectory, operation);
            printFileResult(state.outputMutex, workerNumber, inputPath, successful);

            {
                std::lock_guard<std::mutex> lock(state.statisticsMutex);

                if (successful) {
                    ++state.success;
                } else {
                    ++state.failed;
                }
            }
        }
    }

    void printSummary(
            bool parallel,
            std::size_t workerCount,
            Operation operation,
            std::size_t total,
            std::size_t success,
            std::size_t failed,
            double processingTime,
            const fs::path& outputDirectory) {
        std::cout << "\n========================================\n"
                  << "         PROCESSING SUMMARY\n"
                  << "========================================\n"
                  << "Mode:                    " << (parallel ? "Parallel" : "Sequential") << '\n'
                  << "Operation:               " << operationName(operation) << '\n'
                  << "Worker threads:          " << workerCount << '\n'
                  << "Total images:            " << total << '\n'
                  << "Successfully processed:  " << success << '\n'
                  << "Failed:                  " << failed << '\n'
                  << "Processing time:         " << std::fixed << std::setprecision(3) << processingTime << " seconds\n"
                  << "Results directory:       " << outputDirectory.string() << '\n'
                  << "========================================\n";
    }

}

int main() {
    std::cout << "========================================\n"
              << "       IMAGE PROCESSING UTILITY\n"
              << "========================================\n";

    fs::path inputDirectory;
    fs::path outputDirectory;

    if (!readDirectory("Enter input directory: ", inputDirectory) || !validateInputDirectory(inputDirectory)) {
        return 1;
    }

    if (!readDirectory("Enter output directory: ", outputDirectory) || !prepareOutputDirectory(outputDirectory)) {
        return 1;
    }

    std::vector<fs::path> files;

    std::cout << "\nScanning input directory...\n";
    if (!findImages(inputDirectory, files)) {
        return 1;
    }

    std::cout << "Found " << files.size() << " supported image files.\n";

    if (files.empty()) {
        std::cout << "No supported images found. Nothing to process.\n";
        return 0;
    }

    std::cout << "\nSelect processing operation:\n"
              << "  1. Grayscale\n"
              << "  2. Invert\n"
              << "  3. Grayscale + Invert\n";

    int operationSelection = 0;
    if (!readChoice("Enter operation number: ", 1, 3, operationSelection)) {
        return 1;
    }

    const Operation operation = selectOperation(operationSelection);

    std::cout << "\nSelect processing mode:\n"
              << "  1. Sequential processing\n"
              << "  2. Parallel processing\n";

    int modeSelection = 0;
    if (!readChoice("Enter mode: ", 1, 2, modeSelection)) {
        return 1;
    }

    const bool parallel = modeSelection == 2;
    int workerCount = 1;

    if (parallel && !readPositiveInteger("Enter number of worker threads (> 0): ", workerCount)) {
        return 1;
    }

    std::size_t success = 0;
    std::size_t failed = 0;
    const auto startTime = std::chrono::steady_clock::now();

    if (parallel) {
        ProcessingState state;

        for (const auto& file : files) {
            state.tasks.push(file);
        }

        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(workerCount));

        try {
            for (int i = 0; i < workerCount; ++i) {
                workers.emplace_back(
                        processWorker,
                        static_cast<std::size_t>(i + 1),
                        std::ref(state),
                        std::cref(outputDirectory),
                        operation);
            }
        } catch (...) {
            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }

        for (auto& worker : workers) {
            worker.join();
        }

        success = state.success;
        failed = state.failed;
    } else {
        std::mutex outputMutex;

        for (const auto& file : files) {
            const bool successful = processImage(file, outputDirectory, operation);
            printFileResult(outputMutex, 1, file, successful);

            if (successful) {
                ++success;
            } else {
                ++failed;
            }
        }
    }

    const auto endTime = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = endTime - startTime;

    printSummary(
            parallel,
            static_cast<std::size_t>(workerCount),
            operation,
            files.size(),
            success,
            failed,
            elapsed.count(),
            outputDirectory);

    return success + failed == files.size() ? 0 : 1;
}
