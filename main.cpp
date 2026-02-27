/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
  main.cpp
  CS 5330 - Project 3: Real-time 2-D Object Recognition

  Main application tying together all pipeline stages:
    Tasks 1-4: threshold → morphological → connected components → features
    Tasks 5-6: training DB + nearest-neighbour classification
    Task 7:    confusion matrix evaluation
    Task 9:    one-shot classification via eigenspace (PCA) and CNN (ResNet18) embeddings

  Key bindings:
    q         quit
    0         toggle continuous classification
    1         classify using hand-crafted features (default)
    2         classify using eigenspace embedding
    3         classify using CNN embedding
    4         train: save current object's hand-crafted features with a label
    5         save preprocessed ROI image for eigenspace training
    6         build eigenspace from saved ROI directory
    7         save CNN embedding for current object (training)
    8         ground-truth mode: classify then prompt for true label (confusion matrix)
    9         print confusion matrix
    s         save screenshot
    +/=       increase threshold bias
    -         decrease threshold bias
    space     pause / resume video
    .  ,      next / previous image (directory mode)
*/
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <chrono>
#include <map>

#include "opencv2/opencv.hpp"
#include "opencv2/dnn.hpp"

#include "vision.h"
#include "thresholding.h"
#include "morphological.h"
#include "segmentation.h"
#include "features.h"
#include "classifier.h"
#include "eigenspace.h"

namespace fs = std::filesystem;

// ─── Forward declarations for embedding functions (utilities.cpp) ──────────
int  getEmbedding(cv::Mat &src, cv::Mat &embedding, cv::dnn::Net &net, int debug);
void prepEmbeddingImage(cv::Mat &frame, cv::Mat &embimage,
                        int cx, int cy, float theta,
                        float minE1, float maxE1, float minE2, float maxE2,
                        int debug);

// ─── Paths ─────────────────────────────────────────────────────────────────
static const std::string DB_PATH       = "object_db.csv";
static const std::string ONNX_PATH     = "resnet18-v2-7.onnx";
static const std::string ROI_DIR       = "roi_training";
static const std::string EIGEN_PREFIX  = "eigen";
static const std::string CNN_DB_PATH   = "cnn_db.csv";

// ─── Configuration ─────────────────────────────────────────────────────────
static int   MIN_REGION_AREA = 800;
static int   MORPH_CLOSE_R   = 4;
static int   MORPH_OPEN_R    = 3;
static int   BLUR_KSIZE      = 5;
static float UNKNOWN_THRESH  = 6.0f;
static int   THRESH_BIAS     = 20;       // manual bias added to auto threshold

// ─── CNN embedding DB ──────────────────────────────────────────────────────
struct CNNEntry {
    std::string label;
    std::vector<float> embedding;
};

static std::vector<CNNEntry> cnnDB;

static void loadCNNDB()
{
    cnnDB.clear();
    std::ifstream fin(CNN_DB_PATH);
    if (!fin.is_open()) return;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string label;
        std::getline(ss, label, ',');
        CNNEntry entry;
        entry.label = label;
        float val;
        char comma;
        while (ss >> val) {
            entry.embedding.push_back(val);
            ss >> comma;
        }
        if (!entry.embedding.empty())
            cnnDB.push_back(entry);
    }
}

static void saveCNNDB()
{
    std::ofstream fout(CNN_DB_PATH);
    fout << std::fixed << std::setprecision(6);
    for (auto &e : cnnDB) {
        fout << e.label;
        for (float v : e.embedding) fout << "," << v;
        fout << "\n";
    }
}

static std::string classifyCNN(const std::vector<float> &emb, double &distance)
{
    distance = 1e30;
    std::string best = "unknown";
    for (auto &e : cnnDB) {
        if (e.embedding.size() != emb.size()) continue;
        double ssd = 0;
        for (size_t i = 0; i < emb.size(); i++) {
            double d = emb[i] - e.embedding[i];
            ssd += d * d;
        }
        if (ssd < distance) {
            distance = ssd;
            best = e.label;
        }
    }
    return best;
}

// ─── Process a single frame through the full pipeline ──────────────────────
struct PipelineResult {
    cv::Mat thresholded;
    cv::Mat cleaned;
    cv::Mat regionMap;
    cv::Mat regionColour;
    std::vector<RegionStats>    filteredStats;
    std::vector<RegionFeatures> regionFeatures;
};

PipelineResult processFrame(const cv::Mat &frame)
{
    PipelineResult res;

    // 1. Pre-process: light blur
    cv::Mat blurred;
    cv::GaussianBlur(frame, blurred, cv::Size(BLUR_KSIZE, BLUR_KSIZE), 0);

    // 2. Threshold (from scratch)
    cv::Mat score = computeObjectScore(blurred);
    int thresh    = isodataThreshold(score) + THRESH_BIAS;
    res.thresholded = applyThreshold(score, thresh);

    // 3. Morphological cleanup (from scratch)
    res.cleaned = morphologicalCleanup(res.thresholded, MORPH_CLOSE_R, MORPH_OPEN_R);

    // 4. Connected components (from scratch)
    std::vector<RegionStats> allStats;
    cv::Mat rawLabels = connectedComponents(res.cleaned, allStats);

    // 5. Filter regions
    res.regionMap = filterRegions(rawLabels, allStats, res.filteredStats,
                                  MIN_REGION_AREA, true);
    int numRegions = static_cast<int>(res.filteredStats.size());
    res.regionColour = colourRegionMap(res.regionMap, numRegions);

    // 6. Compute features for each valid region (from scratch)
    for (auto &rs : res.filteredStats) {
        RegionFeatures feat = computeRegionFeatures(res.regionMap, rs.id);
        res.regionFeatures.push_back(feat);
    }

    return res;
}

// ─── Print feature vector ──────────────────────────────────────────────────
void printFeatures(const RegionFeatures &f, const std::string &label = "")
{
    std::cout << (label.empty() ? "Region" : label) << " features: "
              << "filled=" << std::fixed << std::setprecision(3) << f.percentFilled
              << "  AR=" << f.aspectRatio;
    for (int i = 0; i < 7; i++)
        std::cout << "  lH" << i << "=" << std::setprecision(2) << f.logHu[i];
    std::cout << std::endl;
}

// ─── Prepare embedding image for a region ──────────────────────────────────
cv::Mat prepROI(cv::Mat &frame, const RegionFeatures &feat)
{
    cv::Mat embImage;
    prepEmbeddingImage(
        frame, embImage,
        (int)feat.centroidX, (int)feat.centroidY,
        (float)feat.theta,
        (float)feat.minE1, (float)feat.maxE1,
        (float)feat.minE2, (float)feat.maxE2,
        0);
    return embImage;
}

// ─── Flush OpenCV key buffer after terminal input ──────────────────────────
// On macOS, keystrokes can leak to both the terminal and OpenCV window.
// This drains all buffered keys over a 500ms window to ensure none are left.
void flushKeys()
{
    auto start = std::chrono::steady_clock::now();
    while (true) {
        cv::waitKey(1);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 500) break;
    }
}

// ─── Usage ─────────────────────────────────────────────────────────────────
void printUsage(const char *prog)
{
    std::cout
      << "\n====================================================\n"
      << "  CS 5330 Project 3: Real-time 2-D Object Recognition\n"
      << "====================================================\n"
      << "Usage:\n"
      << "  " << prog << "                     webcam mode\n"
      << "  " << prog << " -i <image>          single image\n"
      << "  " << prog << " -d <directory>      directory of images\n"
      << "  " << prog << " -v <video>          video file\n"
      << "\nKey bindings:\n"
      << "  q       quit\n"
      << "  0       toggle classification ON/OFF\n"
      << "  1/2/3   classify with: features / eigenspace / CNN\n"
      << "  4       train hand-crafted features\n"
      << "  5       save ROI for eigenspace\n"
      << "  6       build eigenspace from saved ROIs\n"
      << "  7       save CNN embedding\n"
      << "  8       ground-truth evaluation (confusion matrix)\n"
      << "  9       print confusion matrix\n"
      << "  s       save screenshot\n"
      << "  +/=     increase threshold bias\n"
      << "  -       decrease threshold bias\n"
      << "  space   pause/resume\n"
      << "  . ,     next/prev image (directory mode)\n\n";
}

// ─── Main ──────────────────────────────────────────────────────────────────
int main(int argc, char *argv[])
{
    // ── Parse arguments ──
    enum Mode { WEBCAM, IMAGE, DIRECTORY, VIDEO };
    Mode mode = WEBCAM;
    std::string inputPath;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "-i" && i + 1 < argc) { mode = IMAGE;     inputPath = argv[++i]; }
        else if (arg == "-d" && i + 1 < argc) { mode = DIRECTORY; inputPath = argv[++i]; }
        else if (arg == "-v" && i + 1 < argc) { mode = VIDEO;     inputPath = argv[++i]; }
        else if (arg == "-h") { printUsage(argv[0]); return 0; }
    }

    // ── Load hand-crafted feature classifier ──
    ObjectClassifier classifier;
    int dbCount = classifier.loadDB(DB_PATH);
    std::cout << "Hand-crafted DB: " << dbCount << " entries\n";

    // ── Try to load ONNX model ──
    cv::dnn::Net dnnNet;
    bool hasDNN = false;
    if (fs::exists(ONNX_PATH)) {
        try {
            dnnNet = cv::dnn::readNetFromONNX(ONNX_PATH);
            hasDNN = true;
            std::cout << "DNN model loaded: " << ONNX_PATH << "\n";
        } catch (const cv::Exception &ex) {
            std::cerr << "ONNX load failed: " << ex.what() << "\n";
        }
    } else {
        std::cout << "ONNX not found (" << ONNX_PATH << "). CNN disabled.\n";
    }

    // ── Load CNN embedding DB ──
    loadCNNDB();
    std::cout << "CNN DB: " << cnnDB.size() << " entries\n";

    // ── Load eigenspace ──
    EigenspaceClassifier eigenClassifier;
    eigenClassifier.load(EIGEN_PREFIX);

    // ── Set up input source ──
    cv::VideoCapture cap;
    std::vector<std::string> imageFiles;
    int imageIdx = 0;

    if (mode == WEBCAM) {
        cap.open(0);
        if (!cap.isOpened()) { std::cerr << "Cannot open webcam.\n"; return 1; }
    } else if (mode == VIDEO) {
        cap.open(inputPath);
        if (!cap.isOpened()) { std::cerr << "Cannot open video.\n"; return 1; }
    } else if (mode == DIRECTORY) {
        for (auto &entry : fs::directory_iterator(inputPath)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
                imageFiles.push_back(entry.path().string());
        }
        std::sort(imageFiles.begin(), imageFiles.end());
        if (imageFiles.empty()) { std::cerr << "No images.\n"; return 1; }
        std::cout << imageFiles.size() << " images found.\n";
    }

    int  classMethod  = 1;      // 1=features, 2=eigen, 3=CNN
    bool classifyMode = true;
    bool paused       = false;
    int  saveCounter  = 0;
    int  roiCounter   = 0;
    cv::Mat lastFrame;

    fs::create_directories(ROI_DIR);
    printUsage(argv[0]);

    // ─── Main loop ─────────────────────────────────────────────────────────
    while (true) {
        cv::Mat frame;

        if (mode == WEBCAM || mode == VIDEO) {
            if (!paused) {
                cap >> frame;
                if (frame.empty()) break;
                lastFrame = frame.clone();
            } else {
                if (lastFrame.empty()) { cv::waitKey(30); continue; }
                frame = lastFrame.clone();
            }
        } else if (mode == IMAGE) {
            if (lastFrame.empty()) {
                lastFrame = cv::imread(inputPath);
                if (lastFrame.empty()) { std::cerr << "Cannot read image.\n"; return 1; }
            }
            frame = lastFrame.clone();
        } else {
            if (imageIdx < 0) imageIdx = 0;
            if (imageIdx >= (int)imageFiles.size()) imageIdx = 0;
            frame = cv::imread(imageFiles[imageIdx]);
            if (frame.empty()) { imageIdx++; continue; }
            lastFrame = frame.clone();
        }

        // ── Run pipeline ──
        PipelineResult res = processFrame(frame);
        cv::Mat display = frame.clone();

        std::string methodName = (classMethod == 1) ? "Features" :
                                 (classMethod == 2) ? "Eigenspace" : "CNN";

        // ── Draw overlays for each detected region ──
        for (auto &feat : res.regionFeatures) {
            drawOrientedBB(display, feat);

            if (classifyMode) {
                std::string label;
                double dist = 1e30;

                if (classMethod == 1 && classifier.size() > 0) {
                    auto fv = feat.getFeatureVector();
                    label = classifier.classify(fv, dist);
                    if (dist > UNKNOWN_THRESH) label = "unknown";
                }
                else if (classMethod == 2 && eigenClassifier.isBuilt()) {
                    cv::Mat roi = prepROI(frame, feat);
                    if (!roi.empty()) label = eigenClassifier.classify(roi, dist);
                }
                else if (classMethod == 3 && hasDNN && !cnnDB.empty()) {
                    cv::Mat roi = prepROI(frame, feat);
                    if (!roi.empty()) {
                        cv::Mat emb;
                        getEmbedding(roi, emb, dnnNet, 0);
                        std::vector<float> ev(emb.ptr<float>(0),
                                              emb.ptr<float>(0) + emb.cols);
                        label = classifyCNN(ev, dist);
                    }
                }

                if (!label.empty()) {
                    std::string text = label + " [" + methodName + "]";
                    drawLabel(display, feat, text);
                }
            }
        }

        // HUD
        cv::putText(display, "Mode: " + methodName + (classifyMode ? " ON" : " OFF"),
                    cv::Point(10, 25), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(0, 255, 255), 2);
        if (mode == DIRECTORY && !imageFiles.empty()) {
            std::string fn = fs::path(imageFiles[imageIdx]).filename().string();
            cv::putText(display, fn, cv::Point(10, 50),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);
        }

        // Show pipeline stages
        cv::imshow("Original + Overlays", display);
        cv::Mat threshDisp, cleanDisp;
        cv::cvtColor(res.thresholded, threshDisp, cv::COLOR_GRAY2BGR);
        cv::cvtColor(res.cleaned, cleanDisp, cv::COLOR_GRAY2BGR);
        cv::imshow("Thresholded", threshDisp);
        cv::imshow("Cleaned (Morphological)", cleanDisp);
        cv::imshow("Regions", res.regionColour);

        // ── Key handling ──
        // All commands that require text input use NUMBER keys
        // so they never conflict with typing labels in the terminal.
        int waitTime = (mode == WEBCAM || mode == VIDEO) ? 30 : 0;
        int key = cv::waitKey(waitTime) & 0xFF;

        if (key == 'q' || key == 27) break;

        else if (key == '4') {
            // ── Train hand-crafted features ──
            if (res.regionFeatures.empty()) { std::cout << "No region.\n"; continue; }
            std::cout << "Label: "; std::string label; std::getline(std::cin, label);
            flushKeys();
            if (!label.empty()) {
                auto fv = res.regionFeatures[0].getFeatureVector();
                classifier.addEntry(label, fv);
                classifier.saveDB(DB_PATH);
                printFeatures(res.regionFeatures[0], label);
                std::cout << "DB: " << classifier.size() << " entries.\n";
            }
        }
        else if (key == '0') {
            classifyMode = !classifyMode;
            std::cout << "Classification " << (classifyMode ? "ON" : "OFF") << "\n";
        }
        else if (key == '1') { classMethod = 1; std::cout << "→ Hand-crafted features\n"; }
        else if (key == '2') { classMethod = 2; std::cout << "→ Eigenspace\n"; }
        else if (key == '3') { classMethod = 3; std::cout << "→ CNN embedding\n"; }

        else if (key == '8') {
            // ── Ground truth evaluation ──
            if (res.regionFeatures.empty()) { std::cout << "No region.\n"; continue; }
            double dist; std::string predicted;
            if (classMethod == 1) {
                auto fv = res.regionFeatures[0].getFeatureVector();
                predicted = classifier.classify(fv, dist);
                if (dist > UNKNOWN_THRESH) predicted = "unknown";
            } else if (classMethod == 2 && eigenClassifier.isBuilt()) {
                cv::Mat roi = prepROI(frame, res.regionFeatures[0]);
                predicted = eigenClassifier.classify(roi, dist);
            } else if (classMethod == 3 && hasDNN && !cnnDB.empty()) {
                cv::Mat roi = prepROI(frame, res.regionFeatures[0]);
                cv::Mat emb; getEmbedding(roi, emb, dnnNet, 0);
                std::vector<float> ev(emb.ptr<float>(0), emb.ptr<float>(0) + emb.cols);
                predicted = classifyCNN(ev, dist);
            } else { std::cout << "Classifier not ready.\n"; continue; }

            std::cout << "Predicted: " << predicted << " (d=" << dist << ")\n";
            std::cout << "True label (enter=accept): ";
            std::string tl; std::getline(std::cin, tl);
            flushKeys();
            if (tl.empty()) tl = predicted;
            classifier.recordResult(tl, predicted);
        }
        else if (key == '9') { classifier.printConfusionMatrix(); }
        // Note: to reset confusion matrix, restart the program

        else if (key == '5') {
            // ── Save ROI for eigenspace ──
            if (res.regionFeatures.empty()) { std::cout << "No region.\n"; continue; }
            std::cout << "ROI label: "; std::string label; std::getline(std::cin, label);
            flushKeys();
            if (label.empty()) continue;
            cv::Mat roi = prepROI(frame, res.regionFeatures[0]);
            if (!roi.empty()) {
                std::string fname = ROI_DIR + "/" + label + "_"
                                  + std::to_string(roiCounter++) + ".png";
                cv::imwrite(fname, roi);
                std::cout << "Saved: " << fname << "\n";
                cv::imshow("Saved ROI", roi);
            }
        }
        else if (key == '6') {
            // ── Build eigenspace ──
            int n = eigenClassifier.buildFromDirectory(ROI_DIR);
            if (n > 0) eigenClassifier.save(EIGEN_PREFIX);
            else std::cout << "No images in " << ROI_DIR << "/\n";
        }
        else if (key == '7') {
            // ── Save CNN embedding ──
            if (!hasDNN) { std::cout << "DNN not loaded.\n"; continue; }
            if (res.regionFeatures.empty()) { std::cout << "No region.\n"; continue; }
            std::cout << "CNN label: "; std::string label; std::getline(std::cin, label);
            flushKeys();
            if (label.empty()) continue;
            cv::Mat roi = prepROI(frame, res.regionFeatures[0]);
            if (!roi.empty()) {
                cv::Mat emb; getEmbedding(roi, emb, dnnNet, 0);
                CNNEntry entry;
                entry.label = label;
                entry.embedding.assign(emb.ptr<float>(0), emb.ptr<float>(0) + emb.cols);
                cnnDB.push_back(entry);
                saveCNNDB();
                std::cout << "CNN DB: " << cnnDB.size() << " entries.\n";
            }
        }

        else if (key == 's') {
            std::string f = "screenshot_" + std::to_string(saveCounter++) + ".png";
            cv::imwrite(f, display);
            std::cout << "Saved " << f << "\n";
        }
        else if (key == '+' || key == '=') { THRESH_BIAS += 5; std::cout << "Bias: " << THRESH_BIAS << "\n"; }
        else if (key == '-')               { THRESH_BIAS -= 5; std::cout << "Bias: " << THRESH_BIAS << "\n"; }
        else if (key == ' ') { paused = !paused; }
        else if (key == '.' || key == 83) { if (mode == DIRECTORY) imageIdx++; }
        else if (key == ',' || key == 81) { if (mode == DIRECTORY) imageIdx--; }
    }

    cv::destroyAllWindows();
    return 0;
}