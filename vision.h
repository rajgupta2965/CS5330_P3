/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
vision.h
  CS 5330 - Project 3: Real-time 2-D Object Recognition
  Common structs, enums, and declarations used across all modules.
*/
#pragma once

#include "opencv2/opencv.hpp"
#include <vector>
#include <string>
#include <cmath>

// ─── Region statistics from connected components ───────────────────────────
struct RegionStats {
    int id;             // region label
    int area;           // pixel count
    int left, top;      // axis-aligned bounding box
    int width, height;
    double centroidX, centroidY;
    bool touchesBorder; // true if region touches image edge
};

// ─── Full feature set for a region ─────────────────────────────────────────
struct RegionFeatures {
    int regionId;
    double centroidX, centroidY;
    double theta;       // orientation of primary axis (radians)

    // oriented bounding box extents (projections onto eigenvectors)
    double minE1, maxE1;   // along primary axis
    double minE2, maxE2;   // along secondary axis

    // translation / scale / rotation invariant features
    double percentFilled;  // region area / oriented BB area
    double aspectRatio;    // oriented BB  max(w,h)/min(w,h) — always >= 1
    double huMoments[7];   // Hu's seven moment invariants
    double logHu[7];       // log-transformed Hu moments

    // build the feature vector used for classification
    std::vector<double> getFeatureVector() const {
        return { percentFilled, aspectRatio,
                 logHu[0], logHu[1], logHu[2],
                 logHu[3], logHu[4], logHu[5], logHu[6] };
    }
};

// ─── Training entry stored in the object DB ────────────────────────────────
struct TrainingEntry {
    std::string label;
    std::vector<double> features;
};