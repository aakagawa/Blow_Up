#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60); 
    
    showGui = true;
    gui.setup();
    gui.setSize(720, 740);
    gui.setDefaultWidth(720);
    gui.setDefaultHeight(20);

    // Scale camera to display 
    grabber.setup(3840, 2160); // 4k if available
    grabber.setDesiredFrameRate(30); // Set webcam framerate to 30fps
    grabber.update();

    // Display settings
    gui.add(displaySettings.setup("DISPLAY SETTINGS", ""));
    gui.add(flipImage.setup("Flip image", true)); 
    
    // Grid settings
    gui.add(gridSettings.setup("GRID SETTINGS", ""));
    gui.add(cellOutline.setup("Cell outline (0: none 1: white 2: black)", 0, 0, 2)); 
    gui.add(minRow.setup("Minimum num of rows", 2, 2, 256)); 
    gui.add(minCol.setup("Minimum num of columns", 2, 2, 256)); 
    gui.add(maxRow.setup("Maximum num of rows", 128, 2, 256));  
    gui.add(maxCol.setup("Maximum num of columns", 128, 2, 256)); 

    // Animation settings
    gui.add(animationSettings.setup("ANIMATION SETTINGS", ""));
    gui.add(gridUpdateInterval.setup("Grid update interval", 5000, 2000, 10000)); 
    gui.add(gridUpdateIntervalUncertainty.setup("Grid update interval uncertainty", 500, 0, 1000)); 
    gui.add(maxGridProbability.setup("Probability of maximum grid", 0.2, 0, 1.0)); 
    gui.add(max1x2Cell.setup("Max num of 1x2 cells", 2, 0, 10));  
    gui.add(max2x1Cell.setup("Max num of 2x1 cells", 2, 0, 10)); 
    gui.add(max2x2Cell.setup("Max num of 2x2 cells", 2, 0, 10)); 
    gui.add(bigCellScale.setup("Big cell scale", 1, 0, 4));
    gui.add(scaleFactor.setup("Scale factor (blow up)", 0.25, 0.05, 0.5));
    gui.add(offsetFactorX.setup("Horizontal offset factor (blow up)", 7.5, 0.0, 20.0));
    gui.add(offsetFactorY.setup("Vertical offset factor (blow up)", 2.5, 0.0, 10.0));
    gui.add(transDuration.setup("Transition duration", 1000, 500, 5000)); 
    gui.add(focusFollowSpeed.setup("Focus follow speed", 0.2, 0.05, 0.5));  

    // Detection settings
    gui.add(detectionSettings.setup("DETECTION SETTINGS", ""));
    gui.add(detectionAreaTopLeftX.setup("Detection area top left x", 0, 0, grabber.getWidth())); 
    gui.add(detectionAreaTopLeftY.setup("Detection area top left y", 0, 0, grabber.getHeight())); 
    gui.add(detectionAreaBottomRightX.setup("Detection area bottom right x (default: maximum width)", grabber.getWidth(), 0, grabber.getWidth())); 
    gui.add(detectionAreaBottomRightY.setup("Detection area bottom right y (default: maximum height)", grabber.getHeight(), 0, grabber.getHeight())); 
    gui.add(minConfidence.setup("Required confidence", 0.5, 0.0, 0.9)); 
    gui.add(minSize.setup("Required size", 0.2, 0.0, 0.9)); 
    gui.add(transInTimeout.setup("Transition in timeout (time until transition after detection)", 0, 0, 5000)); 
    gui.add(transOutTimeout.setup("Transition out timeout (time until transition after undetection)", 3000, 0, 5000)); 
    gui.add(inactiveTimeout.setup("Time until inactive person is considered un detected", 10000, 5000, 30000)); 

    // Advanced Settings 
    gui.add(advancedSettings.setup("ADVANCED SETTINGS", ""));
    gui.add(frameGrain.setup("detection frameGrain", 2, 1, 6));
    gui.add(maxMovementThreshold.setup("maxMovementThreshold", 0.75, 0.5, 1.0));
    gui.add(minMovementThreshold.setup("minMovementThreshold", 0.05, 0.01, 0.2));
    
    // Initialize diplay 
    inputWidth = grabber.getWidth(); 
    inputHeight = grabber.getHeight();

    outputWidth = ofGetWidth();
    outputHeight = ofGetHeight(); 

    // Calculate camera and display aspect ratios
    cameraAspectRatio = inputWidth / static_cast<float>(inputHeight);
    displayAspectRatio = outputWidth / static_cast<float>(outputHeight);

    if (cameraAspectRatio > displayAspectRatio) { // Camera is wider than display
        baseScale = outputHeight / static_cast<float>(inputHeight);
    } else { // Camera is taller than display
        baseScale = outputWidth / static_cast<float>(inputWidth);
    }

    // Calculate new dimensions
    imageWidth = inputWidth * baseScale;
    imageHeight = inputHeight * baseScale;

    // Initialize YOLO model 
    if (!yolo.setup("model", "classes.txt")) { 
        ofLogError() << "Failed to setup YOLO model!";
        std::exit(EXIT_FAILURE);
    }

    // Normalize object bounding box coordinates
    yolo.setNormalize(true); 

    // Initialize bigCells and offsetFactors size 
    for (int row = 0; row < MAX_ROWS; ++row) {
        for (int col = 0; col < MAX_COLS; ++col) {
            bigCells[row][col] = glm::vec2(1, 1); 
            offsetFactors[row][col] = glm::vec2(1, 1); 
        }
    }

    // Initialize gridRefreshInterval
    gridRefreshInterval = ofGetElapsedTimeMillis() + gridUpdateInterval + ofRandom(-gridUpdateIntervalUncertainty, gridUpdateIntervalUncertainty);
    
    // Initialize grid
    updateGrid();
}

void ofApp::update() {
    grabber.update();

    static int frameCount; 

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

    // Transition timeout
    if (personDetected) {
        if (!lastPersonDetected) { // Person detection state changed from undetected to detected
            if (!pendingDetectionState) { // If not already waiting
                pendingDetectionState = true;
                detectionTimestamp = ofGetElapsedTimeMillis(); // Start timer
            }
        } else if (pendingDetectionState && ofGetElapsedTimeMillis() - detectionTimestamp > transInTimeout && !isTransitioning) {
            // ofLogNotice() << "transInTimeout";
            isTransitioning = true;
            transitionStartTime = ofGetElapsedTimeMillis();
            // Reset progress to start a new transition if needed
            progress = 0.0;
            pendingDetectionState = false;
        }
    } else {
        if (lastPersonDetected) { // Person detection state changed from detected to undetected
            if (!pendingDetectionState) { // If not already waiting
                pendingDetectionState = true;
                detectionTimestamp = ofGetElapsedTimeMillis(); // Start timer
            }
        } else if (pendingDetectionState && ofGetElapsedTimeMillis() - detectionTimestamp > transOutTimeout && !isTransitioning) {
            // ofLogNotice() << "transOutTimeout";
            isTransitioning = true;
            transitionStartTime = ofGetElapsedTimeMillis();
            // Reset progress to start a new transition if needed
            progress = 0.0;        
            pendingDetectionState = false;
        }
    }

    // Transition
    if (isTransitioning) {
        rampx = ofGetElapsedTimeMillis() - transitionStartTime;
        progress = rampx / static_cast<float>(transDuration);
        progress = ofClamp(progress, 0.0, 1.0);
        if (personDetected) {
            // ofLogNotice() << "person detected ramp";
            rampy = ofLerp(0, 1, progress);

            if (progress == 0) {
                mergeCells();
                offsetCells();
            }
        } else {
            // ofLogNotice() << "person undetected ramp";
            rampy = ofLerp(1, 0, progress);

            if (progress == 1) {
                for (int row = 0; row < gridRows; ++row) { // Resetting mergeCells
                    for (int col = 0; col < gridCols; ++col) {
                        bigCells[row][col] = glm::vec2(1, 1);
                        offsetFactors[row][col] = glm::vec2(0, 0);
                    }
                }
            }
        }

        // Update the last state to the current at the end of transition
        if (progress >= 1.0) {
            isTransitioning = false;
            lastPersonDetected = personDetected; 
        }

    } else if (personDetected != lastPersonDetected) { // Update lastPersonDetected if transition didn't start 
        lastPersonDetected = personDetected;
    }

    // Update grid every gridRefreshInterval ± intervalUncertainty only if !personDetected
    if (!personDetected && !isTransitioning && !pendingDetectionState) { 
        if (ofGetElapsedTimeMillis() > gridRefreshInterval) {
            updateGrid();
            gridRefreshInterval = ofGetElapsedTimeMillis() + gridUpdateInterval + ofRandom(-gridUpdateIntervalUncertainty, gridUpdateIntervalUncertainty);
        }        
    }

    // Calculate position
    if (personDetected) { 
        currentFocus.x = ofLerp(currentFocus.x, targetFocus.x, focusFollowSpeed);
        currentFocus.y = ofLerp(currentFocus.y, targetFocus.y, focusFollowSpeed);
    }
}

void ofApp::processFrame(ofPixels pixels) {
    // Crop detection area
    detectionAreaTopLeft.x = detectionAreaTopLeftX;
    detectionAreaTopLeft.y = detectionAreaTopLeftY; 
    detectionAreaBottomRight.x = detectionAreaBottomRightX;
    detectionAreaBottomRight.y = detectionAreaBottomRightY;

    detectionAreaWidth = detectionAreaBottomRight.x - detectionAreaTopLeft.x;
    detectionAreaHeight = detectionAreaBottomRight.y - detectionAreaTopLeft.y;

    pixels.crop(detectionAreaTopLeft.x, detectionAreaTopLeft.y, detectionAreaWidth, detectionAreaHeight);
    
    pixels.resize(inputWidth * 0.75, inputHeight * 0.75);
    yolo.setInput(pixels);
    yolo.update();

    auto objects = yolo.getObjects();
    currentTime = ofGetElapsedTimeMillis();
    personDetected = false;
    activePersonCount = 0;
    domSize = 0; 

    for (auto& object : objects) {
        if (object.ident.text == "person") { // Check if object is person
            float confidence = object.confidence;
            if (confidence >= minConfidence) { // Check confidence
                float size = object.bbox.width * object.bbox.height;
                currentPosition = glm::vec2(object.bbox.x + object.bbox.width / 2, object.bbox.y + object.bbox.height / 2);
                if (size > minSize) { // Check size
                    personDetected = true; 
                    bool found = false;

                    for (auto& trackedPerson : trackedPeople) {
                        movement = glm::distance(currentPosition, trackedPerson.position);
                        if (movement < maxMovementThreshold) { // Maximum movement threshold to be considered the same person. 
                            if (movement > minMovementThreshold) { // Minimum movement threshold to be considered active.
                                trackedPerson.moveTimestamp = currentTime; // Update moveTimestamp
                            }
                            trackedPerson.position = currentPosition; // Update position of this person
                            trackedPerson.size = size; // Update size of this person
                            found = true; // same person found
                            break;
                        }
                    }

                    if (!found) {
                        trackedPeople.emplace_back(TrackedPerson{currentPosition, size, currentTime}); // New person, add to trackedPeople
                    }
                }
            }
        }
    }

    // Expire from trackedPeople 
    trackedPeople.erase(std::remove_if(trackedPeople.begin(), trackedPeople.end(), [this](const TrackedPerson& tp) {
        return (currentTime - tp.moveTimestamp) > expiryTime;
    }), trackedPeople.end());

    // Find largest active person
    for (auto& trackedPerson : trackedPeople) { // Checking if this person is still active
        if ((currentTime - trackedPerson.moveTimestamp) < inactiveTimeout) {
            activePersonCount++; 
            // ofLogNotice() << "trackedPerson.size: " << trackedPerson.size;
            // if not expired, logic here for size competition
            if (trackedPerson.size > domSize) {
                domSize = trackedPerson.size;
                targetFocus = trackedPerson.position;
            }   
        } 
    }

    if (activePersonCount <= 0 || activePersonCount > 99) {
        activePersonCount =  0; 
        personDetected = false; 
    }

    // ofLogNotice() << "targetFocus: " << targetFocus;
    // ofLogNotice() << "activePersonCount: " << activePersonCount;
}

void ofApp::updateGrid() {
    float prob = ofRandom(1.0);
    if (prob < maxGridProbability) { // Maximum grid size with given probability 
        gridRows = maxRow; 
        gridCols = maxCol;
    } else {
        gridRows = ofRandom(minRow, maxRow);
        gridCols = ofRandom(minCol, maxCol);
    }

    cellWidth = outputWidth / static_cast<float>(gridCols);
    cellHeight = outputHeight / static_cast<float>(gridRows);
}

void ofApp::mergeCells() {
    int mergeCount = ofRandom(0, 11);
    for (int i = 0; i < mergeCount; ++i) {
        int thisRow = ofRandom(0, gridRows);
        int thisCol = ofRandom(0, gridCols);
        rowSpan = ofRandom(1, 3); // Random between 1-2 
        colSpan = ofRandom(1, 3);

        rowSpan *= bigCellScale;
        colSpan *= bigCellScale;

        // Check if the merge is within the grid boundaries
        if (thisRow + rowSpan > gridRows || thisCol + colSpan > gridCols) {
            continue;
        }

        // Determine the type of merge and check against max allowed
        if ((rowSpan == 1 && colSpan == 2 && n1x2CellCount  >= max1x2Cell) ||
            (rowSpan == 2 && colSpan == 1 && n2x1CellCount >= max2x1Cell) ||
            (rowSpan == 2 && colSpan == 2 && n2x2CellCount >= max2x2Cell)) {
            continue;
        }

        // Update the count of current cells
        if (rowSpan == 1 && colSpan == 2) {
            n1x2CellCount++;
        }
        else if (rowSpan == 2 && colSpan == 1) {
            n2x1CellCount++;
        }
        else if (rowSpan == 2 && colSpan == 2) {
            n2x2CellCount++;
        }

        // Implement the merging
        for (int r = 0; r < rowSpan; ++r) {
            for (int c = 0; c < colSpan; ++c) {
                if (r == 0 && c == 0) {
                    bigCells[thisRow][thisCol] = glm::vec2(rowSpan, colSpan);
                } else {
                    bigCells[thisRow + r][thisCol + c] = glm::vec2(-1, -1);
                }
            }
        }
    }
}

void ofApp::offsetCells() { 
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            // Generate random offsets within a desired range
            offsetFactors[row][col].x = ofRandom(-(offsetFactorX), offsetFactorX);
            offsetFactors[row][col].y = ofRandom(-(offsetFactorY), offsetFactorY);
        }
    }
}

void ofApp::draw() { 
    ofPushMatrix();
    // Apply transformations to flip rendering horizontally
    if (flipImage) {
        ofTranslate(outputWidth, 0); // Move the origin to the mirrored position
        ofScale(-1, 1); // Flip the X-axis
    }
    
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            if (bigCells[row][col].x == -1 && bigCells[row][col].y == -1) {
                continue; 
            }

            rowSpan = 1;
            colSpan = 1;

            if (bigCells[row][col].x > 1 || bigCells[row][col].y > 1) {
                rowSpan = bigCells[row][col].x;
                colSpan = bigCells[row][col].y;
            }

            cellDrawWidth = cellWidth * colSpan;
            cellDrawHeight = cellHeight * rowSpan;

            cellDrawAspectRatio = cellDrawWidth / cellDrawHeight; 

            nthColX = cellWidth * col;
            nthRowY = cellHeight * row;

            // focusWidth = (rampy * ((imageWidth * scaleFactor) - 1)) + 1; // Decide scale factor when personDetected
            // focusHeight = (rampy * (((imageWidth / cellDrawAspectRatio) * scaleFactor) - 1)) + 1; // Decide scale factor when personDetected

            if (cellDrawAspectRatio >= 1) {
                focusWidth = (rampy * (imageWidth * scaleFactor) - 1 ) + 1;
                focusHeight = focusWidth / cellDrawAspectRatio; 
            } else {
                focusHeight = (rampy * (imageHeight * scaleFactor) - 1) + 1; 
                focusWidth = focusHeight * cellDrawAspectRatio;
            }

            offsetX = cellWidth * offsetFactors[row][col].x;
            offsetY = cellHeight * offsetFactors[row][col].y;
            
            minNthFocusX = nthColX + (cellWidth / 2); 
            minNthFocusY = nthRowY + (cellHeight / 2);

            maxNthFocusX = (nthColX - ((focusWidth - cellWidth) / 2) + ((focusWidth * currentFocus.x) - (focusWidth / 2))) + offsetX;
            maxNthFocusY = (nthRowY - ((focusHeight - cellHeight) / 2) + ((focusHeight * currentFocus.y) - (focusHeight / 2))) + offsetY;

            nthFocusX = (rampy * (maxNthFocusX - minNthFocusX)) + minNthFocusX;
            nthFocusY = (rampy * (maxNthFocusY - minNthFocusY)) + minNthFocusY;

            nthFocusX = ofClamp(nthFocusX, 0, imageWidth - focusWidth);
            nthFocusY = ofClamp(nthFocusY, 0, imageHeight - focusHeight);

            image.drawSubsection(nthColX, nthRowY, cellDrawWidth, cellDrawHeight, nthFocusX, nthFocusY, focusWidth, focusHeight);
            
            if (cellOutline == 1) {
                ofNoFill();
                ofSetColor(255, 255, 255);
                ofSetLineWidth(1);
                ofDrawRectangle(nthColX, nthRowY, cellDrawWidth, cellDrawHeight);
                ofSetColor(255, 255, 255);
            } else if (cellOutline == 2) {
                ofNoFill();
                ofSetColor(0, 0, 0);
                ofSetLineWidth(1);
                ofDrawRectangle(nthColX, nthRowY, cellDrawWidth, cellDrawHeight);
                ofSetColor(255, 255, 255);
            }
        }
    }
    ofPopMatrix();

    if (showGui) {
        gui.draw();
    }
}

void ofApp::keyPressed(int key) {
    if (key == 'g' || key == 'G') {
        showGui = !showGui; // Toggle GUI visibility
    }
}

void ofApp::exit() {
    if (grabber.isInitialized()) {
        grabber.close();
    }

    gui.clear();

    if (futureResult.valid()) {
        futureResult.get(); // Ensuring that the task is completed before exit
    }

    trackedPeople.clear();
}
