/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
thresholding.h
  CS 5330 - Project 3
  Custom thresholding written from scratch (no cv::threshold).
  Uses HSV saturation+value scoring with ISODATA (K=2 k-means) for
  adaptive threshold selection.
*/
#pragma once

#include "opencv2/opencv.hpp"

// Compute a single-channel "object score" image from BGR input.
// High values → likely object; low values → likely background.
// Formula per pixel: score = (255 - V) + S   (clamped to [0,255])
// where V = value and S = saturation in HSV.
cv::Mat computeObjectScore(const cv::Mat &bgr);

// ISODATA algorithm (K-means with K=2) on a sampled subset of pixel values.
// Returns the threshold that separates the two clusters (midpoint of means).
int isodataThreshold(const cv::Mat &scoreImg, int sampleStep = 4);

// Apply a binary threshold: dst(r,c) = 255 if src(r,c) >= thresh, else 0.
// Written from scratch — no cv::threshold.
cv::Mat applyThreshold(const cv::Mat &scoreImg, int thresh);

// Convenience: full pipeline  BGR → binary foreground mask.
cv::Mat thresholdImage(const cv::Mat &bgr);