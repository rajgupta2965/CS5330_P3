/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
features.h
  CS 5330 - Project 3
  Custom feature computation — moments, orientation, oriented bounding box,
  and Hu moment invariants.  Written from scratch (no cv::moments / cv::HuMoments).
  Also provides declarations used by utilities.cpp for the embedding pipeline.
*/
#pragma once

#include "opencv2/opencv.hpp"
#include "vision.h"
#include <vector>

// Compute full features for a single region given a label map and region ID.
// Includes: centroid, orientation, oriented bounding box, percent filled,
// aspect ratio, and Hu's seven moment invariants.
RegionFeatures computeRegionFeatures(const cv::Mat &labelMap, int regionId);

// Draw the oriented bounding box and primary axis on an image.
void drawOrientedBB(cv::Mat &display, const RegionFeatures &feat,
                    cv::Scalar bbColour  = cv::Scalar(0, 255, 0),
                    cv::Scalar axColour  = cv::Scalar(0, 0, 255),
                    int thickness = 2);

// Draw a text label near the centroid.
void drawLabel(cv::Mat &display, const RegionFeatures &feat,
               const std::string &label,
               cv::Scalar colour = cv::Scalar(255, 255, 0));