/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
  classifier.cpp
  CS 5330 - Project 3
  Object classifier with CSV-based training DB, scaled Euclidean distance
  nearest-neighbour recognition, and confusion matrix evaluation.
*/
#include "classifier.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <set>
#include <numeric>

// ─── Load training DB from CSV ─────────────────────────────────────────────
int ObjectClassifier::loadDB(const std::string &path)
{
    entries.clear();
    std::ifstream fin(path);
    if (!fin.is_open()) return 0;

    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string label;
        if (!std::getline(ss, label, ',')) continue;

        TrainingEntry entry;
        entry.label = label;
        double val;
        char comma;
        while (ss >> val) {
            entry.features.push_back(val);
            ss >> comma; // consume comma
        }
        if (!entry.features.empty())
            entries.push_back(entry);
    }
    return static_cast<int>(entries.size());
}

// ─── Save training DB to CSV ───────────────────────────────────────────────
void ObjectClassifier::saveDB(const std::string &path) const
{
    std::ofstream fout(path);
    fout << std::fixed << std::setprecision(8);
    for (auto &e : entries) {
        fout << e.label;
        for (double v : e.features)
            fout << "," << v;
        fout << "\n";
    }
}

// ─── Add a training entry ──────────────────────────────────────────────────
void ObjectClassifier::addEntry(const std::string &label,
                                const std::vector<double> &features)
{
    entries.push_back({label, features});
}

// ─── Compute per-feature standard deviations ───────────────────────────────
std::vector<double> ObjectClassifier::computeStdDevs() const
{
    if (entries.empty()) return {};
    int dim = static_cast<int>(entries[0].features.size());
    int N   = static_cast<int>(entries.size());

    std::vector<double> mean(dim, 0.0), var(dim, 0.0);

    for (auto &e : entries)
        for (int i = 0; i < dim; i++)
            mean[i] += e.features[i];
    for (int i = 0; i < dim; i++)
        mean[i] /= N;

    for (auto &e : entries)
        for (int i = 0; i < dim; i++) {
            double d = e.features[i] - mean[i];
            var[i] += d * d;
        }

    std::vector<double> stdev(dim);
    for (int i = 0; i < dim; i++) {
        stdev[i] = std::sqrt(var[i] / std::max(N - 1, 1));
        if (stdev[i] < 1e-10) stdev[i] = 1.0; // avoid division by zero
    }
    return stdev;
}

// ─── Classify using scaled Euclidean nearest neighbour ─────────────────────
std::string ObjectClassifier::classify(const std::vector<double> &features,
                                       double &distance) const
{
    if (entries.empty()) { distance = 1e30; return "unknown"; }

    std::vector<double> stdev = computeStdDevs();
    int dim = static_cast<int>(features.size());

    double bestDist = 1e30;
    std::string bestLabel = "unknown";

    for (auto &e : entries) {
        if ((int)e.features.size() != dim) continue;
        double dist = 0.0;
        for (int i = 0; i < dim; i++) {
            double d = (features[i] - e.features[i]) / stdev[i];
            dist += d * d;
        }
        dist = std::sqrt(dist);
        if (dist < bestDist) {
            bestDist = dist;
            bestLabel = e.label;
        }
    }

    distance = bestDist;
    return bestLabel;
}

// ─── Confusion matrix helpers ──────────────────────────────────────────────
void ObjectClassifier::recordResult(const std::string &trueLabel,
                                    const std::string &predictedLabel)
{
    confMatrix[trueLabel][predictedLabel]++;
}

void ObjectClassifier::resetConfusionMatrix()
{
    confMatrix.clear();
}

void ObjectClassifier::printConfusionMatrix() const
{
    // Collect all labels
    std::set<std::string> labels;
    for (auto &row : confMatrix) {
        labels.insert(row.first);
        for (auto &col : row.second)
            labels.insert(col.first);
    }

    std::vector<std::string> lblVec(labels.begin(), labels.end());
    int n = static_cast<int>(lblVec.size());

    // Header
    std::cout << "\n=== Confusion Matrix (rows=true, cols=predicted) ===\n";
    std::cout << std::setw(15) << " ";
    for (auto &l : lblVec)
        std::cout << std::setw(12) << l;
    std::cout << "\n";

    int correct = 0, total = 0;
    for (auto &trueL : lblVec) {
        std::cout << std::setw(15) << trueL;
        for (auto &predL : lblVec) {
            int count = 0;
            auto rowIt = confMatrix.find(trueL);
            if (rowIt != confMatrix.end()) {
                auto colIt = rowIt->second.find(predL);
                if (colIt != rowIt->second.end())
                    count = colIt->second;
            }
            std::cout << std::setw(12) << count;
            total += count;
            if (trueL == predL) correct += count;
        }
        std::cout << "\n";
    }

    if (total > 0)
        std::cout << "Accuracy: " << correct << "/" << total
                  << " = " << std::fixed << std::setprecision(1)
                  << (100.0 * correct / total) << "%\n\n";
}

std::vector<std::string> ObjectClassifier::getLabels() const
{
    std::set<std::string> uniq;
    for (auto &e : entries)
        uniq.insert(e.label);
    return std::vector<std::string>(uniq.begin(), uniq.end());
}