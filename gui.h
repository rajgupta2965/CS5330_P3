/*
  gui.h
  CS 5330 - Project 3
  Qt-based GUI for the object recognition system.
*/
#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QTimer>
#include <QSpinBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QStatusBar>
#include <QFileDialog>

#include "opencv2/opencv.hpp"
#include "opencv2/dnn.hpp"

#include "vision.h"
#include "classifier.h"
#include "eigenspace.h"

// Forward declarations from utilities.cpp
int  getEmbedding(cv::Mat &src, cv::Mat &embedding, cv::dnn::Net &net, int debug);
void prepEmbeddingImage(cv::Mat &frame, cv::Mat &embimage,
                        int cx, int cy, float theta,
                        float minE1, float maxE1, float minE2, float maxE2,
                        int debug);

// ─── CNN embedding DB (shared with gui.cpp) ────────────────────────────────
struct CNNEntry {
    std::string label;
    std::vector<float> embedding;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // Source
    void onOpenWebcam();
    void onOpenImage();
    void onOpenDirectory();
    void onOpenVideo();
    void onNextImage();
    void onPrevImage();

    // Pipeline
    void onTimerTick();
    void processAndDisplay();

    // Training & classification
    void onTrainFeatures();
    void onTrainCNNEmbedding();
    void onSaveROI();
    void onBuildEigenspace();
    void onGroundTruth();
    void onPrintConfusion();
    void onResetConfusion();
    void onClearDB();

    // Parameters
    void onThreshBiasChanged(int val);
    void onCloseRadiusChanged(int val);
    void onOpenRadiusChanged(int val);
    void onMinAreaChanged(int val);
    void onBlurChanged(int val);
    void onMethodChanged(int index);
    void onClassifyToggled(bool on);

    // Misc
    void onSaveScreenshot();

private:
    // ── GUI widgets ──
    // Image panels
    QLabel *panelOriginal;
    QLabel *panelThresh;
    QLabel *panelCleaned;
    QLabel *panelRegions;

    // Source controls
    QPushButton *btnWebcam, *btnImage, *btnDirectory, *btnVideo;
    QPushButton *btnPrev, *btnNext;
    QLabel      *lblSource;

    // Classification controls
    QComboBox   *cmbMethod;
    QCheckBox   *chkClassify;
    QPushButton *btnTrainFeatures, *btnTrainCNN, *btnSaveROI;
    QPushButton *btnBuildEigen;
    QPushButton *btnGroundTruth, *btnPrintConfusion, *btnResetConfusion;
    QPushButton *btnClearDB;

    // Parameter controls
    QSlider  *sldThreshBias;
    QSpinBox *spnCloseR, *spnOpenR, *spnMinArea, *spnBlur;
    QLabel   *lblThreshVal;

    // Misc
    QPushButton *btnScreenshot;
    QTextEdit   *txtLog;

    // ── Pipeline state ──
    QTimer *timer;

    cv::VideoCapture cap;
    std::vector<std::string> imageFiles;
    int imageIdx = 0;
    cv::Mat lastFrame;
    cv::Mat lastDisplay;

    enum SourceMode { NONE, WEBCAM, IMAGE, DIRECTORY, VIDEO };
    SourceMode sourceMode = NONE;

    // Parameters
    int threshBias   = 20;
    int closeRadius  = 4;
    int openRadius   = 3;
    int minArea      = 800;
    int blurKSize    = 5;
    int classMethod  = 0; // 0=features, 1=eigen, 2=CNN
    bool classifyOn  = true;
    float unknownThresh = 6.0f;

    // Classifiers
    ObjectClassifier classifier;
    EigenspaceClassifier eigenClassifier;
    cv::dnn::Net dnnNet;
    bool hasDNN = false;
    std::vector<CNNEntry> cnnDB;

    int roiCounter  = 0;
    int saveCounter = 0;

    // Latest features for training buttons
    std::vector<RegionFeatures> currentFeats;

    // ── Helpers ──
    QPixmap matToPixmap(const cv::Mat &mat, int maxW, int maxH);
    void    log(const QString &msg);
    void    loadCNNDB();
    void    saveCNNDB();
    std::string classifyCNN(const std::vector<float> &emb, double &distance);
    cv::Mat prepROI(cv::Mat &frame, const RegionFeatures &feat);
    QGroupBox *createSourceGroup();
    QGroupBox *createClassifyGroup();
    QGroupBox *createParamsGroup();
    QGroupBox *createActionsGroup();
};