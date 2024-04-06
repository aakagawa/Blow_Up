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
		void setup() override;
		void setupWebcam();
		void update() override;
		void draw() override;
		
		void processFrame(ofPixels pixels);
		void updateGrid();

		void drawGrid();
		
		
	private: 
		// Adjustable
		ofxPanel gui;
		bool showGui = true;
		ofxIntSlider webcamWidth, webcamHeight;
		ofxIntSlider intervalMedian, intervalUncertainty; 

		ofVideoGrabber grabber; 
		ofImage image; 
		ofxYolo yolo;

		// setup
		float baseScale; 
		int imageWidth; 
		int imageHeight;

		// update 
		bool lastPersonDetected = false;
		bool isTransitioning = false;
		uint64_t transitionStartTime;
		uint64_t elapsedTime; 
		float progress = 0.0;
		float transitionDuration = 1000;
		float nthCellCenterX, nthCellCenterY;
		static constexpr int MAX_ROWS = 256; // Maximum grid rows
		static constexpr int MAX_COLS = 256; // Maximum grid columns
		glm::vec2 cellFocus[MAX_ROWS][MAX_COLS]; // 2D array for cell focus points
		float awef = 0;

		// draw
		float nthColX, nthRowY; 
		float focusWidth, focusHeight;
		float minNthFocusX, minNthFocusY;
		float maxNthFocusX, maxNthFocusY; 
		float nthFocusX, nthFocusY;

		// updateGrid
		int gridRefreshInterval;
		float maxGridProbability = 0.2;
		int gridRows, gridCols;
		float cellWidth, cellHeight;
		float cellAspectRatio;

		// processFrame
		std::future<void> futureResult; // For async frame processing
		uint64_t currentTime; 
		bool personDetected; 
		int activePersonCount; 
		glm::vec2 sumPositions;
		glm::vec2 position;
		std::vector<TrackedPerson> trackedPeople;
		float movement;
		float maxMovementThreshold = 0.1; // Make adjustable
		float minMovementThreshold = 0.05; // Make adjustable
		uint64_t inactiveTimeThreshold = 10000;
		glm::vec2 meanCenter;
		float detectedCenterX, detectedCenterY;
};
