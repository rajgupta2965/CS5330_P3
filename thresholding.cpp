/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
  thresholding.cpp
  CS 5330 - Project 3
  Custom thresholding — written entirely from scratch.

  Strategy:
  1. Convert BGR → HSV.
  2. Compute a per-pixel "object score" that combines low Value (dark objects)
     and high Saturation (coloured objects) into a single channel.
     score = clamp( (255 - V) + S , 0, 255 )
     This makes both dark and saturated objects score high while the white
     background scores low.
  3. Run the ISODATA algorithm (iterative K=2 k-means) on a sub-sampled set of
     score values to find an adaptive threshold.
  4. Binarise: pixels with score >= threshold are foreground (255), rest are 0.
*/
#include "thresholding.h"
#include <vector>
#include <cmath>
#include <algorithm>

// ──────────────────────────────────────────────────────────────────────────
cv::Mat computeObjectScore(const cv::Mat &bgr)
{
    // Convert to HSV manually (we still use cvtColor — thresholding logic is
    // the from-scratch part, colour-space conversion is just a utility).
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    int rows = hsv.rows;
    int cols = hsv.cols;
    cv::Mat score(rows, cols, CV_8UC1);

    for (int r = 0; r < rows; r++) {
        const uchar *hsvRow = hsv.ptr<uchar>(r);
        uchar       *outRow = score.ptr<uchar>(r);
        for (int c = 0; c < cols; c++) {
            int S = hsvRow[c * 3 + 1];   // saturation  0-255
            int V = hsvRow[c * 3 + 2];   // value       0-255
            int val = (255 - V) + S;
            if (val < 0)   val = 0;
            if (val > 255) val = 255;
            outRow[c] = static_cast<uchar>(val);
        }
    }
    return score;
}

// ──────────────────────────────────────────────────────────────────────────
int isodataThreshold(const cv::Mat &scoreImg, int sampleStep)
{
    // 1. Collect a sub-sample of pixel values
    std::vector<int> samples;
    samples.reserve((scoreImg.rows / sampleStep) * (scoreImg.cols / sampleStep));

    for (int r = 0; r < scoreImg.rows; r += sampleStep) {
        const uchar *row = scoreImg.ptr<uchar>(r);
        for (int c = 0; c < scoreImg.cols; c += sampleStep) {
            samples.push_back(row[c]);
        }
    }

    if (samples.empty()) return 128; // fallback

    // 2. Initialise two means: min and max of the sample
    double mean1 = *std::min_element(samples.begin(), samples.end());
    double mean2 = *std::max_element(samples.begin(), samples.end());

    // 3. Iterate until convergence
    for (int iter = 0; iter < 100; iter++) {
        double sum1 = 0, sum2 = 0;
        int    cnt1 = 0, cnt2 = 0;

        double midpoint = (mean1 + mean2) / 2.0;

        for (int v : samples) {
            if (v < midpoint) {
                sum1 += v; cnt1++;
            } else {
                sum2 += v; cnt2++;
            }
        }

        double newMean1 = (cnt1 > 0) ? sum1 / cnt1 : mean1;
        double newMean2 = (cnt2 > 0) ? sum2 / cnt2 : mean2;

        if (std::abs(newMean1 - mean1) < 0.5 && std::abs(newMean2 - mean2) < 0.5)
            break;

        mean1 = newMean1;
        mean2 = newMean2;
    }

    // Threshold = midpoint of the two converged means
    int thresh = static_cast<int>((mean1 + mean2) / 2.0);
    return thresh;
}

// ──────────────────────────────────────────────────────────────────────────
cv::Mat applyThreshold(const cv::Mat &scoreImg, int thresh)
{
    int rows = scoreImg.rows;
    int cols = scoreImg.cols;
    cv::Mat binary(rows, cols, CV_8UC1);

    for (int r = 0; r < rows; r++) {
        const uchar *in  = scoreImg.ptr<uchar>(r);
        uchar       *out = binary.ptr<uchar>(r);
        for (int c = 0; c < cols; c++) {
            out[c] = (in[c] >= thresh) ? 255 : 0;
        }
    }
    return binary;
}

// ──────────────────────────────────────────────────────────────────────────
cv::Mat thresholdImage(const cv::Mat &bgr)
{
    cv::Mat score = computeObjectScore(bgr);
    int thresh    = isodataThreshold(score);
    return applyThreshold(score, thresh);
}