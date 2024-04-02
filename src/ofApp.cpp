#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60); // Set rendering framerate to 60fps
    
    // Setup GUI
    showGui = true;
    gui.setup();
    gui.setSize(540, 540);
    gui.setDefaultWidth(500); // Slightly less than panel width to fit nicely
    gui.setDefaultHeight(45); // Taller elements for better interaction
    gui.add(webcamWidth.setup("Width", 1920, 1920, 3840));
    gui.add(webcamHeight.setup("Height", 1080, 1080, 2160));
    gui.add(timerMedian.setup("Timer Median", 3000, 500, 5000));
    gui.add(timerUncertainty.setup("Timer Uncertainty", 500, 0, 1500));
    gui.add(morphInDuration.setup("Morph In Duration", 2000, 1000, 5000));
    gui.add(morphOutDuration.setup("Morph Out Duration", 2000, 1000, 5000));
    gui.add(untilMorphIn.setup("Until Morph In", 2000, 1000, 5000));
    gui.add(untilMorphOut.setup("Until Morph Out", 1000, 500, 3000));

    setupWebcam();
    // Initialize YOLO model
    if (!yolo.setup("model", "classes.txt")) {
        ofLogError() << "Failed to setup YOLO model!";
        std::exit(EXIT_FAILURE);
    }
    yolo.setNormalize(true); // Normalize object bounding box coordinates

    gridRefreshTimer = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);

    updateGrid();
}

void ofApp::update() {
    webcam.update();

    static int frameCount = 0;
    int processEveryNthFrame = 3;

    if (webcam.isFrameNew()) {
        if (++frameCount % processEveryNthFrame == 0) {
            frameCount = 0; // Reset frame counter to avoid overflow
            if (futureResult.valid() ? futureResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready : true) {
                futureResult = std::async(std::launch::async, &ofApp::processFrame, this, webcam.getPixels());
            }
        }
    }
        
    ofLogNotice() << "personDetected in update() = " << personDetected;
    ofLogNotice() << "isTransitioning = " << isTransitioning;

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
            resolutionFactor = ofLerp(targetResolutionFactorUndetected, targetResolutionFactorDetected, progress);
            // zoomFactor = ofLerp(targetZoomFactorUndetected, targetZoomFactorDetected, progress);
            // offsetFactor = ofLerp(targetOffsetFactorUndetected, targetOffsetFactorDetected, progress);
            ofLogNotice() << "resolutionFactor: "<< resolutionFactor;
        } 
        else {
            ofLogNotice() << "person undetected ramp";
            resolutionFactor = ofLerp(targetResolutionFactorDetected, targetResolutionFactorUndetected, progress);
            // zoomFactor = ofLerp(targetZoomFactorDetected, targetZoomFactorUndetected, progress);
            // offsetFactor = ofLerp(targetOffsetFactorDetected, targetOffsetFactorUndetected, progress);
            ofLogNotice() << "resolutionFactor: "<< resolutionFactor;
        }

        if (progress >= 1.0) {
            isTransitioning = false;
            lastPersonDetected = personDetected; // Update the last state to the current at the end of transition
        }
    }
    // Ensure to update the lastPersonDetected variable outside and after the if(isTransitioning) block if the transition doesn't start
    else if (personDetected != lastPersonDetected) {
        lastPersonDetected = personDetected;
    }

    // updateGrid
    if (ofGetElapsedTimeMillis() > gridRefreshTimer) {
        updateGrid();
        gridRefreshTimer = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);
    }

    // Map the resolutionFactor to a range that defines the number of subdivisions within the cell
    subdivisions = 1 + static_cast<int>(resolutionFactor * (maxSubdivisions - 1));

    // Calculate the size of each sub-cell
    subCellWidth = cellWidth / subdivisions;
    subCellHeight = cellHeight / subdivisions;

    ofLogNotice() << "subCellWidth: " << subCellWidth;
    ofLogNotice() << "subCellHeight: " << subCellHeight;
}

void ofApp::draw() {
    ofSetFullscreen(true);
    ofPixels& pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight());
    
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            fillCell(pixels, col * cellWidth, row * cellHeight);
            // zoom(meanCenter, zoomFactor);
            // offsetCell(offsetFactor);
        }
    }

    if (showGui) {
        gui.draw();
    }
}

void ofApp::keyPressed(int key) {
    if (key == 'g' || key == 'G') {
        showGui = !showGui; // Toggle GUI visibility
    }
}

void ofApp::setupWebcam() {
    webcam.setDesiredFrameRate(30); // Set webcam framerate to 30fps
    webcam.initGrabber(webcamWidth, webcamHeight);
}

void ofApp::processFrame(ofPixels pixels) {
    pixels.resize(960, 540); // Adjust resizing directly on ofPixels
    yolo.setInput(pixels);
    yolo.update();

    // Retrieve and log detected objects
    auto objects = yolo.getObjects();
    currentTime = ofGetElapsedTimeMillis();
    personDetected = false;

    sumPositions = glm::vec2(0, 0);
    activePersonCount = 0;

    for (auto& object : objects) {
        if (object.ident.text == "person") { // Focus on persons only
            glm::vec2 currentPosition = glm::vec2(object.bbox.x + object.bbox.width / 2, object.bbox.y + object.bbox.height / 2);
            bool found = false;

            // Check if same person 
            for (auto& trackedPerson : trackedPeople) {
                float movement = glm::distance(currentPosition, trackedPerson.position);
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
            
            if (!found) { // 
                trackedPeople.emplace_back(TrackedPerson{currentPosition, currentTime});
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
    meanCenter = activePersonCount > 0 ? sumPositions / static_cast<float>(activePersonCount) : glm::vec2(ofGetWidth() / 2, ofGetHeight() / 2);
}

void ofApp::fillCell(ofPixels& pixels, float nthCellX, float nthCellY) {
    for (int subCol = 0; subCol < subdivisions; ++subCol) {
        for (int subRow = 0; subRow < subdivisions; ++subRow) {
            // Calculate the average color of this sub-cell
            nthSubCellX = nthCellX + (subCol * subCellWidth);
            nthSubCellY = nthCellY + (subRow * subCellHeight);
            nthSubCellWidth = nthSubCellX + subCellWidth;
            nthSubCellHeight = nthSubCellY + subCellHeight;
            
            ofColor avgColor = calculateAverageColor(pixels, nthSubCellX, nthSubCellY, nthSubCellWidth, nthSubCellHeight);
            ofSetColor(avgColor);
            ofDrawRectangle(nthSubCellX, nthSubCellY, subCellWidth, subCellHeight);
        }
    }
}

ofColor ofApp::calculateAverageColor(ofPixels& pixels, float x, float y, float width, float height) {
    count = 0;
    totalR = 0, totalG = 0, totalB = 0;
    // grain = std::max(1.0f, (width * height) / 4.0f); // Keep the grain calculation but ensure it's at least 1
    // grain = std::min(grain, std::min(width, height)); // Ensure grain does not exceed the dimensions of the area
    for (int i = floor(x); i < width; i += 2) {
        for (int j = floor(y); j < height; j += 2) {
            ofColor c = pixels.getColor(i, j);
            totalR += c.r;
            totalG += c.g;
            totalB += c.b;
            ++count;
        }
    }
    if (count == 0) {
        return ofColor(0, 0, 0); // Return a default color if no pixels were processed
    } else {
        subCellR = totalR / count;
        subCellG = totalG / count;
        subCellB = totalB / count;
        return ofColor(subCellR, subCellG, subCellB);
    }
    
}

// void ofApp::zoom(glm::vec2 meanCenter, float zoomFactor) {

//     ofPushMatrix(); // Save the current coordinate system
//     ofTranslate((meanCenter.x * ofGetWidth()), (meanCenter.y * ofGetHeight())); // Move back by meanCenter adjusted by zoom
//     ofScale(zoomFactor, zoomFactor); // Apply zoom

//     ofPopMatrix(); // Restore the original coordinate system
// }

// void ofApp::offsetCell(float offsetFactor) { 
//     offsetX = ofRandom(-offsetFactor, offsetFactor) * cellWidth;
//     offsetY = ofRandom(-offsetFactor, offsetFactor) * cellHeight;

//     ofTranslate(offsetX, offsetY);
// }

void ofApp::updateGrid() {
    ofPixels& pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight());

    float prob = ofRandom(1.0);
    if (prob < maxGridProbability) {
        gridRows = gridCols = 256; // Maximum grid size with given probability
    } else {
        gridRows = ofRandom(1, 256);
        gridCols = ofRandom(1, 256);
    }

    cellWidth = ofGetWidth() / static_cast<float>(gridCols);
    cellHeight = ofGetHeight() / static_cast<float>(gridRows);

}

int ofApp::roundSliderValue(int currentValue, int increment) {
    int remainder = currentValue % increment;
    if (remainder != 0) {
        int newValue = remainder >= (increment / 2) ? (currentValue + (increment - remainder)) : (currentValue - remainder);
        return newValue;
    }
    return currentValue;
}