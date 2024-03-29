#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofImage.h"
#include "ofxTensorFlow2.h"
#include "ofxYolo.h"
#include <future>
#include <vector>

// Struct to track objects detected by YOLO
struct TrackedObject {
    ofxYolo::Object yoloObject;
    glm::vec2 lastPosition;
    uint64_t lastSeen;
    bool isActive;

    TrackedObject(const ofxYolo::Object& obj, const glm::vec2& pos, uint64_t time)
        : yoloObject(obj), lastPosition(pos), lastSeen(time), isActive(true) {}
};

class ofApp : public ofBaseApp {
public:
    // Setup functions
    void setup();
    void setupWebcam();

    // Update & Draw functions
    void update();
    void draw();

    // Input handling
    void keyPressed(int key);

    // Custom functionality
    void processFrame(ofPixels pixels);
    void updateGrid();
    void drawGrid();
    void toggleModes();
    void renderMode1();
    void renderMode2();
    int roundSliderValue(int currentValue, int increment);

    // For morphing 
    uint64_t transitionStartTime;
    float currentZoomFactor;
    float targetZoomFactor;
    bool isTransitioning = false;

private:
    // Video capture
    ofVideoGrabber webcam;
    
    // YOLO object detection
    ofxYolo yolo;
    std::vector<TrackedObject> trackedObjects;

    // GUI components
    ofxPanel gui;
    bool showGui = true;
    ofxIntSlider webcamWidth, webcamHeight;
    ofxIntSlider timerMedian, timerUncertainty;
    ofxIntSlider morphInDuration, morphOutDuration;
    ofxIntSlider untilMorphIn, untilMorphOut;
    ofxFloatSlider mode2MaxOffsetFactor;
    ofxFloatSlider baseZoomFactor, bigCellZoomFactor;

    // Processing control
    std::future<void> futureResult; // Future for async frame processing
    int gridRefreshTimer; // For controlling dynamic updates

    // Grid display
    int gridRows, gridCols;
    float maxGridProbability = 0.2; // Probability for maximum grid size

    float cellWidth, cellHeight;

    // Object tracking parameters
    float activityThreshold = 0.02; // Threshold for detecting movement as a percentage of FOV
    uint64_t inactiveTimeThreshold = 5000; // Time in ms to wait before marking an object as inactive

    // State control
    bool isObjectDetected = false;
    int mode = 1; // Application mode (1: normal, 2: zoom and offset)
};
