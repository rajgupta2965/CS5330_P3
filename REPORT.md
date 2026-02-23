# Project 3 Report: Real-time 2-D Object Recognition

**Note on Format:** This report is written in Markdown and includes local image references. To generate the final PDF for submission, please use a Markdown to PDF converter (like Pandoc or the one in your IDE) that can resolve local image paths.

## Project Description

This project implements a real-time 2-D object recognition system in C++ using OpenCV. The system identifies objects placed on a white surface from a top-down camera view, in a manner that is invariant to translation, scale, and rotation. It works by processing each frame through a multi-stage pipeline: thresholding to separate the object from the background, morphological filtering to clean up the binary image, connected components analysis to identify distinct regions, and feature extraction to compute shape descriptors for each region.

All four of these core pipeline stages were written from scratch without relying on any OpenCV built-in functions for thresholding, morphology, connected components, or moment computation. The system supports three classification methods: hand-crafted features based on Hu moment invariants, eigenspace (PCA) embeddings, and CNN (ResNet18) embeddings. Classification uses a nearest-neighbour approach where new objects are compared against a stored training database.

The system supports webcam input for real-time operation, as well as single image, directory, and video file modes for offline processing and evaluation.


## Task 1: Thresholding

The thresholding algorithm was implemented entirely from scratch. The approach converts each frame to HSV and computes a per-pixel "object score" using the formula `score = (255 - V) + S`, where V is the value channel and S is the saturation channel. This makes both dark objects (low V) and colorful objects (high S) score highly, while the white background scores low.

The threshold value is determined automatically using the ISODATA algorithm, which is essentially K-means with K=2 run on a subsampled set of score values. The algorithm iteratively splits the pixel scores into two clusters (background and foreground) and converges on a threshold at the midpoint between the two cluster means. A manual bias of +20 was added to the automatic threshold to ensure thinner and lighter objects like the wrench and chisel handle are fully captured.

The following images show the thresholded result for three of the test objects. The foreground appears white and the background appears black.

<img src="screenshots/thresh_triangle.png" width="300"> <img src="screenshots/thresh_wrench.png" width="300"> <img src="screenshots/thresh_keyfob.png" width="300">


## Task 2: Morphological Filtering

The morphological filtering was also written from scratch using a circular (disk) structuring element. The cleanup strategy uses a two-step approach: first closing (dilate then erode) with a radius of 4 pixels to fill small holes inside detected objects, followed by opening (erode then dilate) with a radius of 3 pixels to remove small isolated noise blobs from the background.

This grow/shrink approach was chosen because the thresholded images tend to have small holes inside objects (especially the triangle's cutout edges and the chisel's reflective metal surface) as well as small spurious blobs from shadows and surface texture on the cardboard frame. Closing fills the holes while opening removes the noise, and performing them in this order ensures that holes are sealed before noise removal potentially fragments the regions.

The following images show the cleaned binary result for the same three objects. Compare these with the thresholded images above to see how the morphological operations have smoothed the regions.

<img src="screenshots/clean_triangle.png" width="300"> <img src="screenshots/clean_wrench.png" width="300"> <img src="screenshots/clean_keyfob.png" width="300">


## Task 3: Connected Components / Segmentation

Connected components analysis was implemented from scratch using the classic two-pass algorithm with union-find and 8-connectivity. In the first pass, the algorithm scans left-to-right, top-to-bottom, assigning labels to foreground pixels based on their already-visited neighbours (up-left, up, up-right, left). When a pixel has multiple differently-labelled neighbours, the labels are recorded as equivalent using a union-find data structure with path compression. In the second pass, all equivalences are resolved and the label map is renumbered sequentially.

After labelling, the system filters regions to keep only the actual objects. Regions smaller than 800 pixels are discarded as noise. Any region with pixels within 8 pixels of the image border is removed, which effectively eliminates the cardboard frame and desk edges that sometimes get thresholded. Regions larger than 40% of the total image area are also discarded as background clutter. The remaining regions are sorted by centrality (distance from image center) and at most 5 are retained, prioritizing the most centrally placed objects.

The following images show the colour-coded region maps where each detected region is displayed in a distinct colour. Background is black.

<img src="screenshots/region_triangle.png" width="300"> <img src="screenshots/region_wrench.png" width="300"> <img src="screenshots/region_keyfob.png" width="300">


## Task 4: Feature Computation

Feature computation was implemented entirely from scratch. For each detected region, the system computes raw moments (m00, m10, m01, m20, m11, m02 through third order), then derives the centroid, central moments, and normalised central moments from these. The orientation angle is computed as `theta = 0.5 * atan2(2 * mu11, mu20 - mu02)` from the 2x2 inertia tensor of the region.

The oriented bounding box is constructed by projecting all region pixels onto the primary and secondary axes (the eigenvectors of the inertia tensor) and recording the min/max extents along each. Two shape descriptors are computed from the oriented bounding box: percent filled (region area divided by oriented BB area) and aspect ratio (longer side divided by shorter side, always >= 1).

Hu's seven moment invariants are computed from the normalised central moments, providing features that are invariant to translation, scale, and rotation. These are then log-transformed using `logHu[i] = -sign(hu[i]) * log10(|hu[i]|)` to compress their range for use in the feature vector.

The full 9-dimensional feature vector used for classification consists of: percent filled, aspect ratio, and the seven log-transformed Hu moments.

The following images show the oriented bounding box (green) and primary axis (red) overlaid on each object, along with the classification label.

<img src="screenshots/feat_triangle.png" width="300"> <img src="screenshots/feat_roller.png" width="300"> <img src="screenshots/feat_wrench.png" width="300"> <img src="screenshots/feat_chisel.png" width="300"> <img src="screenshots/feat_keyfob.png" width="300">

**Feature vectors for each object:**

| Object | Filled | AR | logHu0 | logHu1 | logHu2 | logHu3 | logHu4 | logHu5 | logHu6 |
|--------|--------|----|--------|--------|--------|--------|--------|--------|--------|
| triangle | 0.466 | 1.931 | 0.58 | 1.69 | 2.11 | 3.44 | -6.35 | -4.38 | 6.38 |
| roller | 0.156 | 1.693 | 0.06 | 0.61 | -0.11 | 0.21 | 0.26 | 0.52 | -2.95 |
| wrench | 0.226 | 3.883 | -0.22 | -0.39 | 0.18 | 0.56 | 0.96 | 0.46 | -1.38 |
| chisel | 0.565 | 4.338 | 0.33 | 0.74 | 1.51 | 1.65 | 3.22 | 2.02 | -5.01 |
| keyfob | 0.784 | 1.548 | 0.76 | 2.34 | 4.88 | 6.08 | 11.57 | 7.26 | -12.27 |

The features show clear differentiation between objects. The keyfob has the highest percent filled (0.784) since it is a solid oval shape, while the roller has the lowest (0.156) due to its thin handle. The chisel and wrench have high aspect ratios (4.3 and 3.9) reflecting their elongated shapes, while the keyfob is the most compact (1.5). The Hu moments provide additional discrimination, with the keyfob showing the most extreme values due to its smooth elliptical shape.


## Task 5: Training System

The training system stores feature vectors alongside their labels in a simple CSV file (`object_db.csv`). When the user presses `4` in the application, the system extracts the feature vector of the currently detected object and prompts for a label via the terminal. The label and 9-dimensional feature vector are then appended to the CSV file.

Each line in the CSV takes the format: `label,f0,f1,...,f8`. The database persists across sessions, so previously trained objects remain available when the program is restarted. The user can add multiple training examples per object to improve classification robustness.

For this evaluation, one training example per object was collected from the provided development images, giving a database of 5 entries (triangle, roller, wrench, chisel, keyfob).


## Task 6: Classification

Classification uses a scaled Euclidean distance nearest-neighbour approach. For each feature dimension, the standard deviation across all training entries is computed. The distance between a new feature vector and a training entry is then calculated as:

`distance = sqrt( sum( ((x_i - y_i) / stdev_i)^2 ) )`

This scaling ensures that features with larger natural ranges do not dominate the distance computation. The unknown object is assigned the label of the nearest training entry. If the distance exceeds a configurable threshold (set to 6.0), the object is labelled as "unknown" to handle objects not in the database.

The following images show the classification results for all five objects using hand-crafted features.

<img src="screenshot_1.png" width="300"> <img src="screenshot_2.png" width="300"> <img src="screenshot_3.png" width="300"> <img src="screenshot_4.png" width="300"> <img src="screenshot_5.png" width="300">

All five objects are correctly classified with their training labels: triangle, roller, wrench, chisel, and keyfob.


## Task 7: Performance Evaluation

The confusion matrix was generated by cycling through each test image and recording the predicted label against the known true label. With the provided development images, the hand-crafted feature classifier achieved perfect accuracy.

```
=== Confusion Matrix (rows=true, cols=predicted) ===
                     chisel      keyfob      roller    triangle      wrench
         chisel           1           0           0           0           0
         keyfob           0           1           0           0           0
         roller           0           0           1           0           0
       triangle           0           0           0           1           0
         wrench           0           0           0           0           1
Accuracy: 5/5 = 100.0%
```

The 100% accuracy on these test images reflects the fact that each test image was taken under the same lighting and camera setup as the training image. In practice, accuracy would likely decrease with changes in illumination, different camera angles, or partial occlusion. The scaled Euclidean distance metric helps handle minor variations by normalising each feature dimension, but more training samples from varied conditions would be needed for robust real-world performance.


## Task 9: One-Shot Classification with Embeddings

### Preprocessing

Both embedding methods share the same preprocessing pipeline. Given a detected region with its centroid, orientation angle, and extent along the primary and secondary axes, the original image is rotated so the region's primary axis aligns with the X-axis. A region of interest (ROI) is then extracted corresponding to the oriented bounding box. This ROI is resized to the appropriate input size for each embedding method (64x64 for eigenspace, 224x224 for CNN).

The following images show the extracted ROIs used for embedding. Note that the wrench and chisel produce very thin ROIs because only their dark metal portions are detected by the thresholding, making the oriented bounding box very narrow.

<img src="roi_training/triangle_0.png" width="200"> <img src="roi_training/roller_1.png" width="200"> <img src="roi_training/wrench_2.png" width="200"> <img src="roi_training/chisel_4.png" width="200"> <img src="roi_training/keyfob_5.png" width="200">

### Eigenspace (PCA) Embedding

The eigenspace was built by collecting preprocessed ROI images for each object, converting them to greyscale, resizing to 64x64, flattening each into a vector, computing the mean image, subtracting it, and performing SVD on the resulting difference matrix. The top 6 eigenvectors (equal to the number of training images) were retained.

To classify a new image, it is preprocessed the same way, the mean is subtracted, and the result is projected onto the eigenvectors to produce a 6-dimensional embedding. Classification uses sum-of-squared-differences against the stored training embeddings.

**Eigenspace results:**

| Object | Predicted | Correct? |
|--------|-----------|----------|
| triangle | wrench | No |
| roller | keyfob | No |
| wrench | wrench | Yes |
| chisel | triangle | No |
| keyfob | chisel | No |

**Eigenspace accuracy: 1/5 = 20%**

The eigenspace performed poorly because with only 5 to 6 training images, the PCA space is too sparse to build meaningful eigenvectors. Additionally, the thin ROIs for the wrench and chisel look nearly identical when resized to 64x64, making them hard to distinguish. The ROI quality directly impacts eigenspace performance since it operates on raw pixel values.

<img src="screenshot_6.png" width="300"> <img src="screenshot_7.png" width="300"> <img src="screenshot_8.png" width="300"> <img src="screenshot_9.png" width="300"> <img src="screenshot_10.png" width="300">


### CNN (ResNet18) Embedding

The CNN embedding uses a pre-trained ResNet18 model loaded from an ONNX file. The preprocessed ROI is resized to 224x224, normalised, and passed through the network. The 512-dimensional feature vector from the penultimate layer serves as the embedding. Classification again uses sum-of-squared-differences against stored training embeddings.

**CNN results:**

| Object | Predicted | Correct? |
|--------|-----------|----------|
| triangle | triangle | Yes |
| roller | roller | Yes |
| wrench | wrench | Yes |
| chisel | chisel | Yes |
| keyfob | keyfob | Yes |

**CNN accuracy: 5/5 = 100%**

The CNN performed perfectly even with one-shot training (a single example per category). This is because ResNet18 was pre-trained on ImageNet with millions of images, so its 512-dimensional feature space is already highly discriminative. Even with the thin, partial ROIs for the wrench and chisel, the network can extract enough texture and shape information to distinguish between objects.

<img src="screenshot_11.png" width="300"> <img src="screenshot_12.png" width="300"> <img src="screenshot_13.png" width="300"> <img src="screenshot_14.png" width="300"> <img src="screenshot_15.png" width="300">


### Comparison of All Three Methods

| Method | Features | Dimensions | Accuracy | Notes |
|--------|----------|------------|----------|-------|
| Hand-crafted | Hu moments, fill ratio, aspect ratio | 9 | 5/5 = 100% | Uses full region shape, not ROI image |
| Eigenspace (PCA) | Pixel-level PCA projection | 6 | 1/5 = 20% | Needs many more training images |
| CNN (ResNet18) | Pre-trained deep features | 512 | 5/5 = 100% | Works well even one-shot |

The hand-crafted features and CNN both achieved perfect accuracy, but for different reasons. The hand-crafted features work because they use the full region's shape properties (moments, fill ratio, aspect ratio) computed over all detected pixels, so they are not affected by the quality of the extracted ROI. The CNN works because its pre-trained features are inherently powerful and can discriminate even from small or partial image crops.

The eigenspace struggled because PCA is fundamentally a data-driven method that needs a sufficient number of training images to build a meaningful low-dimensional representation. With only one image per class, the eigenspace essentially memorises the training images rather than learning discriminative features. Additionally, the quality of the ROI extraction matters much more for pixel-based methods. The thin slivers extracted for the wrench and chisel are almost indistinguishable at 64x64 resolution, leading to frequent misclassification.


# Extensions

## Qt Desktop GUI

A full Qt desktop GUI was developed as an extension. The GUI provides a professional dark-themed interface with four tiled panels showing each pipeline stage simultaneously (original with overlays, thresholded, morphologically cleaned, and colour-coded region map).

The sidebar includes grouped controls for source selection (webcam, image, directory, video with navigation buttons), classification method selection via dropdown (Features, Eigenspace, CNN), and training operations via dialog buttons. All pipeline parameters including threshold bias, morphology radii, minimum region area, and blur kernel size can be adjusted in real-time using sliders and spinboxes, with changes immediately reflected in the display. A timestamped log panel at the bottom tracks all actions and system messages.

## All Four Pipeline Stages From Scratch

All four core stages of the pipeline were implemented from scratch without using any OpenCV built-in functions for these operations. Specifically: ISODATA adaptive thresholding with custom HSV-based scoring, erosion and dilation with circular structuring elements, two-pass connected components with union-find and 8-connectivity, and full moment computation including raw, central, normalised central moments, orientation, oriented bounding box, and all seven Hu moment invariants. OpenCV was used only for basic utilities like image I/O, colour space conversion, and Gaussian blur.

## Both Embedding Methods

Both the eigenspace (PCA) and CNN (ResNet18) embedding methods were implemented for one-shot classification as described in Task 9, with comparative evaluation showing the strengths and weaknesses of each approach.


## Time Travel Days
0 days used.


## Reflection

This project was a thorough exercise in building a complete computer vision pipeline from the ground up. Implementing each stage from scratch, rather than calling a single OpenCV function, forced a deep understanding of what these algorithms actually do and why they work. For instance, writing the connected components algorithm made it clear why the two-pass approach with union-find is efficient, and implementing Hu moments revealed how higher-order moment invariants achieve rotation independence through careful combinations of normalised central moments.

The comparison between the three classification methods was the most interesting part. It was surprising that simple hand-crafted features (just 9 numbers) matched the CNN's accuracy on this dataset, while the eigenspace with its pixel-level analysis performed so poorly. This reinforced the idea that choosing the right features matters more than having more features, and that data-driven methods like PCA need sufficient training data to be useful.

Working with the thresholding also highlighted how much the rest of the pipeline depends on getting that first step right. Objects like the chisel, which has both a dark metal blade and a light wooden handle, required careful threshold tuning to capture the full shape. When only part of the object was detected, the features changed dramatically, leading to misclassification. This kind of cascading error is something that is easy to overlook when using pre-built library functions.


## Acknowledgements

*   **Professor Bruce Maxwell** and the course materials for CS5330, which provided the project specification, utilities.cpp, embedding.py, and the ResNet18 ONNX model.
*   **OpenCV Documentation:** For references on image I/O, colour space conversion, Gaussian blur, and the DNN module.
*   **An AI assistant (Claude):** Was used to help write and debug code, implement the Qt GUI, and for project documentation.