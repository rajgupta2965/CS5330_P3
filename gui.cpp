/*
Name: Sangeeth Deleep Menon | NUID: 002524579
Course: CS5330; Pattern Recognition and Computer Vision | Section: 03 | CRN: 40669 | Online

Name: Raj Gupta | NUID: 002068701
Course: CS5330; Pattern Recognition and Computer Vision | Section: 01 | CRN: 38745 | Online
*/

/*
  gui.cpp
  CS 5330 - Project 3
  Qt-based GUI implementation.
*/
#include "gui.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QApplication>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "thresholding.h"
#include "morphological.h"
#include "segmentation.h"
#include "features.h"

namespace fs = std::filesystem;

static const std::string DB_PATH      = "object_db.csv";
static const std::string ONNX_PATH    = "resnet18-v2-7.onnx";
static const std::string ROI_DIR      = "roi_training";
static const std::string EIGEN_PREFIX = "eigen";
static const std::string CNN_DB_PATH  = "cnn_db.csv";

// ═══════════════════════════════════════════════════════════════════════════
//  Construction
// ═══════════════════════════════════════════════════════════════════════════
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("CS 5330 — Real-time 2-D Object Recognition");
    resize(1400, 850);

    // ── Image panels ──
    panelOriginal = new QLabel("Original + Overlays");
    panelThresh   = new QLabel("Thresholded");
    panelCleaned  = new QLabel("Cleaned");
    panelRegions  = new QLabel("Regions");

    for (auto *lbl : {panelOriginal, panelThresh, panelCleaned, panelRegions}) {
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setMinimumSize(320, 240);
        lbl->setStyleSheet("QLabel { background-color: #1e1e1e; color: #aaa; "
                           "border: 1px solid #444; font-size: 13px; }");
        lbl->setScaledContents(false);
    }

    QGridLayout *imageGrid = new QGridLayout;
    imageGrid->addWidget(panelOriginal, 0, 0);
    imageGrid->addWidget(panelThresh,   0, 1);
    imageGrid->addWidget(panelCleaned,  1, 0);
    imageGrid->addWidget(panelRegions,  1, 1);
    imageGrid->setSpacing(4);

    QWidget *imageArea = new QWidget;
    imageArea->setLayout(imageGrid);

    // ── Sidebar ──
    QVBoxLayout *sidebar = new QVBoxLayout;
    sidebar->addWidget(createSourceGroup());
    sidebar->addWidget(createClassifyGroup());
    sidebar->addWidget(createParamsGroup());
    sidebar->addWidget(createActionsGroup());
    sidebar->addStretch();

    QWidget *sideWidget = new QWidget;
    sideWidget->setLayout(sidebar);
    sideWidget->setFixedWidth(260);

    // ── Log panel ──
    txtLog = new QTextEdit;
    txtLog->setReadOnly(true);
    txtLog->setMaximumHeight(120);
    txtLog->setStyleSheet("QTextEdit { background-color: #1a1a2e; color: #eee; "
                          "font-family: 'Courier New', Courier; font-size: 12px; }");

    // ── Main layout ──
    QVBoxLayout *rightSide = new QVBoxLayout;
    rightSide->addWidget(imageArea, 1);
    rightSide->addWidget(txtLog);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(sideWidget);
    mainLayout->addLayout(rightSide, 1);

    QWidget *central = new QWidget;
    central->setLayout(mainLayout);
    setCentralWidget(central);

    // ── Status bar ──
    statusBar()->showMessage("Ready — open a source to begin.");

    // ── Timer for camera / video ──
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::onTimerTick);

    // ── Load DBs ──
    int n = classifier.loadDB(DB_PATH);
    log(QString("Hand-crafted DB: %1 entries").arg(n));

    if (fs::exists(ONNX_PATH)) {
        try {
            dnnNet = cv::dnn::readNetFromONNX(ONNX_PATH);
            hasDNN = true;
            log("DNN model loaded.");
        } catch (const cv::Exception &ex) {
            log(QString("DNN load failed: %1").arg(ex.what()));
        }
    } else {
        log("ONNX not found — CNN embedding disabled.");
    }

    loadCNNDB();
    log(QString("CNN DB: %1 entries").arg((int)cnnDB.size()));

    eigenClassifier.load(EIGEN_PREFIX);
    fs::create_directories(ROI_DIR);
}

MainWindow::~MainWindow()
{
    timer->stop();
    if (cap.isOpened()) cap.release();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Sidebar group builders
// ═══════════════════════════════════════════════════════════════════════════
QGroupBox *MainWindow::createSourceGroup()
{
    auto *grp = new QGroupBox("Source");
    auto *lay = new QVBoxLayout;

    btnWebcam    = new QPushButton("Webcam");
    btnImage     = new QPushButton("Open Image…");
    btnDirectory = new QPushButton("Open Directory…");
    btnVideo     = new QPushButton("Open Video…");

    QHBoxLayout *navRow = new QHBoxLayout;
    btnPrev = new QPushButton("◀ Prev");
    btnNext = new QPushButton("Next ▶");
    btnPrev->setEnabled(false);
    btnNext->setEnabled(false);
    navRow->addWidget(btnPrev);
    navRow->addWidget(btnNext);

    lblSource = new QLabel("No source");
    lblSource->setStyleSheet("color: #888;");

    lay->addWidget(btnWebcam);
    lay->addWidget(btnImage);
    lay->addWidget(btnDirectory);
    lay->addWidget(btnVideo);
    lay->addLayout(navRow);
    lay->addWidget(lblSource);

    grp->setLayout(lay);

    connect(btnWebcam,    &QPushButton::clicked, this, &MainWindow::onOpenWebcam);
    connect(btnImage,     &QPushButton::clicked, this, &MainWindow::onOpenImage);
    connect(btnDirectory, &QPushButton::clicked, this, &MainWindow::onOpenDirectory);
    connect(btnVideo,     &QPushButton::clicked, this, &MainWindow::onOpenVideo);
    connect(btnPrev,      &QPushButton::clicked, this, &MainWindow::onPrevImage);
    connect(btnNext,      &QPushButton::clicked, this, &MainWindow::onNextImage);

    return grp;
}

QGroupBox *MainWindow::createClassifyGroup()
{
    auto *grp = new QGroupBox("Classification");
    auto *lay = new QVBoxLayout;

    cmbMethod = new QComboBox;
    cmbMethod->addItem("Hand-crafted Features");
    cmbMethod->addItem("Eigenspace (PCA)");
    cmbMethod->addItem("CNN (ResNet18)");

    chkClassify = new QCheckBox("Classify continuously");
    chkClassify->setChecked(true);

    btnTrainFeatures = new QPushButton("Train Features (4)");
    btnTrainCNN      = new QPushButton("Train CNN Embed (7)");
    btnSaveROI       = new QPushButton("Save ROI (5)");
    btnBuildEigen    = new QPushButton("Build Eigenspace (6)");

    lay->addWidget(cmbMethod);
    lay->addWidget(chkClassify);
    lay->addWidget(btnTrainFeatures);
    lay->addWidget(btnTrainCNN);
    lay->addWidget(btnSaveROI);
    lay->addWidget(btnBuildEigen);

    grp->setLayout(lay);

    connect(cmbMethod,       QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onMethodChanged);
    connect(chkClassify,     &QCheckBox::toggled, this, &MainWindow::onClassifyToggled);
    connect(btnTrainFeatures,&QPushButton::clicked, this, &MainWindow::onTrainFeatures);
    connect(btnTrainCNN,     &QPushButton::clicked, this, &MainWindow::onTrainCNNEmbedding);
    connect(btnSaveROI,      &QPushButton::clicked, this, &MainWindow::onSaveROI);
    connect(btnBuildEigen,   &QPushButton::clicked, this, &MainWindow::onBuildEigenspace);

    return grp;
}

QGroupBox *MainWindow::createParamsGroup()
{
    auto *grp = new QGroupBox("Parameters");
    auto *lay = new QGridLayout;

    sldThreshBias = new QSlider(Qt::Horizontal);
    sldThreshBias->setRange(-80, 80);
    sldThreshBias->setValue(0);
    lblThreshVal = new QLabel("0");

    spnCloseR  = new QSpinBox; spnCloseR->setRange(0, 15);  spnCloseR->setValue(closeRadius);
    spnOpenR   = new QSpinBox; spnOpenR->setRange(0, 15);   spnOpenR->setValue(openRadius);
    spnMinArea = new QSpinBox; spnMinArea->setRange(50, 10000); spnMinArea->setValue(minArea);
    spnMinArea->setSingleStep(100);
    spnBlur    = new QSpinBox; spnBlur->setRange(1, 21);    spnBlur->setValue(blurKSize);
    spnBlur->setSingleStep(2);

    int r = 0;
    lay->addWidget(new QLabel("Thresh Bias:"), r, 0);
    lay->addWidget(sldThreshBias, r, 1);
    lay->addWidget(lblThreshVal, r, 2);
    r++;
    lay->addWidget(new QLabel("Close R:"), r, 0);
    lay->addWidget(spnCloseR, r, 1, 1, 2);
    r++;
    lay->addWidget(new QLabel("Open R:"), r, 0);
    lay->addWidget(spnOpenR, r, 1, 1, 2);
    r++;
    lay->addWidget(new QLabel("Min Area:"), r, 0);
    lay->addWidget(spnMinArea, r, 1, 1, 2);
    r++;
    lay->addWidget(new QLabel("Blur K:"), r, 0);
    lay->addWidget(spnBlur, r, 1, 1, 2);

    grp->setLayout(lay);

    connect(sldThreshBias, &QSlider::valueChanged, this, &MainWindow::onThreshBiasChanged);
    connect(spnCloseR,  QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onCloseRadiusChanged);
    connect(spnOpenR,   QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onOpenRadiusChanged);
    connect(spnMinArea, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onMinAreaChanged);
    connect(spnBlur,    QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onBlurChanged);

    return grp;
}

QGroupBox *MainWindow::createActionsGroup()
{
    auto *grp = new QGroupBox("Evaluation");
    auto *lay = new QVBoxLayout;

    btnGroundTruth   = new QPushButton("Ground Truth (8)");
    btnPrintConfusion = new QPushButton("Print Confusion Matrix (9)");
    btnResetConfusion = new QPushButton("Reset Matrix");
    btnClearDB       = new QPushButton("Clear Training DB");
    btnScreenshot    = new QPushButton("Save Screenshot (s)");

    lay->addWidget(btnGroundTruth);
    lay->addWidget(btnPrintConfusion);
    lay->addWidget(btnResetConfusion);
    lay->addWidget(btnClearDB);
    lay->addWidget(btnScreenshot);

    grp->setLayout(lay);

    connect(btnGroundTruth,   &QPushButton::clicked, this, &MainWindow::onGroundTruth);
    connect(btnPrintConfusion,&QPushButton::clicked, this, &MainWindow::onPrintConfusion);
    connect(btnResetConfusion,&QPushButton::clicked, this, &MainWindow::onResetConfusion);
    connect(btnClearDB,       &QPushButton::clicked, this, &MainWindow::onClearDB);
    connect(btnScreenshot,    &QPushButton::clicked, this, &MainWindow::onSaveScreenshot);

    return grp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Source slots
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::onOpenWebcam()
{
    timer->stop();
    if (cap.isOpened()) cap.release();

    cap.open(0);
    if (!cap.isOpened()) {
        log("ERROR: Cannot open webcam.");
        return;
    }
    sourceMode = WEBCAM;
    lblSource->setText("Webcam");
    btnPrev->setEnabled(false);
    btnNext->setEnabled(false);
    timer->start(33); // ~30 fps
    log("Webcam opened.");
}

void MainWindow::onOpenImage()
{
    timer->stop();
    if (cap.isOpened()) cap.release();

    QString path = QFileDialog::getOpenFileName(this, "Open Image", "",
                   "Images (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty()) return;

    lastFrame = cv::imread(path.toStdString());
    if (lastFrame.empty()) { log("Cannot read image."); return; }

    sourceMode = IMAGE;
    imageFiles.clear();
    lblSource->setText(QFileInfo(path).fileName());
    btnPrev->setEnabled(false);
    btnNext->setEnabled(false);
    processAndDisplay();
    log("Opened: " + path);
}

void MainWindow::onOpenDirectory()
{
    timer->stop();
    if (cap.isOpened()) cap.release();

    QString dir = QFileDialog::getExistingDirectory(this, "Open Image Directory");
    if (dir.isEmpty()) return;

    imageFiles.clear();
    for (auto &entry : fs::directory_iterator(dir.toStdString())) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
            imageFiles.push_back(entry.path().string());
    }
    std::sort(imageFiles.begin(), imageFiles.end());

    if (imageFiles.empty()) { log("No images found."); return; }

    imageIdx = 0;
    sourceMode = DIRECTORY;
    lblSource->setText(QString("%1 images").arg(imageFiles.size()));
    btnPrev->setEnabled(true);
    btnNext->setEnabled(true);

    lastFrame = cv::imread(imageFiles[0]);
    processAndDisplay();
    log(QString("Directory: %1 images").arg(imageFiles.size()));
}

void MainWindow::onOpenVideo()
{
    timer->stop();
    if (cap.isOpened()) cap.release();

    QString path = QFileDialog::getOpenFileName(this, "Open Video", "",
                   "Video (*.mp4 *.avi *.mov *.mkv)");
    if (path.isEmpty()) return;

    cap.open(path.toStdString());
    if (!cap.isOpened()) { log("Cannot open video."); return; }

    sourceMode = VIDEO;
    lblSource->setText(QFileInfo(path).fileName());
    btnPrev->setEnabled(false);
    btnNext->setEnabled(false);
    timer->start(33);
    log("Video: " + path);
}

void MainWindow::onNextImage()
{
    if (sourceMode != DIRECTORY || imageFiles.empty()) return;
    imageIdx = (imageIdx + 1) % (int)imageFiles.size();
    lastFrame = cv::imread(imageFiles[imageIdx]);
    processAndDisplay();
    lblSource->setText(QString::fromStdString(
        fs::path(imageFiles[imageIdx]).filename().string()));
}

void MainWindow::onPrevImage()
{
    if (sourceMode != DIRECTORY || imageFiles.empty()) return;
    imageIdx = (imageIdx - 1 + (int)imageFiles.size()) % (int)imageFiles.size();
    lastFrame = cv::imread(imageFiles[imageIdx]);
    processAndDisplay();
    lblSource->setText(QString::fromStdString(
        fs::path(imageFiles[imageIdx]).filename().string()));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Timer tick — grab frame from cam/video
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::onTimerTick()
{
    if (!cap.isOpened()) return;
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        timer->stop();
        log("End of stream.");
        return;
    }
    lastFrame = frame;
    processAndDisplay();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Core pipeline + display
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::processAndDisplay()
{
    if (lastFrame.empty()) return;

    cv::Mat frame = lastFrame.clone();

    // 1. Blur
    cv::Mat blurred;
    int k = blurKSize | 1; // ensure odd
    cv::GaussianBlur(frame, blurred, cv::Size(k, k), 0);

    // 2. Threshold
    cv::Mat score = computeObjectScore(blurred);
    int thresh    = isodataThreshold(score) + threshBias;
    cv::Mat binary = applyThreshold(score, thresh);

    // 3. Morphological cleanup
    cv::Mat cleaned = morphologicalCleanup(binary, closeRadius, openRadius);

    // 4. Connected components
    std::vector<RegionStats> allStats;
    cv::Mat rawLabels = connectedComponents(cleaned, allStats);

    std::vector<RegionStats> filteredStats;
    cv::Mat labelMap = filterRegions(rawLabels, allStats, filteredStats, minArea, true);
    int numRegions = (int)filteredStats.size();
    cv::Mat regionColour = colourRegionMap(labelMap, numRegions);

    // 5. Features
    std::vector<RegionFeatures> feats;
    for (auto &rs : filteredStats)
        feats.push_back(computeRegionFeatures(labelMap, rs.id));

    // 6. Draw overlays
    cv::Mat display = frame.clone();
    std::string methodNames[] = {"Features", "Eigenspace", "CNN"};

    for (auto &feat : feats) {
        drawOrientedBB(display, feat);

        if (classifyOn) {
            std::string label;
            double dist = 1e30;

            if (classMethod == 0 && classifier.size() > 0) {
                auto fv = feat.getFeatureVector();
                label = classifier.classify(fv, dist);
                if (dist > unknownThresh) label = "unknown";
            }
            else if (classMethod == 1 && eigenClassifier.isBuilt()) {
                cv::Mat roi = prepROI(frame, feat);
                if (!roi.empty()) label = eigenClassifier.classify(roi, dist);
            }
            else if (classMethod == 2 && hasDNN && !cnnDB.empty()) {
                cv::Mat roi = prepROI(frame, feat);
                if (!roi.empty()) {
                    cv::Mat emb;
                    getEmbedding(roi, emb, dnnNet, 0);
                    std::vector<float> ev(emb.ptr<float>(0), emb.ptr<float>(0) + emb.cols);
                    label = classifyCNN(ev, dist);
                }
            }

            if (!label.empty()) {
                std::string text = label + " [" + methodNames[classMethod] + "]";
                drawLabel(display, feat, text);
            }
        }
    }

    lastDisplay = display.clone();

    // Status
    statusBar()->showMessage(QString("Regions: %1  |  Method: %2  |  Thresh: %3")
        .arg(numRegions)
        .arg(QString::fromStdString(methodNames[classMethod]))
        .arg(thresh));

    // Update panels
    int pw = panelOriginal->width();
    int ph = panelOriginal->height();

    panelOriginal->setPixmap(matToPixmap(display, pw, ph));

    cv::Mat threshBGR, cleanBGR;
    cv::cvtColor(binary, threshBGR, cv::COLOR_GRAY2BGR);
    cv::cvtColor(cleaned, cleanBGR, cv::COLOR_GRAY2BGR);

    panelThresh->setPixmap(matToPixmap(threshBGR, pw, ph));
    panelCleaned->setPixmap(matToPixmap(cleanBGR, pw, ph));
    panelRegions->setPixmap(matToPixmap(regionColour, pw, ph));

    // Store features for training/eval buttons
    currentFeats = feats;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Training & classification slots
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::onTrainFeatures()
{
    if (currentFeats.empty()) { log("No region detected."); return; }
    bool ok;
    QString label = QInputDialog::getText(this, "Train Features",
                    "Enter label for this object:", QLineEdit::Normal, "", &ok);
    if (!ok || label.isEmpty()) return;

    auto fv = currentFeats[0].getFeatureVector();
    classifier.addEntry(label.toStdString(), fv);
    classifier.saveDB(DB_PATH);
    log(QString("Trained '%1'. DB: %2 entries.").arg(label).arg(classifier.size()));
}

void MainWindow::onTrainCNNEmbedding()
{
    if (!hasDNN) { log("DNN not loaded."); return; }
    if (currentFeats.empty()) { log("No region."); return; }

    bool ok;
    QString label = QInputDialog::getText(this, "Train CNN Embedding",
                    "Enter label:", QLineEdit::Normal, "", &ok);
    if (!ok || label.isEmpty()) return;

    cv::Mat frame = lastFrame.clone();
    cv::Mat roi = prepROI(frame, currentFeats[0]);
    if (roi.empty()) { log("Empty ROI."); return; }

    cv::Mat emb;
    getEmbedding(roi, emb, dnnNet, 0);
    CNNEntry entry;
    entry.label = label.toStdString();
    entry.embedding.assign(emb.ptr<float>(0), emb.ptr<float>(0) + emb.cols);
    cnnDB.push_back(entry);
    saveCNNDB();
    log(QString("CNN embedding saved for '%1'. DB: %2").arg(label).arg((int)cnnDB.size()));
}

void MainWindow::onSaveROI()
{
    if (currentFeats.empty()) { log("No region."); return; }
    bool ok;
    QString label = QInputDialog::getText(this, "Save ROI",
                    "Enter label:", QLineEdit::Normal, "", &ok);
    if (!ok || label.isEmpty()) return;

    cv::Mat frame = lastFrame.clone();
    cv::Mat roi = prepROI(frame, currentFeats[0]);
    if (roi.empty()) { log("Empty ROI."); return; }

    std::string fname = ROI_DIR + "/" + label.toStdString() + "_"
                      + std::to_string(roiCounter++) + ".png";
    cv::imwrite(fname, roi);
    log(QString("ROI saved: %1").arg(QString::fromStdString(fname)));
}

void MainWindow::onBuildEigenspace()
{
    int n = eigenClassifier.buildFromDirectory(ROI_DIR);
    if (n > 0) {
        eigenClassifier.save(EIGEN_PREFIX);
        log(QString("Eigenspace built from %1 images.").arg(n));
    } else {
        log("No ROI images found in " + QString::fromStdString(ROI_DIR));
    }
}

void MainWindow::onGroundTruth()
{
    if (currentFeats.empty()) { log("No region."); return; }

    double dist;
    std::string predicted;
    cv::Mat frame = lastFrame.clone();

    if (classMethod == 0) {
        auto fv = currentFeats[0].getFeatureVector();
        predicted = classifier.classify(fv, dist);
        if (dist > unknownThresh) predicted = "unknown";
    } else if (classMethod == 1 && eigenClassifier.isBuilt()) {
        cv::Mat roi = prepROI(frame, currentFeats[0]);
        predicted = eigenClassifier.classify(roi, dist);
    } else if (classMethod == 2 && hasDNN && !cnnDB.empty()) {
        cv::Mat roi = prepROI(frame, currentFeats[0]);
        cv::Mat emb; getEmbedding(roi, emb, dnnNet, 0);
        std::vector<float> ev(emb.ptr<float>(0), emb.ptr<float>(0) + emb.cols);
        predicted = classifyCNN(ev, dist);
    } else {
        log("Classifier not ready."); return;
    }

    bool ok;
    QString trueLabel = QInputDialog::getText(this, "Ground Truth",
        QString("Predicted: %1 (dist=%2)\nEnter true label:")
            .arg(QString::fromStdString(predicted)).arg(dist, 0, 'f', 2),
        QLineEdit::Normal, QString::fromStdString(predicted), &ok);
    if (!ok || trueLabel.isEmpty()) return;

    classifier.recordResult(trueLabel.toStdString(), predicted);
    log(QString("GT: %1 → %2").arg(trueLabel).arg(QString::fromStdString(predicted)));
}

void MainWindow::onPrintConfusion()
{
    classifier.printConfusionMatrix();
    log("Confusion matrix printed to console.");
}

void MainWindow::onResetConfusion()
{
    classifier.resetConfusionMatrix();
    log("Confusion matrix reset.");
}

void MainWindow::onClearDB()
{
    auto reply = QMessageBox::question(this, "Clear DB",
                 "Delete all training entries?",
                 QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        // Recreate empty DB
        std::ofstream(DB_PATH).close();
        classifier.loadDB(DB_PATH);
        log("Training DB cleared.");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Parameter slots
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::onThreshBiasChanged(int val)
{
    threshBias = val;
    lblThreshVal->setText(QString::number(val));
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onCloseRadiusChanged(int val)
{
    closeRadius = val;
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onOpenRadiusChanged(int val)
{
    openRadius = val;
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onMinAreaChanged(int val)
{
    minArea = val;
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onBlurChanged(int val)
{
    blurKSize = val | 1; // ensure odd
    spnBlur->setValue(blurKSize);
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onMethodChanged(int index)
{
    classMethod = index;
    log(QString("Method: %1").arg(cmbMethod->currentText()));
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

void MainWindow::onClassifyToggled(bool on)
{
    classifyOn = on;
    if (sourceMode == IMAGE || sourceMode == DIRECTORY) processAndDisplay();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Misc slots
// ═══════════════════════════════════════════════════════════════════════════
void MainWindow::onSaveScreenshot()
{
    if (lastDisplay.empty()) { log("Nothing to save."); return; }
    std::string fname = "screenshot_" + std::to_string(saveCounter++) + ".png";
    cv::imwrite(fname, lastDisplay);
    log(QString("Screenshot saved: %1").arg(QString::fromStdString(fname)));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════════
QPixmap MainWindow::matToPixmap(const cv::Mat &mat, int maxW, int maxH)
{
    cv::Mat rgb;
    if (mat.channels() == 3)
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    else if (mat.channels() == 1)
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    else
        rgb = mat;

    QImage img(rgb.data, rgb.cols, rgb.rows, (int)rgb.step, QImage::Format_RGB888);
    QPixmap pix = QPixmap::fromImage(img.copy()); // deep copy
    return pix.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void MainWindow::log(const QString &msg)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    txtLog->append(QString("[%1] %2").arg(ts, msg));
}

void MainWindow::loadCNNDB()
{
    cnnDB.clear();
    std::ifstream fin(CNN_DB_PATH);
    if (!fin.is_open()) return;
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string label; std::getline(ss, label, ',');
        CNNEntry entry; entry.label = label;
        float val; char comma;
        while (ss >> val) { entry.embedding.push_back(val); ss >> comma; }
        if (!entry.embedding.empty()) cnnDB.push_back(entry);
    }
}

void MainWindow::saveCNNDB()
{
    std::ofstream fout(CNN_DB_PATH);
    fout << std::fixed << std::setprecision(6);
    for (auto &e : cnnDB) {
        fout << e.label;
        for (float v : e.embedding) fout << "," << v;
        fout << "\n";
    }
}

std::string MainWindow::classifyCNN(const std::vector<float> &emb, double &distance)
{
    distance = 1e30;
    std::string best = "unknown";
    for (auto &e : cnnDB) {
        if (e.embedding.size() != emb.size()) continue;
        double ssd = 0;
        for (size_t i = 0; i < emb.size(); i++) {
            double d = emb[i] - e.embedding[i];
            ssd += d * d;
        }
        if (ssd < distance) { distance = ssd; best = e.label; }
    }
    return best;
}

cv::Mat MainWindow::prepROI(cv::Mat &frame, const RegionFeatures &feat)
{
    cv::Mat embImage;
    prepEmbeddingImage(frame, embImage,
        (int)feat.centroidX, (int)feat.centroidY, (float)feat.theta,
        (float)feat.minE1, (float)feat.maxE1,
        (float)feat.minE2, (float)feat.maxE2, 0);
    return embImage;
}