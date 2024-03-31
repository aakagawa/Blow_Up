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
            if (futureResult.valid() ? futureResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready : true) {
                futureResult = std::async(std::launch::async, &ofApp::processFrame, this, webcam.getPixels());
            }
        }   
    }

    if (isTransitioning) {
        uint64_t elapsedTime = ofGetElapsedTimeMillis() - transitionStartTime;
        float transitionDuration = 5000; // 5000ms for the transition
        float progress = elapsedTime / transitionDuration;
        progress = ofClamp(progress, 0.0, 1.0); // Clamp progress to [0, 1]

        currentZoomFactor = ofLerp(1.0, targetZoomFactor, progress);
        resolutionFactor = ofLerp(0.0, targetResolutionFactor, progress);
            
        if (progress >= 1.0) {
            isTransitioning = false; // End transition
        }
    }
}

void ofApp::draw() {
    ofSetFullscreen(true);
    ofPixels& pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight());

    if (isTransitioning) {
        // Apply zoom centered on meanCenter
        ofPushMatrix();
        ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
        ofTranslate(-meanCenter.x * currentZoomFactor, -meanCenter.y * currentZoomFactor);
        ofScale(currentZoomFactor, currentZoomFactor);
    }

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            float offsetX = isTransitioning ? ofRandom(-mode2MaxOffsetFactor * cellWidth, mode2MaxOffsetFactor * cellWidth) : 0;
            float offsetY = isTransitioning ? ofRandom(-mode2MaxOffsetFactor * cellHeight, mode2MaxOffsetFactor * cellHeight) : 0;
            fillCell(pixels, col * cellWidth + offsetX, row * cellHeight + offsetY, resolutionFactor);
        }
    }

    if (isTransitioning) {
        ofPopMatrix();
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

    bool personDetected = false;
    glm::vec2 sumPositions(0, 0);
    int detectedPersonsCount = 0;
    std::vector<bool> updated(objects.size(), false); // Track which tracked people are updated

    for (auto& object : objects) {
        if (object.ident.text != "person") continue; // Focus on persons only

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
    }

    // Check for inactivity
    for (size_t i = 0; i < trackedPeople.size(); ++i) {
        if (!updated[i] && (currentTime - trackedPeople[i].lastMoveTime > inactiveTimeThreshold)) {
            trackedPeople[i].isActive = false;
        }
    }

    // If persons are detected, calculate the mean center and initiate transition
    if (personDetected) {
        meanCenter = sumPositions / static_cast<float>(detectedPersonsCount);

        // Only start the transition if it's not already ongoing
        if (!isTransitioning) {
            isTransitioning = true;
            transitionStartTime = ofGetElapsedTimeMillis();
            currentZoomFactor = 1.0; // Reset zoom factor for the transition
            resolutionFactor = 0.0; // Reset resolution factor for the transition
        }
    }
}

void ofApp::fillCell(ofPixels& pixels, int col, int row, float resolutionFactor) {
    float cellStartX = col * cellWidth;
    float cellStartY = row * cellHeight;
    float cellEndX = cellStartX + cellWidth;
    float cellEndY = cellStartY + cellHeight;

    // Use calculateAverageColor for low resolutionFactor, directly drawing for higher values
    if (resolutionFactor <= 0.1) { // Threshold for using average color
        ofColor avgColor = calculateAverageColor(pixels, cellStartX, cellStartY, cellEndX, cellEndY);
        ofSetColor(avgColor);
        ofDrawRectangle(cellStartX, cellStartY, cellWidth, cellHeight);
    } else {
        // Sample pixels based on resolutionFactor to reduce drawing operations
        int step = std::max(1, static_cast<int>((1.0f - resolutionFactor) * 10));
        for (int x = cellStartX; x < cellEndX; x += step) {
            for (int y = cellStartY; y < cellEndY; y += step) {
                ofColor color = pixels.getColor(x, y);
                ofSetColor(color);
                ofDrawRectangle(x, y, step, step); // Draw sampled pixels as small squares
                
            }
        }
    }
}

ofColor ofApp::calculateAverageColor(ofPixels& pixels, float startX, float startY, float endX, float endY) {
    startX = ofClamp(startX, 0, pixels.getWidth() - 1);
    startY = ofClamp(startY, 0, pixels.getHeight() - 1);
    endX = ofClamp(endX, startX, pixels.getWidth());
    endY = ofClamp(endY, startY, pixels.getHeight());

    // Sample rate reduces the number of pixels processed for averaging
    int sampleRate = 10; // Adjust based on performance vs accuracy needs
    long totalRed = 0, totalGreen = 0, totalBlue = 0;
    int pixelCount = 0;

    for (int x = startX; x < endX; x += sampleRate) {
        for (int y = startY; y < endY; y += sampleRate) {
            ofColor color = pixels.getColor(x, y);
            totalRed += color.r;
            totalGreen += color.g;
            totalBlue += color.b;
            pixelCount++;
        }
    }

    if (pixelCount == 0) return ofColor(0, 0, 0); // Avoid division by zero
    return ofColor(totalRed / pixelCount, totalGreen / pixelCount, totalBlue / pixelCount);
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

int ofApp::roundSliderValue(int currentValue, int increment) {
    int remainder = currentValue % increment;
    if (remainder != 0) {
        int newValue = remainder >= (increment / 2) ? (currentValue + (increment - remainder)) : (currentValue - remainder);
        return newValue;
    }
    return currentValue;
}
