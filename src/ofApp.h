#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "ofxXmlSettings.h"
#include "ofxTensorFlow2.h"
#include "ofxYolo.h"
#include <future>
#include <vector>

struct TrackedPerson {
    glm::vec2 position;
    float size;
    uint64_t moveTimestamp;

    TrackedPerson(glm::vec2 pos, float area, uint64_t time) : position(pos), size(area), moveTimestamp(time) {}
};

class ofApp : public ofBaseApp {    
public:
    // Lifecycle methods
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;

    void saveSettings();
    void loadSettings();
    void loadDefaultSettings();

    void processFrame(ofPixels pixels);

    void updateGrid();
    void mergeCells();
    void offsetCells();

    void keyPressed(int key) override;

private:
    ofxPanel gui;
    bool showGui = true;
    ofxLabel displaySettings, gridSettings, animationSettings, detectionSettings, advancedSettings; 
    string settingsFile = "settings.xml";

    ofVideoGrabber grabber; 
    ofImage image;
    ofxYolo yolo;
    
    int inputWidth, inputHeight, outputWidth, outputHeight;
    ofxIntField displayWidth, displayHeight;
    float cameraAspectRatio, displayAspectRatio; 
    float baseScale;
    int imageWidth, imageHeight;
    ofxToggle flipImage;

    ofxIntField frameGrain;
    ofxFloatField pixelsResize;

    std::future<void> futureResult; // For asynchronous frame processing

    bool personDetected, lastPersonDetected; 
    bool pendingDetectionState;
    int detectionTimestamp; 
    ofxIntField transInTimeout, transOutTimeout;
    bool isTransitioning;
    int transitionStartTime;
    float progress;
    int rampx;
    float rampy;
    ofxIntField transDuration; 

    glm::vec2 currentFocus;
    glm::vec2 targetFocus; 
    ofxFloatField focusFollowSpeed; 

    ofRectangle cameraFOV;
    float cameraFOVscaleFactor = 0.375;
    ofRectangle detectionArea; 
    ofxIntSlider detectionAreaTopLeftX, detectionAreaTopLeftY, detectionAreaBottomRightX, detectionAreaBottomRightY;
    int lastDetectionAreaTopLeftX, lastDetectionAreaTopLeftY, lastDetectionAreaBottomRightX, lastDetectionAreaBottomRightY;
    int detectionAreaWidth, detectionAreaHeight;

    std::vector<TrackedPerson> trackedPeople;

    uint currentTime;

    ofxFloatField minSize;
    float domSize; 

    glm::vec2 currentPosition;

    ofxFloatField minConfidence; 

    float movement;
    ofxFloatField maxMovementThreshold;
    ofxFloatField minMovementThreshold; 

    int activePersonCount;
    ofxIntField inactiveTimeout; 
    ofxIntField expiryTime;

    static const int MAX_ROWS = 256;
    static const int MAX_COLS = 256;
    int gridRows, gridCols;
    ofxIntField minRow, minCol, maxRow, maxCol;
    ofxIntField gridUpdateInterval, gridUpdateIntervalUncertainty;
    int gridRefreshInterval;
    ofxIntField maxGridProbability; 
    float cellWidth, cellHeight;

    ofxIntField outlineR, outlineG, outlineB;
    ofxIntField cellOutlineWidth;

    glm::vec2 bigCells[MAX_ROWS][MAX_COLS];
    int rowSpan, colSpan;
    int n1x2CellCount, n2x1CellCount, n2x2CellCount;
    ofxIntField max1x2Cell, max2x1Cell, max2x2Cell, bigCellScale;

    ofxIntField zoomFactor;
    int lastZoomFactor;
    float scaleFactor;

    glm::vec2 offsetFactors[MAX_ROWS][MAX_COLS];
    ofxFloatField offsetFactorX, offsetFactorY;
    
    float cellDrawWidth, cellDrawHeight;
    float cellDrawAspectRatio;
    float nthColX, nthRowY;
    float focusWidth, focusHeight;
    float offsetX, offsetY;
    float minNthFocusX, minNthFocusY;
    float maxNthFocusX, maxNthFocusY;
    float nthFocusX, nthFocusY;
};
