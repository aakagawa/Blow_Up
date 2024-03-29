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
    gui.add(timerMedian.setup("Timer Median", 2000, 500, 3000));
    gui.add(timerUncertainty.setup("Timer Uncertainty", 500, 0, 1500));
    gui.add(morphInDuration.setup("Morph In Duration", 2000, 1000, 5000));
    gui.add(morphOutDuration.setup("Morph Out Duration", 2000, 1000, 5000));
    gui.add(untilMorphIn.setup("Until Morph In", 2000, 1000, 5000));
    gui.add(untilMorphOut.setup("Until Morph Out", 1000, 500, 3000));
    gui.add(mode2MaxOffsetFactor.setup("Mode 2 Max Offset Factor", 0.1, 0.0, 1.0));
    gui.add(baseZoomFactor.setup("Detection Zoom Factor", 1.2, 1.0, 5.0));
    gui.add(bigCellZoomFactor.setup("Big Cell Zoom Factor", 0.1, 0.0, 1.0));
    setupWebcamButton.addListener(this, &ofApp::setupWebcam);

    setupWebcam();
    // Initialize YOLO model
    if (!yolo.setup("model", "classes.txt")) {
        ofLogError() << "Failed to setup YOLO model!";
        std::exit(EXIT_FAILURE);
    }
    yolo.setNormalize(true); // Normalize object bounding box coordinates
    
    isObjectDetected = false;
    mode = 1;

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
            // Process the frame asynchronously
            if (futureResult.valid() ? futureResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready : true) {
                futureResult = std::async(std::launch::async, &ofApp::processFrame, this, webcam.getPixels());
            }
        }
    }

    toggleModes();
    if (isTransitioning) {
        unit64_t elapsedTime = ofGetElapsedTimeMillis() - transitionStartTime; 
        float progress = elapsedTime / static_cast<float>(mode == 1 ? morphInDuration : morphOutDuration);

        if (progress >= 1.0) {
            progress = 1.0;
            isTransitioning = false; 
        }

        currentZoomFactor = ofLerp(currentZoomFactor, targetZoomFactor, progress);
    }
    
    if (ofGetElapsedTimeMillis() > gridRefreshTimer) {
        updateGrid();
        gridRefreshTimer = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);
    }
}

void ofApp::draw() {
    ofSetFullscreen(true);

    drawGrid();

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

    std::vector<TrackedObject> updatedTrackedObjects;

    for (auto& object : objects) {

        // Log object details
        ofLogNotice() << "Detected: " << object.ident.text 
                      << " with confidence: " << object.confidence;
                      
        // Accessing bounding box coordinates
        ofRectangle bbox = object.bbox;
        ofLogNotice() << "Bounding Box: [X: " << bbox.x << ", Y: " << bbox.y
                      << ", Width: " << bbox.width << ", Height: " << bbox.height << "]";

        glm::vec2 currentPosition(
            object.bbox.x,
            object.bbox.y
        );

        bool found = false;

        // Logic A for 'active'/'inactive' handling
        for (auto& tracked : trackedObjects) {
            if (tracked.yoloObject.ident.text == object.ident.text) { // Example matching method
                float distanceMoved = glm::distance(currentPosition, tracked.lastPosition);
                if (distanceMoved < activityThreshold * ofGetWidth()) {
                    if (currentTime - tracked.lastSeen > inactiveTimeThreshold) {
                        tracked.isActive = false;
                    }
                } else {
                    tracked.isActive = true;
                }
                tracked.lastPosition = currentPosition;
                tracked.lastSeen = currentTime;
                found = true;
                updatedTrackedObjects.push_back(tracked);
                break;
            }
        }

        // Logic B for 'active'/'inactive' handling
        // for (auto& tracked : trackedObjects) {
        // // Simplistic matching; consider improving for real applications
        //     if (tracked.yoloObject.ident.text == object.ident.text) {
        //         float distanceMoved = glm::distance(currentPosition, tracked.lastPosition);
        //         if (distanceMoved >= activityThreshold * std::max(ofGetWidth(), ofGetHeight())) {
        //             tracked.isActive = true;
        //         } else if (currentTime - tracked.lastSeen > inactiveTimeThreshold) {
        //             tracked.isActive = false;
        //         }
        //         tracked.lastPosition = currentPosition;
        //         tracked.lastSeen = currentTime;
        //         found = true;
        //         break;  // Important to prevent matching the same object multiple times
        //     }
        // }

        if (!found) {
            updatedTrackedObjects.emplace_back(object, currentPosition, currentTime);
        }
    }
    // REMOVE OLD OBJECTS OR HANDLE OBJECTS THAT WEREN'T UPDATED
}

void ofApp::updateGrid() {
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

void ofApp::drawGrid() {
    ofPixels& pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight()); // Resize webcam input to match window size

    // Call either renderMode1 or renderMode2 based on the current mode
    if (mode == 1) {
        renderMode1();
    } else if (mode == 2) {
        renderMode2();
    }
}

void ofApp::toggleModes() {
        bool hasActiveObjects = std::any_of(trackedObjects.begin(), trackedObjects.end(), [](const TrackedObject& obj) {
        return obj.isActive;
    });

    int newMode = hasActiveObjects ? 2 : 1; 

        if (newMode != mode) {
        // Transition starts
        transitionStartTime = ofGetElapsedTimeMillis();
        isTransitioning = true;
        
        if (newMode == 2) {
            // Zooming in
            currentZoomFactor = 1.0; // Assume starting from no zoom
            targetZoomFactor = baseZoomFactor; // Assume this is your desired zoom level
        } else {
            // Zooming out or other transitions
            currentZoomFactor = baseZoomFactor;
            targetZoomFactor = 1.0;
        }
    }

    mode = hasActiveObjects ? 2 : 1;
}

void ofApp::renderMode1() {
    // Iterate through each cell in the grid 
    for(int row = 0; row < gridRows; ++row){
        for(int col = 0; col < gridCols; ++col){
            // Calculate the center pixel coordinates of the current cell
            int centerX = (col + 0.5) * cellWidth;
            int centerY = (row + 0.5) * cellHeight;

            // Ensure the coordinates are within the bounds of the image
            centerX = ofClamp(centerX, 0, pixels.getWidth() - 1);
            centerY = ofClamp(centerY, 0, pixels.getHeight() - 1);

            // Get the color of the center pixel
            ofColor centerColor = pixels.getColor(centerX, centerY);

            // Set the fill color to the color of the center pixel
            ofSetColor(centerColor);

            // Draw the cell at the appropriate location with the color of the center pixel
            ofDrawRectangle(col * cellWidth, row * cellHeight, cellWidth, cellHeight);
        }
    }
}

void ofApp::renderMode2() {
    glm::vec2 meanCenter(0, 0);
    int activeCount = 0;

    for (const auto& obj : trackedObjects) {
        if (obj.isActive) {
            meanCenter += obj.lastPosition;
            activeCount++;
        }
    }

    if (activeCount > 0) {
        meanCenter /= activeCount;
        ofPushMatrix(); // Save the current graphics context

        // Translate so that meanCenter becomes the new origin
        ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2); // Move origin to screen center
        ofTranslate(-meanCenter.x * baseZoomFactor, -meanCenter.y * baseZoomFactor); // Center on meanCenter, adjusted by zoomFactor

        // Apply Zoom
        ofScale(currentZoomFactor, currentZoomFactor);

        // Loop through each cell
        for(int row = 0; row < gridRows; ++row) {
            for(int col = 0; col < gridCols; ++col) {
                // Calculate the base position for the cell
                float baseX = col * cellWidth;
                float baseY = row * cellHeight;

                // Calculate random offset within the range of 0.0 to 0.5 of cell dimensions
                float offsetX = ofRandom(0.0, mode2MaxOffsetFactor) * cellWidth;
                float offsetY = ofRandom(0.0, mode2MaxOffsetFactor) * cellHeight;

                // Adjusted position with offset
                float posX = baseX + offsetX;
                float posY = baseY + offsetY;

                // Now draw your cell's content using posX and posY as the top-left corner
                // This is where you would draw the content for each cell, now offset
                // For demonstration, drawing a rectangle representing the cell's content:
                ofDrawRectangle(posX, posY, cellWidth - offsetX, cellHeight - offsetY); // Reduced size to prevent overflow due to offset
            }
        }

        // Now draw your objects or scene here
        // The drawing here will be zoomed in on meanCenter

        ofPopMatrix(); // Restore the original graphics context
    }
}

int ofApp::roundSliderValue(int currentValue, int increment) {
    int remainder = currentValue % increment;
    if (remainder != 0) {
        int newValue = remainder >= (increment / 2) ? (currentValue + (increment - remainder)) : (currentValue - remainder);
        return newValue;
    }
    return currentValue;
}
