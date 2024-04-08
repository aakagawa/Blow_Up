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

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            bigCells[row][col] = glm::vec2(1, 1); 
        }
    }
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
            if (progress == 0) {
                ofLogNotice() << "ping!!"; 
                // Offset cell content
                for (int row = 0; row < gridRows; ++row) {
                    for (int col = 0; col < gridCols; ++col) {
                        // Generate random offsets within a desired range
                        offsetFactors[row][col].x = ofRandom(-2.0f, 2.0f);
                        offsetFactors[row][col].y = ofRandom(-2.0f, 2.0f);
                    }
                }
                // Choose cells to merge 
                mergeCount = ofRandom(0, 11);
                for (int i = 0; i < mergeCount; ++i) {
                    thisRow = ofRandom(0, gridRows);
                    thisCol = ofRandom(0, gridCols);
                    rowSpan = ofRandom(1, 3);
                    colSpan = ofRandom(1, 3);
                    bigCells[thisRow][thisCol] = glm::vec2(rowSpan, colSpan);
                }
            }
        } else {
            ofLogNotice() << "person undetected ramp";
            awef = ofLerp(1, 0, progress);
            if (progress == 1) {
                for (int row = 0; row < gridRows; ++row) {
                    for (int col = 0; col < gridCols; ++col) {
                        bigCells[row][col] = glm::vec2(1, 1); 
                    }
                }
            }
             // numMerges = 0;
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
    ofLogNotice() << "activePersonCount: " << activePersonCount;
    meanCenter = (activePersonCount > 0) ? sumPositions / static_cast<float>(activePersonCount) : glm::vec2(0.5, 0.5);
}

void ofApp::updateGrid() {
    float prob = ofRandom(1.0);
    if (prob < maxGridProbability) {
        gridRows = gridCols = 64; // Maximum grid size with given probability
    } else {
        gridRows = ofRandom(1, 64);
        gridCols = ofRandom(1, 64);
    }

    cellWidth = imageWidth / static_cast<float>(gridCols);
    cellHeight = imageHeight / static_cast<float>(gridRows);

    cellAspectRatio = cellWidth / cellHeight;
}

void ofApp::draw() { 
    if (image.isAllocated()) {
        for (int row = 0; row < gridRows; ++row) {
            for (int col = 0; col < gridCols; ++col) {
                rowSpan = 1;
                colSpan = 1;

                if (bigCells[row][col].x > 1 || bigCells[row][col].y > 1) {
                    ofLogNotice() << "bigCell!";
                    rowSpan = bigCells[row][col].x;
                    colSpan = bigCells[row][col].y;
                }

                cellDrawWidth = cellWidth * colSpan; 
                cellDrawHeight = cellHeight * rowSpan; 

                nthColX = cellWidth * col;
                nthRowY = cellHeight * row;

                focusWidth = (awef * ((cellWidth * 3) - 1)) + 1; // Decide scale factor when personDetected
                focusHeight = (awef * ((cellHeight * 3) - 1)) + 1; // Decide scale factor when personDetected

                offsetX = cellWidth * offsetFactors[row][col].x;
                offsetY = cellHeight * offsetFactors[row][col].y;

                minNthFocusX = nthColX + (cellWidth / 2); 
                minNthFocusY = nthRowY + (cellHeight / 2);

                maxNthFocusX = (nthColX - ((focusWidth - cellWidth) / 2) + ((focusWidth * meanCenter.x) - (focusWidth / 2))) + offsetX;
                maxNthFocusY = (nthRowY - ((focusHeight - cellHeight) / 2) + ((focusHeight * meanCenter.y) - (focusHeight / 2))) + offsetY;

                nthFocusX = (((awef * (maxNthFocusX - minNthFocusX)) + minNthFocusX));
                nthFocusY = (((awef * (maxNthFocusY - minNthFocusY)) + minNthFocusY));
            
                image.drawSubsection(nthColX, nthRowY, cellDrawWidth, cellDrawHeight, nthFocusX, nthFocusY, focusWidth, focusHeight);

                ofNoFill();
                ofDrawRectangle(nthColX, nthRowY, cellDrawWidth, cellDrawHeight);
            }
        }
    }
}
