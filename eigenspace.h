/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
eigenspace.h
  CS 5330 - Project 3
  Eigenspace (PCA) embedding for one-shot classification.
  Can build an eigenspace from a set of training images, project new
  images into the space, and classify via nearest-neighbour SSD.
*/
#pragma once

#include "opencv2/opencv.hpp"
#include <vector>
#include <string>

class EigenspaceClassifier {
public:
    static constexpr int IMG_SIZE = 64;   // resize ROIs to IMG_SIZE × IMG_SIZE
    static constexpr int N_EVEC  = 10;    // number of eigenvectors to retain

    // Build eigenspace from a directory of labelled images.
    // Filenames should be:  label_001.png, label_002.png, …
    // Returns number of training images processed.
    int buildFromDirectory(const std::string &dir);

    // Project a single preprocessed image (already resized to IMG_SIZE×IMG_SIZE)
    // into the eigenspace.  Returns the embedding vector (N_EVEC dimensions).
    std::vector<double> project(const cv::Mat &img) const;

    // Classify by comparing SSD of embeddings to all training embeddings.
    // Returns predicted label and the SSD distance.
    std::string classify(const cv::Mat &img, double &distance) const;

    // Save eigenspace (mean, eigenvectors, training embeddings) to files.
    void save(const std::string &prefix) const;

    // Load eigenspace from files.
    bool load(const std::string &prefix);

    bool isBuilt() const { return built; }

private:
    bool built = false;
    cv::Mat meanVec;             // 1 × (IMG_SIZE*IMG_SIZE) float
    cv::Mat eigenVectors;        // N_EVEC × (IMG_SIZE*IMG_SIZE) float  (rows are eigenvectors)

    // Training embeddings
    struct EigenEntry {
        std::string label;
        std::vector<double> embedding;
    };
    std::vector<EigenEntry> trainEntries;
};