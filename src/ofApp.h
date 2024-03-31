#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofImage.h"
#include "ofxTensorFlow2.h"
#include "ofxYolo.h"
#include <future>
#include <vector>
#include "glm/vec2.hpp" // Make sure glm/vec2.hpp is included for glm::vec2

struct TrackedPerson {
    glm::vec2 position; // Current position of the person
    uint64_t lastMoveTime; // Last time the person moved
    bool isActive; // Is the person active?

    TrackedPerson(glm::vec2 pos, uint64_t time)
        : position(pos), lastMoveTime(time), isActive(true) {}
};

class ofApp : public ofBaseApp {
public:
    void setup();
    void setupWebcam();
    void update();
    void draw();
    void processFrame(ofPixels pixels);
    void updateGrid();
    void fillCell(ofPixels& pixels, int col, int row, float resolutionFactor);
    ofColor calculateAverageColor(ofPixels& pixels, float startX, float startY, float endX, float endY);
    void keyPressed(int key);
    int roundSliderValue(int currentValue, int increment);

    // Variables for the transition
    bool isTransitioning = false;
    uint64_t transitionStartTime = 0;
    float currentZoomFactor = 1.0; // Start with no zoom
    float targetZoomFactor = 2.0; // Zoom in factor
    float resolutionFactor = 0.0; // Start with averaged color
    float targetResolutionFactor = 1.0; // Transition to full resolution
    glm::vec2 meanCenter; // Center for zooming based on detection

private:
    // Video capture
    ofVideoGrabber webcam;
    
    // YOLO object detection
    ofxYolo yolo;

    // Vector to track people 
    std::vector<TrackedPerson> trackedPeople;

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
    float movementThreshold = 0.02; // Threshold for detecting movement as a percentage of FOV
    uint64_t inactiveTimeThreshold = 5000; // Time in ms to wait before marking an object as inactive

    // State control
    bool isObjectDetected = false;
    int mode = 1; // Application mode (1: normal, 2: zoom and offset)
};
