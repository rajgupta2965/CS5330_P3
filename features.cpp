/*
  features.cpp
  CS 5330 - Project 3
  Custom feature computation — written entirely from scratch.

  Computes, for each region:
  1. Raw moments  m00, m10, m01, m20, m11, m02, m30, m21, m12, m03
  2. Centroid  (cx, cy)
  3. Central moments  mu20, mu11, mu02, mu30, mu21, mu12, mu03
  4. Orientation angle θ from the 2×2 inertia tensor
  5. Oriented bounding box by projecting region pixels onto the eigenvectors
  6. Percent filled  =  area / (OBB width × OBB height)
  7. Aspect ratio    =  max(w,h) / min(w,h)
  8. Normalised central moments  η_pq
  9. Hu's seven moment invariants  (translation, scale, rotation invariant)
  10. Log-transformed Hu moments for use as a feature vector

  No OpenCV moment / Hu functions are used.
*/
#include "features.h"
#include <cmath>
#include <algorithm>

// ──────────────────────────────────────────────────────────────────────────
RegionFeatures computeRegionFeatures(const cv::Mat &labelMap, int regionId)
{
    int rows = labelMap.rows, cols = labelMap.cols;
    RegionFeatures feat{};
    feat.regionId = regionId;

    // ── 1. Collect region pixels and raw moments ──
    double m00 = 0;
    double m10 = 0, m01 = 0;
    double m20 = 0, m11 = 0, m02 = 0;
    double m30 = 0, m21 = 0, m12 = 0, m03 = 0;

    std::vector<cv::Point> pixels; // store for OBB projection

    for (int r = 0; r < rows; r++) {
        const int *labRow = labelMap.ptr<int>(r);
        for (int c = 0; c < cols; c++) {
            if (labRow[c] != regionId) continue;

            double x = c, y = r;
            m00 += 1;
            m10 += x;       m01 += y;
            m20 += x * x;   m11 += x * y;   m02 += y * y;
            m30 += x*x*x;   m21 += x*x*y;   m12 += x*y*y;   m03 += y*y*y;

            pixels.emplace_back(c, r);
        }
    }

    if (m00 < 1) return feat; // empty region

    // ── 2. Centroid ──
    double cx = m10 / m00;
    double cy = m01 / m00;
    feat.centroidX = cx;
    feat.centroidY = cy;

    // ── 3. Central moments ──
    double mu20 = m20 - cx * m10;
    double mu11 = m11 - cx * m01;
    double mu02 = m02 - cy * m01;

    double mu30 = m30 - 3*cx*m20 + 2*cx*cx*m10;
    double mu21 = m21 - 2*cx*m11 - cy*m20 + 2*cx*cx*m01;
    double mu12 = m12 - 2*cy*m11 - cx*m02 + 2*cy*cy*m10;
    double mu03 = m03 - 3*cy*m02 + 2*cy*cy*m01;

    // ── 4. Orientation angle ──
    // θ = 0.5 * atan2(2*mu11, mu20 - mu02)
    double theta = 0.5 * std::atan2(2.0 * mu11, mu20 - mu02);
    feat.theta = theta;

    // ── 5. Oriented bounding box ──
    // Primary axis direction
    double cosT = std::cos(theta);
    double sinT = std::sin(theta);

    double minE1 =  1e30, maxE1 = -1e30;
    double minE2 =  1e30, maxE2 = -1e30;

    for (auto &pt : pixels) {
        double dx = pt.x - cx;
        double dy = pt.y - cy;
        // project onto primary axis (e1) and secondary axis (e2)
        double proj1 =  dx * cosT + dy * sinT;   // along primary
        double proj2 = -dx * sinT + dy * cosT;    // along secondary
        if (proj1 < minE1) minE1 = proj1;
        if (proj1 > maxE1) maxE1 = proj1;
        if (proj2 < minE2) minE2 = proj2;
        if (proj2 > maxE2) maxE2 = proj2;
    }

    feat.minE1 = minE1;  feat.maxE1 = maxE1;
    feat.minE2 = minE2;  feat.maxE2 = maxE2;

    // ── 6. Percent filled ──
    double obbW = maxE1 - minE1;
    double obbH = maxE2 - minE2;
    double obbArea = std::max(obbW * obbH, 1.0);
    feat.percentFilled = m00 / obbArea;

    // ── 7. Aspect ratio  (always >= 1) ──
    double longSide  = std::max(obbW, obbH);
    double shortSide = std::max(std::min(obbW, obbH), 1.0);
    feat.aspectRatio = longSide / shortSide;

    // ── 8. Normalised central moments η_pq = μ_pq / m00^((p+q)/2 + 1) ──
    double n2 = std::pow(m00, 2.0);   // (2+0)/2 + 1 = 2
    double n2_5 = std::pow(m00, 2.5); // (3+0)/2 + 1 = 2.5

    double eta20 = mu20 / n2;
    double eta11 = mu11 / n2;
    double eta02 = mu02 / n2;
    double eta30 = mu30 / n2_5;
    double eta21 = mu21 / n2_5;
    double eta12 = mu12 / n2_5;
    double eta03 = mu03 / n2_5;

    // ── 9. Hu's seven moment invariants ──
    double &h1 = feat.huMoments[0];
    double &h2 = feat.huMoments[1];
    double &h3 = feat.huMoments[2];
    double &h4 = feat.huMoments[3];
    double &h5 = feat.huMoments[4];
    double &h6 = feat.huMoments[5];
    double &h7 = feat.huMoments[6];

    h1 = eta20 + eta02;

    h2 = (eta20 - eta02) * (eta20 - eta02) + 4.0 * eta11 * eta11;

    h3 = (eta30 - 3.0*eta12) * (eta30 - 3.0*eta12)
       + (3.0*eta21 - eta03)  * (3.0*eta21 - eta03);

    h4 = (eta30 + eta12) * (eta30 + eta12)
       + (eta21 + eta03)  * (eta21 + eta03);

    double a = eta30 + eta12;
    double b = eta21 + eta03;
    double c1 = eta30 - 3.0*eta12;
    double d = 3.0*eta21 - eta03;

    h5 = c1 * a * (a*a - 3.0*b*b) + d * b * (3.0*a*a - b*b);

    h6 = (eta20 - eta02) * (a*a - b*b) + 4.0 * eta11 * a * b;

    h7 = d * a * (a*a - 3.0*b*b) - c1 * b * (3.0*a*a - b*b);

    // ── 10. Log-transformed Hu moments ──
    for (int i = 0; i < 7; i++) {
        double val = feat.huMoments[i];
        if (std::abs(val) < 1e-30)
            feat.logHu[i] = 0.0;
        else
            feat.logHu[i] = -1.0 * std::copysign(1.0, val) * std::log10(std::abs(val));
    }

    return feat;
}

// ─── Draw the oriented bounding box and primary axis ───────────────────────
void drawOrientedBB(cv::Mat &display, const RegionFeatures &feat,
                    cv::Scalar bbColour, cv::Scalar axColour, int thickness)
{
    double cx = feat.centroidX;
    double cy = feat.centroidY;
    double cosT = std::cos(feat.theta);
    double sinT = std::sin(feat.theta);

    // 4 corners of the oriented BB
    auto corner = [&](double e1, double e2) -> cv::Point {
        double x = cx + e1 * cosT - e2 * sinT;
        double y = cy + e1 * sinT + e2 * cosT;
        return cv::Point(cvRound(x), cvRound(y));
    };

    cv::Point p1 = corner(feat.minE1, feat.minE2);
    cv::Point p2 = corner(feat.maxE1, feat.minE2);
    cv::Point p3 = corner(feat.maxE1, feat.maxE2);
    cv::Point p4 = corner(feat.minE1, feat.maxE2);

    cv::line(display, p1, p2, bbColour, thickness);
    cv::line(display, p2, p3, bbColour, thickness);
    cv::line(display, p3, p4, bbColour, thickness);
    cv::line(display, p4, p1, bbColour, thickness);

    // Primary axis line through centroid (length = extent along primary axis)
    double axLen = (feat.maxE1 - feat.minE1) * 0.5;
    cv::Point axStart(cvRound(cx + feat.minE1 * cosT), cvRound(cy + feat.minE1 * sinT));
    cv::Point axEnd  (cvRound(cx + feat.maxE1 * cosT), cvRound(cy + feat.maxE1 * sinT));
    cv::line(display, axStart, axEnd, axColour, thickness);

    // Small circle at centroid
    cv::circle(display, cv::Point(cvRound(cx), cvRound(cy)), 4, axColour, -1);
}

// ─── Draw label text near centroid ─────────────────────────────────────────
void drawLabel(cv::Mat &display, const RegionFeatures &feat,
               const std::string &label, cv::Scalar colour)
{
    int baseline = 0;
    cv::Size textSz = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                       3.0, 5, &baseline);
    cv::Point org(cvRound(feat.centroidX) - textSz.width / 2,
                  cvRound(feat.centroidY) - 40);

    // Background rectangle for readability
    cv::rectangle(display,
                  cv::Point(org.x - 2, org.y - textSz.height - 2),
                  cv::Point(org.x + textSz.width + 2, org.y + baseline + 2),
                  cv::Scalar(0, 0, 0), cv::FILLED);

    cv::putText(display, label, org, cv::FONT_HERSHEY_SIMPLEX,
                3.0, colour, 5);
}