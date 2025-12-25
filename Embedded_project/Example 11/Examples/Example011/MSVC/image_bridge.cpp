// image_bridge.cpp

#include "image_bridge.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <cstdio>
#include <mutex>


static bool initCamera(cv::VideoCapture& cap) {
    if (cap.isOpened())
        return true;

    int camIndexCandidates[] = { 0, 1 };
    bool opened = false;

#ifdef _WIN32
    // First try DirectShow
    for (int idx : camIndexCandidates) {
        std::cout << "[cam] Trying index " << idx << " with CAP_DSHOW\n";
        if (cap.open(idx, cv::CAP_DSHOW)) {
            opened = true;
            break;
        }
    }
    // Fallback to Media Foundation if DirectShow failed
    if (!opened) {
        for (int idx : camIndexCandidates) {
            std::cout << "[cam] Trying index " << idx << " with CAP_MSMF\n";
            if (cap.open(idx, cv::CAP_MSMF)) {
                opened = true;
                break;
            }
        }
    }
#else
    // Non-Windows: just try default backend
    for (int idx : camIndexCandidates) {
        std::cout << "[cam] Trying index " << idx << " with default backend\n";
        if (cap.open(idx)) {
            opened = true;
            break;
        }
    }
#endif

    if (!opened) {
        std::cerr << "[cam] FAILED to open any camera\n";
        return false;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    return true;
}

// -----------------------------------------------------------------------------
// Public C API implementations
// -----------------------------------------------------------------------------

int cam_capture_and_save(const char* outPath)
{
    // Protect the static VideoCapture and initialization with a mutex to avoid
    // concurrent opens/reads which can cause undefined behavior in OpenCV
    static cv::VideoCapture cap;
    static bool initialized = false;
    static std::mutex cap_mutex;

    std::lock_guard<std::mutex> lock(cap_mutex);

    if (!initialized) {
        if (!initCamera(cap)) {
            return -1;  // failed to open any camera
        }
        initialized = true;
    }

    cv::Mat frame;
    if (!cap.read(frame) || frame.empty()) {
        std::cerr << "[cam] failed to capture frame\n";
        return -2;
    }

    if (!cv::imwrite(outPath, frame)) {
        std::cerr << "[cam] failed to write " << outPath << "\n";
        return -3;
    }

    std::cout << "[cam] saved " << outPath << "\n";
    return 0;
}

int img_invert_process(const char* inPath, const char* outPath)
{
    cv::Mat src = cv::imread(inPath, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cerr << "img_invert_process: failed to read " << inPath << "\n";
        return -1;
    }

    cv::Mat dst;
    cv::bitwise_not(src, dst);

    if (!cv::imwrite(outPath, dst)) {
        std::cerr << "img_invert_process: failed to write " << outPath << "\n";
        return -2;
    }

    std::cout << "img_invert_process: " << inPath << " -> " << outPath << "\n";
    return 0;
}

int img_blur_process(const char* inPath, const char* outPath)
{
    cv::Mat src = cv::imread(inPath, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cerr << "img_blur_process: failed to read " << inPath << "\n";
        return -1;
    }

    cv::Mat dst;
    // Simple Gaussian blur; students can play with kernel size
    cv::GaussianBlur(src, dst, cv::Size(5, 5), 0.0);

    if (!cv::imwrite(outPath, dst)) {
        std::cerr << "img_blur_process: failed to write " << outPath << "\n";
        return -2;
    }

    std::cout << "img_blur_process: " << inPath << " -> " << outPath << "\n";
    return 0;
}

int img_grayscale_process(const char* inPath, const char* outPath)
{
    cv::Mat src = cv::imread(inPath, cv::IMREAD_COLOR);
    if (src.empty()) {
        std::cerr << "img_grayscale_process: failed to read " << inPath << "\n";
        return -1;
    }

    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

    if (!cv::imwrite(outPath, gray)) {
        std::cerr << "img_grayscale_process: failed to write " << outPath << "\n";
        return -2;
    }

    std::cout << "img_grayscale_process: " << inPath << " -> " << outPath << "\n";
    return 0;
}

int img_edge_process(const char* inPath, const char* outPath)
{
    cv::Mat src = cv::imread(inPath, cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::cerr << "img_edge_process: failed to read " << inPath << " (grayscale)\n";
        return -1;
    }

    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(3, 3), 0.0);

    cv::Mat edges;
    cv::Canny(blurred, edges, 100.0, 200.0);

    if (!cv::imwrite(outPath, edges)) {
        std::cerr << "img_edge_process: failed to write " << outPath << "\n";
        return -2;
    }

    std::cout << "img_edge_process: " << inPath << " -> " << outPath << "\n";
    return 0;
}

int img_show_pipeline(int idx)
{
    const char* baseDir = "Images/";  // must exist next to the executable

    char path_original[260];
    char path_invert[260];
    char path_blur[260];
    char path_edge[260];
    char path_gray[260];

    std::snprintf(path_original, sizeof(path_original), "%scapture_%03d.png", baseDir, idx);
    std::snprintf(path_invert, sizeof(path_invert), "%sinvert_%03d.png", baseDir, idx);
    std::snprintf(path_blur, sizeof(path_blur), "%sblur_%03d.png", baseDir, idx);
    std::snprintf(path_edge, sizeof(path_edge), "%sedge_%03d.png", baseDir, idx);
    std::snprintf(path_gray, sizeof(path_gray), "%sgray_%03d.png", baseDir, idx);

    cv::Mat img_original = cv::imread(path_original, cv::IMREAD_COLOR);
    cv::Mat img_invert = cv::imread(path_invert, cv::IMREAD_COLOR);
    cv::Mat img_blur = cv::imread(path_blur, cv::IMREAD_COLOR);
    cv::Mat img_edge = cv::imread(path_edge, cv::IMREAD_GRAYSCALE);
    cv::Mat img_gray = cv::imread(path_gray, cv::IMREAD_GRAYSCALE);

    if (img_original.empty()) {
        std::cerr << "img_show_pipeline: original not found: " << path_original << "\n";
        return -2;
    }

    // Convert grayscale images to BGR so we can concatenate them with color images
    if (!img_edge.empty())
        cv::cvtColor(img_edge, img_edge, cv::COLOR_GRAY2BGR);
    if (!img_gray.empty())
        cv::cvtColor(img_gray, img_gray, cv::COLOR_GRAY2BGR);

    std::vector<cv::Mat> imgs;
    imgs.push_back(img_original);

    if (!img_invert.empty()) imgs.push_back(img_invert);
    if (!img_blur.empty())   imgs.push_back(img_blur);
    if (!img_edge.empty())   imgs.push_back(img_edge);
    if (!img_gray.empty())   imgs.push_back(img_gray);

    if (imgs.size() == 1) {
        cv::imshow("Pipeline", img_original);
        cv::waitKey(1000);  // show for 1 second
        return 0;
    }

    // Resize all to same height for horizontal concatenation
    int target_h = imgs[0].rows;
    for (auto& im : imgs) {
        if (im.rows != target_h) {
            double scale = static_cast<double>(target_h) / im.rows;
            cv::resize(im, im, cv::Size(), scale, scale);
        }
    }

    cv::Mat combined;
    cv::hconcat(imgs, combined);

    cv::imshow("Pipeline", combined);
    cv::waitKey(1000);

    std::cout << "img_show_pipeline: displayed pipeline for index " << idx << "\n";
    return 0;
}
