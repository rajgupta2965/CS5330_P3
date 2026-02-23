/*
segmentation.h
  CS 5330 - Project 3
  Custom connected-components analysis written from scratch
  (no cv::connectedComponents / cv::connectedComponentsWithStats).
  Uses the classic two-pass algorithm with union-find.
*/
#pragma once

#include "opencv2/opencv.hpp"
#include "vision.h"
#include <vector>

// Two-pass connected-components labelling (8-connectivity).
// Returns the label map (CV_32SC1) and fills `stats` with per-region info.
// Label 0 = background.  Region IDs start at 1.
cv::Mat connectedComponents(const cv::Mat &binary,
                            std::vector<RegionStats> &stats);

// Filter out regions smaller than minArea or touching the image border.
// Re-numbers remaining regions sequentially starting at 1.
// Returns updated label map and filtered stats vector.
cv::Mat filterRegions(const cv::Mat &labelMap,
                      const std::vector<RegionStats> &allStats,
                      std::vector<RegionStats> &filtered,
                      int minArea = 500,
                      bool removeBorder = true);

// Colour-code a label map for display (each region gets a distinct colour).
cv::Mat colourRegionMap(const cv::Mat &labelMap, int numRegions);