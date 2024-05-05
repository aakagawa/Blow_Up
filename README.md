# Blow Up v3.0.2
Blow Up v3.0.2 is an advanced real-time video processing application developed with openFrameworks v0.12.0, that transforms camera input based on the presence and activity of people in its field of view. Originally conceived by Conroy Badger in 2007, the v3.0.2 was developed to adapt to newer platforms.

## Features 
* **Real-time Camera Input:** Captures video input from the device's camera. 
* **Dynamic People Detection:** Utilizes YOLOv4 object detection model to instantly identify people entering the camera's field of view.
* **Video Processing:** Applies dynamic processing depending on the presence and positions of people within the view. 
* **High Resolution Rendering:** Displays the processed video in real-time. 

## Build/Installation
### MacOS
1. **Prepare your environment:**
   - Ensure openFrameworks v0.12.0 is installed.
   - Clone or download the [ofxTensorFlow2 repository](https://github.com/zkmkarlsruhe/ofxTensorFlow2) to your openFrameworks addons folder. (Replace OF_ROOT with the path to your openFrameworks installation).
```
cd OF_ROOT/addons
git clone https://github.com/zkmkarlsruhe/ofxTensorFlow2.git
```
2. **Install Dependencies:**
    - TensorFlow 2 and cppflow 2 are required.
    - Pull cppflow to libs/cppflow. 

```
cd ofxTensorFlow2
git submodule update --init --recursive
```
2. **Install Dependencies:**
    - Download the pre-built [TensorFlow2 C library](https://www.tensorflow.org/install/lang_c) and extract:  
        `include/` to `/usr/local/include`\
        `lib/` to `/usr/local/lib/` 

3. **Configure YOLOv4:**
    - Copy YOLOv4 example data to your project folder. (Replace PROJECT with the path to your openFrameworks project folder).
```
cd ofxTensorFlow2
cp -r example_yolo_v4/bin/data/* PROJECT_ROOT/bin/data/
```

4. **Project Setup:**
    - Use openFrameworks projectGenerator to add `ofxGui`, `ofxXmlSettings`, `ofxTensorFlow2` to your project and update.

5. **Build and Run:**
```
make Release
make RunRelease
```

## App GUI Instructions

- **Press key "g" to show/hide GUI.**
- **Press "shift + l" to load default settings.**
- **Adjusted settings are automatically saved upon app exit and automatically loaded upon app launch.**

### Display Settings
- **Flip Image:** Toggle to flip the image horizontally (`true` or `false`).

### Grid Settings
- **Cell Outline R:** Red value of cell outline (0-255).
- **Cell Outline G:** Green value of cell outline (0-255).
- **Cell Outline B:** Blue value of cell outline (0-255).
- **Cell Outline Width:** Width of the cell outline (0-10 pixels).
- **Minimum Number of Rows:** Minimum number of rows in the grid (2-256).
- **Minimum Number of Columns:** Minimum number of columns in the grid (2-256).
- **Maximum Number of Rows:** Maximum number of rows in the grid (2-256).
- **Maximum Number of Columns:** Maximum number of columns in the grid (2-256).

### Animation Settings
- **Grid Update Interval:** Time between grid updates in milliseconds (5000-600000 ms i.e. 5 seconds-10 minutes).
- **Grid Update Interval Uncertainty:** Percentage uncertainty in the grid update interval (0-100%).  
e.g. Grid Update Interval = 5000 ms, Grid Update Interval Uncertainaty = 50%, means 5000 ± 2500.
- **Probability of Maximum Grid:** Probability of the grid being at maximum dimensions (0-100%).
- **Max Number of 1x2 Cells:** Maximum number of 1x2 cells allowed in the grid (0-10).
- **Max Number of 2x1 Cells:** Maximum number of 2x1 cells allowed in the grid (0-10).
- **Max Number of 2x2 Cells:** Maximum number of 2x2 cells allowed in the grid (0-10).
- **Big Cell Scale:** Scale factor for 1x2, 2x1, and 2x2 cells (1-10).
- **Zoom/Scale Factor:** Zoom or scale factor for each cell after blow up (2-20).
- **Horizontal Offset Factor:** Maximum random horizontal offset after blow up as a fraction of image width (0.0-1.0).
- **Vertical Offset Factor:** Maximum random vertical offset after blow up as a fraction of image height (0.0-1.0).
- **Transition Duration:** Duration of transitions from normal to blow up (500-10000 ms).
- **Focus Follow Speed:** Speed at which the focus follows the detected person (0.01-0.5).

### Detection Settings
- **Detection Area Top Left X:** X-coordinate for the top left corner of the detection area (0 to input width).
- **Detection Area Top Left Y:** Y-coordinate for the top left corner of the detection area (0 to input height).
- **Detection Area Bottom Right X:** X-coordinate for the bottom right corner of the detection area; defaults to maximum width (0 to input width).
- **Detection Area Bottom Right Y:** Y-coordinate for the bottom right corner of the detection area; defaults to maximum height (0 to input height).
- **Required Confidence:** Minimum confidence level required to detect a person (0.0-0.9).
- **Required Size:** Minimum size required for a person to be considered detected as a fraction of the total detection area (0.0-0.9).
- **Transition In Timeout:** Timeout before transition after detection, in milliseconds (0-10000 ms).
- **Transition Out Timeout:** Timeout before transition after undetection, in milliseconds (0-10000 ms).
- **Time Until Inactive Person is Considered Undetected:** Time until an inactive person is considered undetected, in milliseconds (2000-60000 ms).

### Advanced Settings 
- **Detection frameGrain:** Frame grain setting for detection (1-6).
- **Detection pixels.resize:** Pixels resize settings for detection (0.2-1.0).
- **maxMovementThreshold:** Maximum movement threshold to be considered the same person (0.5-1.0).
- **minMovementThreshold:** Minimum movement threshold to be considered active. (0.01-0.2).
- **expiryTime:** Time until undetected people are removed from memory (30000-120000 ms).
