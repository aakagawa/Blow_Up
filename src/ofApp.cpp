#include "ofApp.h"

void ofApp::setup() {
    showGui = true;
    gui.setup();
    gui.setSize(540, 540);
    gui.setDefaultWidth(500); // Slightly less than panel width to fit nicely
    gui.setDefaultHeight(45); // Taller elements for better interaction
    gui.add(webcamWidth.setup("Width", 1920, 1920, 3840));
    gui.add(webcamHeight.setup("Height", 1080, 1080, 2160));
    gui.add(intervalMedian.setup("Grid refresh interval", 5000, 2000, 10000));
    gui.add(intervalUncertainty.setup("Gride refresh error", 500, 0, 1000));

    ofSetFrameRate(30); // Set rendering framerate to 60fps
    ofSetFullscreen(true);

    // Scale camera to display 
    grabber.setup(3840, 2160); // 4k if available
    grabber.setDesiredFrameRate(30); // Set webcam framerate to 30fps
    grabber.update();
    int inputWidth = grabber.getWidth();
    int inputHeight = grabber.getHeight();
    ofLogNotice() << "inputWidth: " << inputWidth;
    ofLogNotice() << "inputHeight: " << inputHeight;
    int outputWidth = ofGetWidth();
    int outputHeight = ofGetHeight(); 
    ofLogNotice() << "outputWidth: " << outputWidth;
    ofLogNotice() << "outputHeight: " << outputHeight;

    float cameraAspectRatio = static_cast<float>(inputWidth) / static_cast<float>(inputHeight); // e.g. 1280, 720 = cameraAspectRatio = 1.7777
    float displayAspectRatio = static_cast<float>(outputWidth) / static_cast<float>(outputHeight); // e.g. 1792, 1120  displayAspectRatio = 1.6 

    if (cameraAspectRatio > displayAspectRatio) { // Camera is wider than display
        baseScale = outputHeight / static_cast<float>(inputHeight);
    } else { // Camera is taller than display
        baseScale = outputWidth / static_cast<float>(inputWidth);
    }

    // Calculate new dimensions
    imageWidth = inputWidth * baseScale;
    imageHeight = inputHeight * baseScale;
    ofLogNotice() << "imageWidth: " << imageWidth;
    ofLogNotice() << "imageHeight: " << imageHeight;

    // Initialize YOLO model 
    if (!yolo.setup("model", "classes.txt")) { 
        ofLogError() << "Failed to setup YOLO model!";
        std::exit(EXIT_FAILURE);
    }
    yolo.setNormalize(true); // Normalize object bounding box coordinates

    // Initialize gridRefreshInterval
    gridRefreshInterval = ofGetElapsedTimeMillis() + intervalMedian + ofRandom(-intervalUncertainty, intervalUncertainty);

    updateGrid();
    ofLogNotice() << "initial gridRows: " << gridRows;
    ofLogNotice() << "initial gridCols: " << gridCols;
    ofLogNotice() << "initial cellWidth: " << cellWidth;
    ofLogNotice() << "initial cellHeight: " << cellHeight;
}

void ofApp::update() {
    grabber.update();
    static int frameCount = 0;
    int frameGrain = 4;

    if (grabber.isFrameNew()) {
        ofPixels pixels = grabber.getPixels();
        pixels.resize(imageWidth, imageHeight);
        image.setFromPixels(pixels);
        if (++frameCount % frameGrain == 0) { // Drop framerate for processFrame
            frameCount = 0; // Reset frame counter to avoid overflow
                if (futureResult.valid() ? futureResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready : true) {
                futureResult = std::async(std::launch::async, &ofApp::processFrame, this, grabber.getPixels());
            }
        }
    }

    // Update grid every gridRefreshInterval ± intervalUncertainty
    if (ofGetElapsedTimeMillis() > gridRefreshInterval) {
        updateGrid();
        gridRefreshInterval = ofGetElapsedTimeMillis() + intervalMedian + ofRandom(-intervalUncertainty, intervalUncertainty);
    }

    if (personDetected != lastPersonDetected && !isTransitioning) {
        isTransitioning = true;
        transitionStartTime = ofGetElapsedTimeMillis();
        // Reset progress to start a new transition if needed
        progress = 0.0;
    }

    if (isTransitioning) {
        elapsedTime = ofGetElapsedTimeMillis() - transitionStartTime;
        progress = elapsedTime / static_cast<float>(transitionDuration);
        progress = ofClamp(progress, 0.0, 1.0);
        if (personDetected) {
            ofLogNotice() << "person detected ramp ";
            awef = ofLerp(0, 1, progress);
            numMerges = ofRandom(11);
        } else {
            ofLogNotice() << "person undetected ramp";
            awef = ofLerp(1, 0, progress);
            numMerges = 0;
        }
        ofLogNotice() << "progress: " << progress;
        if (progress >= 1.0) {
            isTransitioning = false;
            lastPersonDetected = personDetected; // Update the last state to the current at the end of transition
        }
    } else if (personDetected != lastPersonDetected) { // Ensure to update the lastPersonDetected variable outside and after the if(isTransitioning) block if the transition doesn't start
        lastPersonDetected = personDetected;
    }
}

// Asynchronous  
void ofApp::processFrame(ofPixels pixels) {
    pixels.resize(960, 540); // Adjust resizing directly on ofPixels
    yolo.setInput(pixels);
    yolo.update();

    // Retrieve and log detected objects
    auto objects = yolo.getObjects();
    currentTime = ofGetElapsedTimeMillis();
    personDetected = false;
    activePersonCount = 0;
    sumPositions = glm::vec2(0, 0);

    for (auto& object : objects) {
        if (object.ident.text == "person") { // Focus on persons only
            position = glm::vec2(object.bbox.x + object.bbox.width / 2, object.bbox.y + object.bbox.height / 2);
            bool found = false;

            // Check if same person 
            for (auto& trackedPerson : trackedPeople) {
                movement = glm::distance(position, trackedPerson.position);
                ofLogNotice() << "movement" << movement; 
                if (movement < maxMovementThreshold) { // If detected person has not moved more than maxMovementThreshold
                    if (movement > minMovementThreshold) { // If detected person who has NOT moved than maxMovementThreshold, has moved more than minMovementThreshol, Rrenew timestamp
                        trackedPerson.lastMoveTime = currentTime;
                        ofLogNotice() << "movement detected, updating time.";
                    }
                    found = true;
                    ofLogNotice() << "same person found"; 
                    break;
                }
            }
            
            if (!found) { 
                trackedPeople.emplace_back(TrackedPerson{position, currentTime});
                ofLogNotice() << "new person found"; 
            }
        } 
    }

    // Check for inactive tracked people
    for (auto& trackedPerson : trackedPeople) {
        if ((currentTime - trackedPerson.lastMoveTime) > inactiveTimeThreshold) {
            trackedPerson.isActive = false;
            ofLogNotice() << "inactive person found"; 
        } else {
            trackedPerson.isActive = true;
            ofLogNotice() << "active person found"; 
        }
        if (trackedPerson.isActive) {
            sumPositions += trackedPerson.position;
            activePersonCount++;
            personDetected = true; 
        } 
    }
    ofLogNotice() << "meanCenter x: " << meanCenter.x;
    ofLogNotice() << "meanCenter y: " << meanCenter.y;
    meanCenter = (activePersonCount > 0) ? sumPositions / static_cast<float>(activePersonCount) : glm::vec2(0.5, 0.5);
}

void ofApp::updateGrid() {
    float prob = ofRandom(1.0);
    if (prob < maxGridProbability) {
        gridRows = gridCols = 128; // Maximum grid size with given probability
    } else {
        gridRows = ofRandom(1, 128);
        gridCols = ofRandom(1, 128);
    }

    cellWidth = imageWidth / static_cast<float>(gridCols);
    cellHeight = imageHeight / static_cast<float>(gridRows);

    cellAspectRatio = cellWidth / cellHeight;
}

void ofApp::draw() { 
    if (image.isAllocated()) {
        for (int row = 0; row < gridRows; ++row) {
            for (int col = 0; col < gridCols; ++col) {
                int colSpan = 1; 
                int rowSpan = 1; 

                // Create 0-10 merged cells
                while (numMerges != 0) {
                    int mergeType = ofRandom(4);
                    
                    if (mergeType == 1 && (col < gridCols - 1) && (row < gridRows - 1)) {
                        colSpan = rowSpan = 2; // 2x2 cells
                    } else if (mergeType == 2 && (row < gridRows - 1)) {
                        rowSpan = 2; // 1x2 cells
                    } else if (mergeType == 3 && (col < gridCols - 1)) {
                        colSpan = 2; // 2x1 cells
                    }
                }

                cellDrawWidth = cellWidth * colSpan; 
                cellDrawHeight = cellHeight * rowSpan; 

                nthColX = col * cellWidth; 
                nthRowY = row * cellHeight;

                focusWidth = (awef * ((cellWidth * 0.66) - 1)) + 1; // Decide scale factor when personDetected
                focusHeight = (awef * ((cellHeight * 0.66) - 1)) + 1; // Decide scale factor when personDetected

                offsetX = focusWidth * 0.2; 
                offsetY = focusHeight * 0.2; 

                minNthFocusX = nthColX + (cellWidth / 2); 
                minNthFocusY = nthRowY + (cellHeight / 2);

                maxNthFocusX = ((cellWidth * meanCenter.x) - (focusWidth / 2));
                maxNthFocusY = ((cellHeight * meanCenter.y) - (focusHeight / 2));

                nthFocusX = ((awef * (maxNthFocusX - minNthFocusX)) + minNthFocusX) + ofRandom(-offsetX, offsetX);
                nthFocusY = ((awef * (maxNthFocusY - minNthFocusY)) + minNthFocusY) + ofRandom(-offsetY, offsetY);
                
                image.drawSubsection(nthColX, nthRowY, cellDrawWidth, cellDrawHeight, nthFocusX, nthFocusY, focusWidth, focusHeight);

                ofNoFill();
                ofDrawRectangle(nthColX, nthRowY, cellDrawWidth, cellDrawHeight);
            }
        }
    }
}
