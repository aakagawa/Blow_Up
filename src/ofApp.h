#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofImage.h"
#include "ofxTensorFlow2.h"
#include "ofxYolo.h"
#include <future>
#include <vector>
#include "glm/vec2.hpp"

struct TrackedPerson {
    glm::vec2 position;
    uint64_t lastMoveTime;
    bool isActive;

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
    void fillCell(ofPixels& pixels, float nthCellX, float nthCellY);
    void zoom(glm::vec2 meanCenter, float zoomFactor);
    void offsetCell(float offsetFactor);
    ofColor calculateAverageColor(ofPixels& pixels, float x, float y, float width, float height);
    void keyPressed(int key);
    int roundSliderValue(int currentValue, int increment);

    // Detection and Transition Control
    bool isTransitioning = false;
    uint64_t currentTime; 
    bool personDetected = false;
    bool lastPersonDetected = false; // Added to track the last state of personDetected
    bool foundActivePerson = false;
    int activePersonCount;
    glm::vec2 sumPositions;
    uint64_t transitionStartTime;
    uint64_t elapsedTime; 
    float progress = 0.0;
    float transitionDuration = 2000;

    // Transition Targets
    float resolutionFactor = 0.0;
    float zoomFactor = 1.0;
    float offsetFactor = 0.0;
    glm::vec2 meanCenter;
    float targetResolutionFactorDetected = 1.0;
    float targetResolutionFactorUndetected = 0.0;
    float targetZoomFactorDetected = 2.0;
    float targetZoomFactorUndetected = 1.0;
    float targetOffsetFactorDetected = 0.2;
    float targetOffsetFactorUndetected = 0.2;

private:
    // Video and Object Detection
    ofVideoGrabber webcam;
    ofxYolo yolo;
    std::vector<TrackedPerson> trackedPeople;

    // GUI Components
    ofxPanel gui;
    bool showGui = true;
    ofxIntSlider webcamWidth, webcamHeight;
    ofxIntSlider timerMedian, timerUncertainty;
    ofxIntSlider morphInDuration, morphOutDuration;
    ofxIntSlider untilMorphIn, untilMorphOut;

    // Grid Display Control
    std::future<void> futureResult; // For async frame processing
    int gridRefreshTimer = 5000; // Dynamic update control
    int gridRows, gridCols;
    float maxGridProbability = 0.2;
    float cellWidth, cellHeight;
    float nthCellX, nthCellY;
    int maxSubdivisions = 16;
    int subdivisions;
    float subCellWidth, subCellHeight;
    float nthSubCellX, nthSubCellY;
    float nthSubCellWidth, nthSubCellHeight;

    int count;
    int totalR = 0, totalG = 0, totalB = 0;
    int subCellR, subCellG, subCellB;
    float grain;
    
    
    // Additional Rendering Parameters
    float offsetX, offsetY;
    float maxMovementThreshold = 0.1; // To be considered the same person
    float minMovementThreshold = 0.02; // To be considered active
    uint64_t inactiveTimeThreshold = 10000; // For marking inactivity
};
