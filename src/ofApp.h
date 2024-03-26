#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofImage.h" // Ensure this is included for ofImage usage
#include "ofxTensorFlow2.h"
#include "ofxYolo.h"
#include <future>

class ofApp : public ofBaseApp{
public:
    void setup();
    void update();
    void draw();
    void keyPressed(int key);

    ofVideoGrabber webcam;
    
    ofxYolo yolo;

    // GUI components
    ofxPanel gui;
    bool showGui;
    ofxButton setupWebcamButton;
    ofxIntSlider webcamWidth, webcamHeight;
    ofxIntSlider timerMedian, timerUncertainty;
    ofxIntSlider morphInDuration, morphOutDuration;
    ofxIntSlider untilMorphIn, untilMorphOut;
    ofxFloatSlider mode2OffsetRange;
    ofxFloatSlider detectionZoomFactor, bigCellZoomFactor;
    
    void setupWebcam();
    void processFrame(ofPixels pixels); // Function to process the frame in a separate thread. Updated to take ofPixels by value for thread safety
    std::future<void> futureResult; // Future to hold the result of the asynchronous call

    ofImage tempImage; // Reusable image for resizing frames
    
    int roundSliderValue(int currentValue, int increment);

    int changeTime;

    int gridRows; 
    int gridCols; 

    float maxGridProbability = 0.2; // 20%

    void updateGridDimensions(); 

    void displayGrid();
    ofColor getAverageColor(ofPixels & pixels, int x, int y, int width, int height);

    bool isObjectDetected;
    int mode; // 1 for normal mode, 2 for zoom and offset mode

};

