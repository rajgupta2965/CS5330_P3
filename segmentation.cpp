/*
  segmentation.cpp
  CS 5330 - Project 3
  Custom connected-components — written entirely from scratch.

  Algorithm: classic two-pass with union-find (8-connectivity).
  Pass 1: scan left-to-right, top-to-bottom.  For each foreground pixel
           check already-visited neighbours (up-left, up, up-right, left).
           Assign the minimum existing label or a new label; record
           equivalences in the union-find structure.
  Pass 2: resolve all equivalences and relabel the image.
  Then compute region statistics (area, centroid, AABB, border flag).
*/
#include "segmentation.h"
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <ctime>

// ─── Union-Find helpers ────────────────────────────────────────────────────
static int ufFind(std::vector<int> &parent, int x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]]; // path compression
        x = parent[x];
    }
    return x;
}

static void ufUnion(std::vector<int> &parent, int a, int b)
{
    int ra = ufFind(parent, a);
    int rb = ufFind(parent, b);
    if (ra != rb) {
        if (ra < rb) parent[rb] = ra;
        else         parent[ra] = rb;
    }
}

// ─── Two-pass connected components (8-connectivity) ────────────────────────
cv::Mat connectedComponents(const cv::Mat &binary,
                            std::vector<RegionStats> &stats)
{
    int rows = binary.rows, cols = binary.cols;
    cv::Mat labels(rows, cols, CV_32SC1, cv::Scalar(0));

    std::vector<int> parent;
    parent.push_back(0);            // index 0 = background
    int nextLabel = 1;

    // Neighbour offsets: up-left, up, up-right, left  (already visited)
    const int drs[] = { -1, -1, -1,  0 };
    const int dcs[] = { -1,  0,  1, -1 };
    const int numNeighbours = 4;

    // ── Pass 1: initial labelling ──
    for (int r = 0; r < rows; r++) {
        const uchar *binRow = binary.ptr<uchar>(r);
        int         *labRow = labels.ptr<int>(r);
        for (int c = 0; c < cols; c++) {
            if (binRow[c] == 0) continue;  // background

            int minLabel = 0; // 0 = no neighbour label found
            std::vector<int> neighbourLabels;

            for (int n = 0; n < numNeighbours; n++) {
                int nr = r + drs[n], nc = c + dcs[n];
                if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
                int nl = labels.at<int>(nr, nc);
                if (nl > 0) {
                    nl = ufFind(parent, nl);
                    neighbourLabels.push_back(nl);
                    if (minLabel == 0 || nl < minLabel) minLabel = nl;
                }
            }

            if (minLabel == 0) {
                // no labelled neighbour → new region
                labRow[c] = nextLabel;
                parent.push_back(nextLabel);
                nextLabel++;
            } else {
                labRow[c] = minLabel;
                // union all neighbour labels
                for (int nl : neighbourLabels)
                    ufUnion(parent, minLabel, nl);
            }
        }
    }

    // ── Pass 2: resolve equivalences & renumber ──
    // Build mapping: old root → new sequential ID
    std::unordered_map<int, int> rootToNew;
    int newId = 0; // 0 reserved for background
    for (int i = 1; i < nextLabel; i++) {
        int root = ufFind(parent, i);
        if (rootToNew.find(root) == rootToNew.end()) {
            newId++;
            rootToNew[root] = newId;
        }
    }

    int numRegions = newId;

    // Relabel and collect per-region accumulators
    std::vector<long long> sumX(numRegions + 1, 0), sumY(numRegions + 1, 0);
    std::vector<int> area(numRegions + 1, 0);
    std::vector<int> minR(numRegions + 1, rows), maxR(numRegions + 1, 0);
    std::vector<int> minC(numRegions + 1, cols), maxC(numRegions + 1, 0);
    std::vector<bool> border(numRegions + 1, false);

    for (int r = 0; r < rows; r++) {
        int *labRow = labels.ptr<int>(r);
        for (int c = 0; c < cols; c++) {
            int old = labRow[c];
            if (old == 0) continue;

            int root = ufFind(parent, old);
            int id   = rootToNew[root];
            labRow[c] = id;

            area[id]++;
            sumX[id] += c;
            sumY[id] += r;
            if (r < minR[id]) minR[id] = r;
            if (r > maxR[id]) maxR[id] = r;
            if (c < minC[id]) minC[id] = c;
            if (c > maxC[id]) maxC[id] = c;

            // Check border
            if (r == 0 || r == rows - 1 || c == 0 || c == cols - 1)
                border[id] = true;
        }
    }

    // ── Build stats vector ──
    stats.clear();
    for (int id = 1; id <= numRegions; id++) {
        if (area[id] == 0) continue;
        RegionStats rs;
        rs.id          = id;
        rs.area        = area[id];
        rs.centroidX   = static_cast<double>(sumX[id]) / area[id];
        rs.centroidY   = static_cast<double>(sumY[id]) / area[id];
        rs.left        = minC[id];
        rs.top         = minR[id];
        rs.width       = maxC[id] - minC[id] + 1;
        rs.height      = maxR[id] - minR[id] + 1;
        rs.touchesBorder = border[id];
        stats.push_back(rs);
    }

    return labels;
}

// ─── Filter and renumber regions ───────────────────────────────────────────
cv::Mat filterRegions(const cv::Mat &labelMap,
                      const std::vector<RegionStats> &allStats,
                      std::vector<RegionStats> &filtered,
                      int minArea, bool removeBorder)
{
    int rows = labelMap.rows, cols = labelMap.cols;

    // Decide which old IDs survive
    std::unordered_map<int, int> oldToNew;
    filtered.clear();
    int newId = 0;

    for (auto &rs : allStats) {
        if (rs.area < minArea) continue;
        if (removeBorder && rs.touchesBorder) continue;
        newId++;
        oldToNew[rs.id] = newId;
        RegionStats ns = rs;
        ns.id = newId;
        filtered.push_back(ns);
    }

    // Build new label map
    cv::Mat out(rows, cols, CV_32SC1, cv::Scalar(0));
    for (int r = 0; r < rows; r++) {
        const int *inRow  = labelMap.ptr<int>(r);
        int       *outRow = out.ptr<int>(r);
        for (int c = 0; c < cols; c++) {
            auto it = oldToNew.find(inRow[c]);
            if (it != oldToNew.end())
                outRow[c] = it->second;
        }
    }

    return out;
}

// ─── Colourised region map for display ─────────────────────────────────────
cv::Mat colourRegionMap(const cv::Mat &labelMap, int numRegions)
{
    // Generate a fixed palette of bright, distinct colours
    static bool seeded = false;
    static std::vector<cv::Vec3b> palette;

    if (!seeded || (int)palette.size() < numRegions + 1) {
        seeded = true;
        palette.resize(numRegions + 1);
        palette[0] = cv::Vec3b(0, 0, 0); // background = black
        std::srand(42); // deterministic colours
        for (int i = 1; i <= numRegions; i++) {
            // HSV with evenly spaced hue, high saturation and value
            int hue = (i * 47) % 180;
            cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, 200 + (std::rand() % 55),
                                                   180 + (std::rand() % 75)));
            cv::Mat bgr;
            cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);
            palette[i] = bgr.at<cv::Vec3b>(0, 0);
        }
    }

    int rows = labelMap.rows, cols = labelMap.cols;
    cv::Mat colour(rows, cols, CV_8UC3, cv::Scalar(0, 0, 0));

    for (int r = 0; r < rows; r++) {
        const int     *labRow = labelMap.ptr<int>(r);
        cv::Vec3b     *colRow = colour.ptr<cv::Vec3b>(r);
        for (int c = 0; c < cols; c++) {
            int id = labRow[c];
            if (id > 0 && id <= numRegions)
                colRow[c] = palette[id];
        }
    }
    return colour;
}