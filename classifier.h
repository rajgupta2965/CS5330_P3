/*
classifier.h
  CS 5330 - Project 3
  Training data management, nearest-neighbour classification with
  scaled Euclidean distance, and confusion matrix evaluation.
*/
#pragma once

#include "vision.h"
#include <vector>
#include <string>
#include <map>

class ObjectClassifier {
public:
    // Load training DB from CSV file.  Returns number of entries loaded.
    int loadDB(const std::string &path);

    // Save training DB to CSV file.
    void saveDB(const std::string &path) const;

    // Add a training entry.
    void addEntry(const std::string &label, const std::vector<double> &features);

    // Classify a feature vector using scaled Euclidean nearest-neighbour.
    // Returns the predicted label and the distance to the nearest neighbour.
    std::string classify(const std::vector<double> &features, double &distance) const;

    // Record a classification result for the confusion matrix.
    void recordResult(const std::string &trueLabel, const std::string &predictedLabel);

    // Print the confusion matrix to stdout.
    void printConfusionMatrix() const;

    // Reset the confusion matrix.
    void resetConfusionMatrix();

    // How many entries in the DB?
    int size() const { return static_cast<int>(entries.size()); }

    // Get all unique labels in the DB.
    std::vector<std::string> getLabels() const;

private:
    std::vector<TrainingEntry> entries;

    // Confusion matrix: confMatrix[true_label][predicted_label] = count
    std::map<std::string, std::map<std::string, int>> confMatrix;

    // Compute per-feature standard deviations across the training set.
    std::vector<double> computeStdDevs() const;
};