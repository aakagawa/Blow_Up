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
        void fillCell(ofPixels& pixels, float nthCellX, float nthCellY, float resolutionFactor);
        void zoom(glm::vec2 meanCenter, float zoomFactor);
        void offsetCell(float offsetFactor);
        ofColor calculateAverageColor(ofPixels& pixels, float x, float y, float width, float height);
        void keyPressed(int key);
        int roundSliderValue(int currentValue, int increment);

        // Variables for the transition
        bool isTransitioning = false;
        bool personDetected;
        uint64_t transitionStartTime;
        uint64_t elapsedTime = 0;
        float progress = 0; 
        float transitionDuration = 5000;
        float resolutionFactor = 0.0; // Start with averaged color
        float zoomFactor = 1.0; // Start with no zoom
        float offsetFactor = 0; 
        float targetResolutionFactorDetected = 1.0;
        float targetResolutionFactorUndetected = 0.0;
        float targetZoomFactorDetected = 2.0; 
        float targetZoomFactorUndetected = 1.0;
        float targetResolutionFactor = 1.0; // Transition to full resolution
        float targetOffsetFactorDetected = 0.2;
        float targetOffsetFactorUndetected = 0.2;
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
        ofxFloatSlider baseZoomFactor, bigCellZoomFactor;

        // Processing control
        std::future<void> futureResult; // Future for async frame processing
        int gridRefreshTimer = 5000; // For controlling dynamic updates

        // Grid display
        int gridRows, gridCols;
        float maxGridProbability = 0.2; // Probability for maximum grid size

        float cellWidth, cellHeight;

        int maxSubdivisions = 8; 
        int subdivisions;

        float subCellWidth, subCellHeight; 
        float nthSubCellX, nthSubCellY;
        float subnthCellWidth, susubnthCellHeight;

        int totalR = 0, totalG = 0, totalB = 0;
        int count;
        float grain;
        int subCellR, subCellG, subCellB;

        float offsetX, offsetY;

        // Object tracking parameters
        float movementThreshold = 0.02; // Threshold for detecting movement as a percentage of FOV
        uint64_t inactiveTimeThreshold = 5000; // Time in ms to wait before marking an object as inactive
};
