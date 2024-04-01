#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(30); // Set rendering framerate to 60fps
    
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
        
    ofLogNotice() << "personDetected = " << personDetected;

    
    // if (personDetected) {
    //     if (!isTransitioning) {
    //         isTransitioning = true;
    //         transitionStartTime = ofGetElapsedTimeMillis();
    //         // Set the target factors for detected state
    //     }
    // } 

    // if (isTransitioning) {
    //     uint64_t elapsedTime = ofGetElapsedTimeMillis() - transitionStartTime;
    //     float transitionDuration = 5000; // Transition duration
    //     float progress = elapsedTime / static_cast<float>(transitionDuration);
    //     progress = ofClamp(progress, 0.0, 1.0);

    //     if (personDetected) {
    //         zoomFactor = ofLerp(targetZoomFactorUndetected, targetZoomFactorDetected, progress);
    //         resolutionFactor = ofLerp(targetResolutionFactorUndetected, targetResolutionFactorDetected, progress);
    //         offsetFactor = ofLerp(targetOffsetFactorUndetected, targetOffsetFactorDetected, progress);
    //     } else {
    //         zoomFactor = ofLerp(targetZoomFactorDetected, targetOffsetFactorUndetected, progress);
    //         resolutionFactor = ofLerp(targetResolutionFactorDetected, targetOffsetFactorUndetected, progress);
    //         offsetFactor = ofLerp(targetOffsetFactorDetected, targetOffsetFactorUndetected, progress);
    //     }

    //     if (progress >= 1.0) {
    //         isTransitioning = false;
    //     }
    // }

    if (ofGetElapsedTimeMillis() > gridRefreshTimer) {
        updateGrid();
        gridRefreshTimer = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);
    }

}

void ofApp::draw() {
    ofSetFullscreen(true);
    ofPixels& pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight());

    ofLogNotice() << "meanCenter = " << meanCenter; 
    
    // zoom(meanCenter, zoomFactor);

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            fillCell(pixels, col * cellWidth, row * cellHeight, resolutionFactor);
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
    uint64_t currentTime = ofGetElapsedTimeMillis();

    glm::vec2 sumPositions(0, 0);
    int detectedPersonsCount = 0;
    std::vector<bool> updated(objects.size(), false); // Track which tracked people are updated

    for (auto& object : objects) {
        if (object.ident.text == "person") { // Focus on persons only

            glm::vec2 currentPosition = glm::vec2(object.bbox.x + object.bbox.width / 2, object.bbox.y + object.bbox.height / 2);
            bool found = false;

            sumPositions += currentPosition;
            detectedPersonsCount++;
            personDetected = true;

            // Update existing tracked person or add a new one
            for (size_t i = 0; i < trackedPeople.size(); ++i) {
                if (glm::distance(currentPosition, trackedPeople[i].position) < movementThreshold) {
                    trackedPeople[i].position = currentPosition;
                    trackedPeople[i].lastMoveTime = currentTime;
                    trackedPeople[i].isActive = true;
                    updated[i] = true;
                    found = true;
                    break;
                }
            }

            if (!found) {
                trackedPeople.emplace_back(TrackedPerson{currentPosition, currentTime});
            }

            sumPositions += currentPosition;
            detectedPersonsCount++;
            
        } else {
            personDetected = false;
        }
    }

    // Check for inactivity
    for (size_t i = 0; i < trackedPeople.size(); ++i) {
        if (!updated[i] && (currentTime - trackedPeople[i].lastMoveTime > inactiveTimeThreshold)) {
            trackedPeople[i].isActive = false;
        }
    }

    if (personDetected) {
        meanCenter = sumPositions / static_cast<float>(detectedPersonsCount);
    }
}

void ofApp::fillCell(ofPixels& pixels, float nthCellX, float nthCellY, float resolutionFactor) {
    // Map the resolutionFactor to a range that defines the number of subdivisions within the cell
    subdivisions = 1 + static_cast<int>(resolutionFactor * (maxSubdivisions - 1));

    // Calculate the size of each sub-cell
    subCellWidth = cellWidth / subdivisions;
    subCellHeight = cellHeight / subdivisions;

    for (int subCol = 0; subCol < subdivisions; ++subCol) {
        for (int subRow = 0; subRow < subdivisions; ++subRow) {
            // Calculate the average color of this sub-cell
            nthSubCellX = nthCellX + (subCol * subCellWidth);
            nthSubCellY = nthCellY + (subRow * subCellHeight);
            subnthCellWidth = nthSubCellX + subCellWidth;
            susubnthCellHeight = nthSubCellY + subCellHeight;
            
            ofColor avgColor = calculateAverageColor(pixels, nthSubCellX, nthSubCellY, subnthCellWidth, susubnthCellHeight);
            ofSetColor(avgColor);
            ofDrawRectangle(nthSubCellX, nthSubCellY, subCellWidth, subCellHeight);
        }
    }
}

ofColor ofApp::calculateAverageColor(ofPixels& pixels, float x, float y, float width, float height) {
    totalR = 0, totalG = 0, totalB = 0;
    count = 0;
    grain = std::max(1.0f, (width * height) / 16.0f); // Keep the grain calculation but ensure it's at least 1
    grain = std::min(grain, std::min(width, height)); // Ensure grain does not exceed the dimensions of the area
    for (int i = x; i < width; i += grain) {
        for (int j = y; j < height; j += grain) {
            ofColor c = pixels.getColor(i, j);
            totalR += c.r;
            totalG += c.g;
            totalB += c.b;
            ++count;
        }
    }
    if (count == 0) {
        return ofColor(0, 0, 0); // Return a default color if no pixels were processed
    } 

    subCellR = totalR / count;
    subCellG = totalG / count;
    subCellB = totalB / count;

    return ofColor(subCellR, subCellG, subCellB);
}

// void ofApp::zoom(glm::vec2 meanCenter, float zoomFactor) {

//     ofPushMatrix(); // Save the current coordinate system
//     ofScale(zoomFactor, zoomFactor); // Apply zoom
//     ofTranslate((meanCenter.x * ofGetWidth()), (meanCenter.y * ofGetHeight())); // Move back by meanCenter adjusted by zoom

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
