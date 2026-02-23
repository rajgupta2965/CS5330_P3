/*
  morphological.cpp
  CS 5330 - Project 3
  Custom morphological filtering — written entirely from scratch.

  Uses a circular (disk) structuring element of configurable radius.
  The cleanup strategy is:
    1. Closing (dilate → erode) to fill small holes inside objects.
    2. Opening (erode → dilate) to remove small noise blobs.
  This is the classic "grow/shrink" approach recommended in the project.
*/
#include "morphological.h"
#include <vector>

// ─── Build a circular structuring element mask ─────────────────────────────
static std::vector<std::pair<int,int>> buildDisk(int radius)
{
    std::vector<std::pair<int,int>> offsets;
    int r2 = radius * radius;
    for (int dr = -radius; dr <= radius; dr++) {
        for (int dc = -radius; dc <= radius; dc++) {
            if (dr * dr + dc * dc <= r2)
                offsets.emplace_back(dr, dc);
        }
    }
    return offsets;
}

// ─── Erosion from scratch ──────────────────────────────────────────────────
cv::Mat customErode(const cv::Mat &binary, int radius)
{
    auto offsets = buildDisk(radius);
    int rows = binary.rows, cols = binary.cols;
    cv::Mat out = cv::Mat::zeros(rows, cols, CV_8UC1);

    for (int r = 0; r < rows; r++) {
        uchar *outRow = out.ptr<uchar>(r);
        for (int c = 0; c < cols; c++) {
            bool allFG = true;
            for (auto &[dr, dc] : offsets) {
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) {
                    allFG = false; break;   // treat border as background
                }
                if (binary.at<uchar>(nr, nc) == 0) {
                    allFG = false; break;
                }
            }
            outRow[c] = allFG ? 255 : 0;
        }
    }
    return out;
}

// ─── Dilation from scratch ─────────────────────────────────────────────────
cv::Mat customDilate(const cv::Mat &binary, int radius)
{
    auto offsets = buildDisk(radius);
    int rows = binary.rows, cols = binary.cols;
    cv::Mat out = cv::Mat::zeros(rows, cols, CV_8UC1);

    for (int r = 0; r < rows; r++) {
        uchar *outRow = out.ptr<uchar>(r);
        for (int c = 0; c < cols; c++) {
            bool anyFG = false;
            for (auto &[dr, dc] : offsets) {
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols)
                    continue;
                if (binary.at<uchar>(nr, nc) != 0) {
                    anyFG = true; break;
                }
            }
            outRow[c] = anyFG ? 255 : 0;
        }
    }
    return out;
}

// ─── Opening = erode then dilate ───────────────────────────────────────────
cv::Mat customOpening(const cv::Mat &binary, int radius)
{
    return customDilate(customErode(binary, radius), radius);
}

// ─── Closing = dilate then erode ───────────────────────────────────────────
cv::Mat customClosing(const cv::Mat &binary, int radius)
{
    return customErode(customDilate(binary, radius), radius);
}

// ─── Full cleanup pipeline ─────────────────────────────────────────────────
cv::Mat morphologicalCleanup(const cv::Mat &binary, int closeRadius, int openRadius)
{
    // 1. Close: fill small holes in objects
    cv::Mat closed = customClosing(binary, closeRadius);
    // 2. Open: remove small isolated noise blobs
    cv::Mat opened = customOpening(closed, openRadius);
    return opened;
}