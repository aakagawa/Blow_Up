# Blow Up v3.00 
Migration of Blow Up to openFrameworks\
Developed with openFrameworks v0.12.0 

## Features 
* **Real-time Camera Input:** Captures video input from the device's camera. 
* **Dynamic People Detection:** Utilizes YOLOv4 object detection model to instantly identify people entering the camera's field of view.
* **Video Processing:** Applies dynamic processing depending on the presence and positions of people within the view. 
* **High Resolution Rendering:** Displays the processed video in real-time. 

## Build/Installation
### MacOS
1. Clone (or download and extract) the [ofxTensorFlow2 repository](https://github.com/zkmkarlsruhe/ofxTensorFlow2) to the addon folder of openFrameworks. (Replace OF_ROOT with the path to your openFrameworks installation). 
```
cd OF_ROOT/addons
git clone https://github.com/zkmkarlsruhe/ofxTensorFlow2.git
```
#### Dependencies 
* TensorFlow 2
* cppflow 2

2. Pull cppflow to libs/cppflow. 
```
cd ofxTensorFlow2
git submodule update --init --recursive
```

3. Download the pre-built [TensorFlow2 C library](https://www.tensorflow.org/install/lang_c) and extract:\
`include/` to `/usr/loca/include`\
`lib/` to `/usr/local/lib/` 

4. Setup YOLOv4. Copy data from example_yolo_v4. (Replace PROJECT with the path to your OF project folder). 
```
cd ofxTensorFlow2
cp -r example_yolo_v4/bin/data/* PROJECT/bin/data/
```

5. Add ofxGui and ofxTensorFlow2 to Addons in openFrameworks Project Generator and Update.

6. Build and Run App
```
make Release
make RunRelease
```

