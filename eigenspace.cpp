/*
  eigenspace.cpp
  CS 5330 - Project 3
  Eigenspace (PCA) one-shot classification.

  Build:  collect greyscale ROI images → flatten → form data matrix →
          compute mean → subtract → SVD → keep top-N eigenvectors.
  Classify:  subtract mean, project onto eigenvectors, compare SSD
             against training embeddings, return nearest-neighbour label.
*/
#include "eigenspace.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace fs = std::filesystem;

// ──────────────────────────────────────────────────────────────────────────
static std::string labelFromFilename(const std::string &fname)
{
    // Expect format:  label_NNN.ext  →  extract "label"
    // Find last '/' or '\' to get basename
    size_t slash = fname.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? fname : fname.substr(slash + 1);
    // Remove extension
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    // Remove trailing _NNN
    size_t us = base.find_last_of('_');
    if (us != std::string::npos) {
        // check if everything after '_' is digits
        std::string suffix = base.substr(us + 1);
        bool allDigit = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
        if (allDigit) base = base.substr(0, us);
    }
    return base;
}

// ──────────────────────────────────────────────────────────────────────────
int EigenspaceClassifier::buildFromDirectory(const std::string &dir)
{
    std::vector<std::string> files;
    std::vector<std::string> labels;

    for (auto &entry : fs::directory_iterator(dir)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
            files.push_back(entry.path().string());
            labels.push_back(labelFromFilename(entry.path().string()));
        }
    }
    std::sort(files.begin(), files.end());

    if (files.empty()) return 0;

    int dim = IMG_SIZE * IMG_SIZE;
    cv::Mat dataMtx; // N × dim

    for (size_t i = 0; i < files.size(); i++) {
        cv::Mat img = cv::imread(files[i], cv::IMREAD_GRAYSCALE);
        if (img.empty()) continue;
        cv::resize(img, img, cv::Size(IMG_SIZE, IMG_SIZE), 0, 0, cv::INTER_AREA);
        cv::Mat row;
        img.reshape(1, 1).convertTo(row, CV_32F); // 1 × dim
        dataMtx.push_back(row);
    }

    int N = dataMtx.rows;
    if (N < 2) return 0;

    // Compute mean
    cv::reduce(dataMtx, meanVec, 0, cv::REDUCE_AVG, CV_32F); // 1 × dim

    // Difference matrix (subtract mean from each row)
    cv::Mat diffMtx = dataMtx.clone();
    for (int i = 0; i < N; i++)
        diffMtx.row(i) -= meanVec;

    // Transpose: columns are images → dim × N
    cv::Mat D = diffMtx.t();

    // SVD (economy)
    cv::Mat U, S, Vt;
    cv::SVD::compute(D, S, U, Vt, cv::SVD::MODIFY_A);
    // U is dim × N, columns are eigenvectors

    // Keep top N_EVEC eigenvectors (as rows for dot-product convenience)
    int keep = std::min(N_EVEC, std::min(U.cols, N));
    eigenVectors = cv::Mat(keep, dim, CV_32F);
    for (int i = 0; i < keep; i++) {
        cv::Mat evRow = U.col(i).t();
        evRow.copyTo(eigenVectors.row(i));
    }

    // Compute eigenvalues for reference
    std::cout << "Eigenspace built with " << N << " images, retaining "
              << keep << " eigenvectors.\n";
    std::cout << "Top singular values: ";
    for (int i = 0; i < std::min(keep, 10); i++)
        std::cout << S.at<float>(i) << " ";
    std::cout << "\n";

    // Project all training images to build training embeddings
    trainEntries.clear();
    for (int i = 0; i < N; i++) {
        cv::Mat diff = dataMtx.row(i) - meanVec; // 1 × dim
        EigenEntry ee;
        ee.label = labels[i];
        ee.embedding.resize(keep);
        for (int j = 0; j < keep; j++)
            ee.embedding[j] = diff.dot(eigenVectors.row(j));
        trainEntries.push_back(ee);
    }

    built = true;
    return N;
}

// ──────────────────────────────────────────────────────────────────────────
std::vector<double> EigenspaceClassifier::project(const cv::Mat &img) const
{
    std::vector<double> emb;
    if (!built) return emb;

    cv::Mat grey;
    if (img.channels() > 1)
        cv::cvtColor(img, grey, cv::COLOR_BGR2GRAY);
    else
        grey = img;

    cv::Mat resized;
    cv::resize(grey, resized, cv::Size(IMG_SIZE, IMG_SIZE), 0, 0, cv::INTER_AREA);

    cv::Mat row;
    resized.reshape(1, 1).convertTo(row, CV_32F);
    cv::Mat diff = row - meanVec;

    int keep = eigenVectors.rows;
    emb.resize(keep);
    for (int j = 0; j < keep; j++)
        emb[j] = diff.dot(eigenVectors.row(j));

    return emb;
}

// ──────────────────────────────────────────────────────────────────────────
std::string EigenspaceClassifier::classify(const cv::Mat &img, double &distance) const
{
    std::vector<double> emb = project(img);
    if (emb.empty() || trainEntries.empty()) {
        distance = 1e30;
        return "unknown";
    }

    double bestDist = 1e30;
    std::string bestLabel = "unknown";

    for (auto &te : trainEntries) {
        double ssd = 0;
        for (size_t i = 0; i < emb.size(); i++) {
            double d = emb[i] - te.embedding[i];
            ssd += d * d;
        }
        if (ssd < bestDist) {
            bestDist = ssd;
            bestLabel = te.label;
        }
    }

    distance = bestDist;
    return bestLabel;
}

// ──────────────────────────────────────────────────────────────────────────
void EigenspaceClassifier::save(const std::string &prefix) const
{
    cv::FileStorage fs(prefix + "_eigenspace.yml", cv::FileStorage::WRITE);
    fs << "meanVec" << meanVec;
    fs << "eigenVectors" << eigenVectors;
    fs << "numEntries" << (int)trainEntries.size();
    for (int i = 0; i < (int)trainEntries.size(); i++) {
        fs << ("label_" + std::to_string(i)) << trainEntries[i].label;
        fs << ("emb_" + std::to_string(i)) << std::vector<double>(trainEntries[i].embedding);
    }
    fs.release();
    std::cout << "Eigenspace saved to " << prefix << "_eigenspace.yml\n";
}

// ──────────────────────────────────────────────────────────────────────────
bool EigenspaceClassifier::load(const std::string &prefix)
{
    cv::FileStorage fs(prefix + "_eigenspace.yml", cv::FileStorage::READ);
    if (!fs.isOpened()) return false;

    fs["meanVec"] >> meanVec;
    fs["eigenVectors"] >> eigenVectors;

    int n;
    fs["numEntries"] >> n;
    trainEntries.clear();
    for (int i = 0; i < n; i++) {
        EigenEntry ee;
        fs["label_" + std::to_string(i)] >> ee.label;
        std::vector<double> emb;
        fs["emb_" + std::to_string(i)] >> emb;
        ee.embedding = emb;
        trainEntries.push_back(ee);
    }
    fs.release();
    built = !meanVec.empty() && !eigenVectors.empty();
    std::cout << "Eigenspace loaded: " << trainEntries.size() << " entries, "
              << eigenVectors.rows << " eigenvectors.\n";
    return built;
}