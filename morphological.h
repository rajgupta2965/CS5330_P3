/*
morphological.h
  CS 5330 - Project 3
  Custom morphological operations written from scratch
  (no cv::erode, cv::dilate, cv::morphologyEx).
*/
#pragma once

#include "opencv2/opencv.hpp"

// Erode: a foreground pixel survives only if every pixel under the
// structuring element is also foreground.
cv::Mat customErode(const cv::Mat &binary, int radius);

// Dilate: a background pixel becomes foreground if any pixel under the
// structuring element is foreground.
cv::Mat customDilate(const cv::Mat &binary, int radius);

// Opening = erode then dilate (removes small foreground noise).
cv::Mat customOpening(const cv::Mat &binary, int radius);

// Closing = dilate then erode (fills small holes).
cv::Mat customClosing(const cv::Mat &binary, int radius);

// Full cleanup pipeline: closing (fill holes) → opening (remove noise).
cv::Mat morphologicalCleanup(const cv::Mat &binary, int closeRadius = 3, int openRadius = 2);