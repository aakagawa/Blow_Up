#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60);

    // Scale camera to display 
    grabber.setup(3840, 2160); // 4k if available
    grabber.setDesiredFrameRate(30); // Set webcam framerate to 30fps
    grabber.update();

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

    showGui = true;
    gui.setup();
    gui.setPosition(0, 0);
    gui.setSize(outputWidth * 0.375, 760);
    gui.setDefaultWidth(720);
    gui.setDefaultHeight(20);

    // Display settings
    gui.add(displaySettings.setup("DISPLAY SETTINGS", ""));
    gui.add(flipImage.setup("Flip image", true)); 
    
    // Grid settings
    gui.add(gridSettings.setup("GRID SETTINGS", ""));
    gui.add(outlineR.setup("Cell outline R", 0, 0, 255));
    gui.add(outlineG.setup("Cell outline G", 0, 0, 255));
    gui.add(outlineB.setup("Cell outline B", 0, 0, 255));
    gui.add(cellOutlineWidth.setup("Cell outline width", 0, 0, 10));
    gui.add(minRow.setup("Minimum num of rows", 2, 2, 256)); 
    gui.add(minCol.setup("Minimum num of columns", 2, 2, 256)); 
    gui.add(maxRow.setup("Maximum num of rows", 64, 2, 256));  
    gui.add(maxCol.setup("Maximum num of columns", 64, 2, 256)); 

    // Animation settings
    gui.add(animationSettings.setup("ANIMATION SETTINGS", ""));
    gui.add(gridUpdateInterval.setup("Grid update interval (default: 5000)", 5000, 5000, 600000)); 
    gui.add(gridUpdateIntervalUncertainty.setup("Grid update interval uncertainty (default: 20)", 20, 0, 100)); 
    gui.add(maxGridProbability.setup("Probability of maximum grid ", 20, 0, 100)); 
    gui.add(max1x2Cell.setup("Max num of 1x2 cells", 2, 0, 10));  
    gui.add(max2x1Cell.setup("Max num of 2x1 cells", 2, 0, 10)); 
    gui.add(max2x2Cell.setup("Max num of 2x2 cells", 2, 0, 10)); 
    gui.add(bigCellScale.setup("Big cell scale", 2, 1, 10));
    gui.add(zoomFactor.setup("Zoom/scale factor (blow up)", 4, 2, 20));
    gui.add(offsetFactorX.setup("Horizontal offset factor (blow up)", 0.2, 0.0, 1.0));
    gui.add(offsetFactorY.setup("Vertical offset factor (blow up)", 0.05, 0.0, 1.0));
    gui.add(transDuration.setup("Transition duration", 1000, 500, 10000)); 
    gui.add(focusFollowSpeed.setup("Focus follow speed", 0.05, 0.01, 0.5));

    // Detection settings
    gui.add(detectionSettings.setup("DETECTION SETTINGS", ""));
    gui.add(detectionAreaTopLeftX.setup("Detection area top left x", 0, 0, inputWidth)); 
    gui.add(detectionAreaTopLeftY.setup("Detection area top left y", 0, 0, inputHeight)); 
    gui.add(detectionAreaBottomRightX.setup("Detection area bottom right x (default: maximum width)", inputWidth, 0, inputWidth)); 
    gui.add(detectionAreaBottomRightY.setup("Detection area bottom right y (default: maximum height)", inputHeight, 0, inputHeight)); 

    gui.add(minConfidence.setup("Required confidence", 0.5, 0.0, 0.9));
    gui.add(minSize.setup("Required size", 0.2, 0.0, 0.9));
    gui.add(transInTimeout.setup("Transition in timeout (time until transition after detection)", 0, 0, 10000));
    gui.add(transOutTimeout.setup("Transition out timeout (time until transition after undetection)", 2500, 0, 10000)); 
    gui.add(inactiveTimeout.setup("Time until inactive person is considered un detected", 10000, 2000, 60000)); 

    // Advanced Settings 
    gui.add(advancedSettings.setup("ADVANCED SETTINGS", ""));
    gui.add(frameGrain.setup("Detection frameGrain", 2, 1, 6)); 
    gui.add(pixelsResize.setup("Detction pixelsResize", 0.75, 0.2, 1.0));
    gui.add(maxMovementThreshold.setup("maxMovementThreshold", 0.75, 0.5, 1.0));
    gui.add(minMovementThreshold.setup("minMovementThreshold", 0.05, 0.01, 0.2));
    gui.add(expiryTime.setup("expiryTime", 60000, 30000, 120000));

    // Load Saved Settings
    loadSettings();

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
    gridRefreshInterval = ofGetElapsedTimeMillis() + gridUpdateInterval + ofRandom(-(gridUpdateInterval * (gridUpdateIntervalUncertainty * 0.01)), (gridUpdateInterval * (gridUpdateIntervalUncertainty * 0.01)));

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
            rampy = ofLerp(0, 1, progress);
            if (progress == 0) {
                mergeCells();
                offsetCells();
            }
        } else {
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
            gridRefreshInterval = ofGetElapsedTimeMillis() + gridUpdateInterval + ofRandom(-(gridUpdateInterval * (gridUpdateIntervalUncertainty * 0.01)), (gridUpdateInterval * (gridUpdateIntervalUncertainty * 0.01)));
        }        
    }

    // Calculate position
    if (personDetected) { 
        currentFocus.x = ofLerp(currentFocus.x, targetFocus.x, focusFollowSpeed);
        currentFocus.y = ofLerp(currentFocus.y, targetFocus.y, focusFollowSpeed);
    }

    // Check if zoomFactor has changed
    if (zoomFactor != lastZoomFactor) {  
        scaleFactor = 1 / static_cast<float>(zoomFactor);
        lastZoomFactor = zoomFactor;
    }
    
    // Check if any of the detectionArea coordinates have changed 
    if (detectionAreaTopLeftX != lastDetectionAreaTopLeftX || detectionAreaTopLeftY != lastDetectionAreaTopLeftY || detectionAreaBottomRightX != lastDetectionAreaBottomRightX || detectionAreaBottomRightY != lastDetectionAreaBottomRightY) {
        detectionAreaWidth = detectionAreaBottomRightX - detectionAreaTopLeftX;
        detectionAreaHeight = detectionAreaBottomRightY - detectionAreaTopLeftY;

        detectionArea.set(detectionAreaTopLeftX, detectionAreaTopLeftY, detectionAreaWidth, detectionAreaHeight);
    }
}

void ofApp::processFrame(ofPixels pixels) {
    pixels.crop(detectionAreaTopLeftX, detectionAreaTopLeftY, detectionAreaWidth, detectionAreaHeight);
    
    pixels.resize(inputWidth * pixelsResize, inputHeight * pixelsResize);
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
                currentPosition = glm::vec2((object.bbox.x + object.bbox.width) * 0.5, (object.bbox.y + object.bbox.height) * 0.5);
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
}

void ofApp::updateGrid() {
    float prob = ofRandom(1.0);
    if (prob < maxGridProbability * 0.01) { // Maximum grid size with given probability 
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

        rowSpan *= bigCellScale;
        colSpan *= bigCellScale;
        
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

            if (cellDrawAspectRatio >= 1) {
                focusWidth = (rampy * (imageWidth * scaleFactor) - 1 ) + 1;
                focusHeight = focusWidth / cellDrawAspectRatio; 
            } else {
                focusHeight = (rampy * (imageHeight * scaleFactor) - 1) + 1; 
                focusWidth = focusHeight * cellDrawAspectRatio;
            }

            offsetX = imageWidth * offsetFactors[row][col].x;
            offsetY = imageHeight * offsetFactors[row][col].y;
            
            minNthFocusX = nthColX + (cellWidth * 0.5); 
            minNthFocusY = nthRowY + (cellHeight * 0.5);

            maxNthFocusX = (nthColX - ((focusWidth - cellWidth) * 0.5) + ((focusWidth * currentFocus.x) - (focusWidth * 0.5))) + offsetX;
            maxNthFocusY = (nthRowY - ((focusHeight - cellHeight) * 0.5) + ((focusHeight * currentFocus.y) - (focusHeight * 0.5))) + offsetY;

            nthFocusX = (rampy * (maxNthFocusX - minNthFocusX)) + minNthFocusX;
            nthFocusY = (rampy * (maxNthFocusY - minNthFocusY)) + minNthFocusY;

            nthFocusX = ofClamp(nthFocusX, 0, imageWidth - focusWidth);
            nthFocusY = ofClamp(nthFocusY, 0, imageHeight - focusHeight);

            image.drawSubsection(nthColX, nthRowY, cellDrawWidth, cellDrawHeight, nthFocusX, nthFocusY, focusWidth, focusHeight);
            
            if (cellOutlineWidth != 0) {
                ofNoFill();
                ofColor outlineColor(outlineR, outlineG, outlineB, 255);
                ofSetColor(outlineColor);
                ofSetLineWidth(cellOutlineWidth);
                ofDrawRectangle(nthColX, nthRowY, cellDrawWidth, cellDrawHeight);
                ofSetColor(255, 255, 255);
            }
        }
    }
    ofPopMatrix();

    if (showGui) {
        gui.draw();
    
        ofPushMatrix();
        ofTranslate(outputWidth * 0.375, 0);

        ofScale(cameraFOVscaleFactor, cameraFOVscaleFactor);
        grabber.draw(0, 0);

        ofSetColor(255, 0, 0);
        ofNoFill();
        ofDrawRectangle(detectionArea);
        ofPopMatrix();
        ofSetColor(255, 255, 255);
    }
}

void ofApp::keyPressed(int key) {
    if (key == 'g' || key == 'G') {
        showGui = !showGui; // Toggle GUI visibility
    }

    if (key == 'L' && ofGetKeyPressed(OF_KEY_SHIFT)) {
        loadDefaultSettings();
    }
}

void ofApp::saveSettings() {
    ofXml xml;
    auto settings = xml.appendChild("Settings");

    // Save display settings
    settings.appendChild("flipImage").set(flipImage ? "true" : "false");
    settings.appendChild("outlineR").set(outlineR.getParameter());
    settings.appendChild("outlineG").set(outlineG.getParameter());
    settings.appendChild("outlineB").set(outlineB.getParameter());
    settings.appendChild("cellOutlineWidth").set(cellOutlineWidth.getParameter());
    settings.appendChild("minRow").set(minRow.getParameter());
    settings.appendChild("minCol").set(minCol.getParameter());
    settings.appendChild("maxRow").set(maxRow.getParameter());
    settings.appendChild("maxCol").set(maxCol.getParameter());

    // Save animation settings
    settings.appendChild("gridUpdateInterval").set(gridUpdateInterval.getParameter());
    settings.appendChild("gridUpdateIntervalUncertainty").set(gridUpdateIntervalUncertainty.getParameter());
    settings.appendChild("maxGridProbability").set(maxGridProbability.getParameter());
    settings.appendChild("max1x2Cell").set(max1x2Cell.getParameter());
    settings.appendChild("max2x1Cell").set(max2x1Cell.getParameter());
    settings.appendChild("max2x2Cell").set(max2x2Cell.getParameter());
    settings.appendChild("bigCellScale").set(bigCellScale.getParameter());
    settings.appendChild("zoomFactor").set(zoomFactor.getParameter());
    settings.appendChild("offsetFactorX").set(offsetFactorX.getParameter());
    settings.appendChild("offsetFactorY").set(offsetFactorY.getParameter());
    settings.appendChild("transDuration").set(transDuration.getParameter());
    settings.appendChild("focusFollowSpeed").set(focusFollowSpeed.getParameter());

    // Save detection settings
    settings.appendChild("detectionAreaTopLeftX").set(detectionAreaTopLeftX.getParameter());
    settings.appendChild("detectionAreaTopLeftY").set(detectionAreaTopLeftY.getParameter());
    settings.appendChild("detectionAreaBottomRightX").set(detectionAreaBottomRightX.getParameter());
    settings.appendChild("detectionAreaBottomRightY").set(detectionAreaBottomRightY.getParameter());
    settings.appendChild("minConfidence").set(minConfidence.getParameter());
    settings.appendChild("minSize").set(minSize.getParameter());
    settings.appendChild("transInTimeout").set(transInTimeout.getParameter());
    settings.appendChild("transOutTimeout").set(transOutTimeout.getParameter());
    settings.appendChild("inactiveTimeout").set(inactiveTimeout.getParameter());

    // Save advanced settings
    settings.appendChild("frameGrain").set(frameGrain.getParameter());
    settings.appendChild("pixelsResize").set(pixelsResize.getParameter());
    settings.appendChild("maxMovementThreshold").set(maxMovementThreshold.getParameter());
    settings.appendChild("minMovementThreshold").set(minMovementThreshold.getParameter());
    settings.appendChild("trackedPeople.erase").set(expiryTime.getParameter());

    xml.save("settings.xml");
}

void ofApp::loadSettings() {
    ofXml xml;
    if(xml.load("settings.xml")) {
        auto settings = xml.getChild("Settings");

        flipImage = settings.getChild("flipImage").getBoolValue();
        outlineR = settings.getChild("outlineR").getIntValue();
        outlineG = settings.getChild("outlineG").getIntValue();
        outlineB = settings.getChild("outlineB").getIntValue();
        cellOutlineWidth = settings.getChild("cellOutlineWidth").getIntValue();
        minRow = settings.getChild("minRow").getIntValue();
        minCol = settings.getChild("minCol").getIntValue();
        maxRow = settings.getChild("maxRow").getIntValue();
        maxCol = settings.getChild("maxCol").getIntValue();

        gridUpdateInterval = settings.getChild("gridUpdateInterval").getIntValue();
        gridUpdateIntervalUncertainty = settings.getChild("gridUpdateIntervalUncertainty").getIntValue();
        maxGridProbability = settings.getChild("maxGridProbability").getIntValue();
        max1x2Cell = settings.getChild("max1x2Cell").getIntValue();
        max2x1Cell = settings.getChild("max2x1Cell").getIntValue();
        max2x2Cell = settings.getChild("max2x2Cell").getIntValue();
        bigCellScale = settings.getChild("bigCellScale").getIntValue();
        zoomFactor = settings.getChild("zoomFactor").getIntValue();
        offsetFactorX = settings.getChild("offsetFactorX").getFloatValue();
        offsetFactorY = settings.getChild("offsetFactorY").getFloatValue();
        transDuration = settings.getChild("transDuration").getIntValue();
        focusFollowSpeed = settings.getChild("focusFollowSpeed").getFloatValue();

        detectionAreaTopLeftX = settings.getChild("detectionAreaTopLeftX").getIntValue();
        detectionAreaTopLeftY = settings.getChild("detectionAreaTopLeftY").getIntValue();
        detectionAreaBottomRightX = settings.getChild("detectionAreaBottomRightX").getIntValue();
        detectionAreaBottomRightY = settings.getChild("detectionAreaBottomRightY").getIntValue();
        detectionAreaWidth = detectionAreaBottomRightX - detectionAreaTopLeftX; 
        detectionAreaHeight = detectionAreaBottomRightY - detectionAreaTopLeftY;

        detectionArea.set(detectionAreaTopLeftX, detectionAreaTopLeftY, detectionAreaWidth, detectionAreaHeight);        

        minConfidence = settings.getChild("minConfidence").getFloatValue();
        minSize = settings.getChild("minSize").getFloatValue();
        transInTimeout = settings.getChild("transInTimeout").getIntValue();
        transOutTimeout = settings.getChild("transOutTimeout").getIntValue();
        inactiveTimeout = settings.getChild("inactiveTimeout").getIntValue();

        frameGrain = settings.getChild("frameGrain").getIntValue();
        pixelsResize = settings.getChild("pixelsResize").getFloatValue();
        maxMovementThreshold = settings.getChild("maxMovementThreshold").getFloatValue();
        minMovementThreshold = settings.getChild("minMovementThreshold").getFloatValue();
        expiryTime = settings.getChild("trackedPeople.erase").getIntValue();
    }
}

void ofApp::loadDefaultSettings() {
    // Reset Display settings
    flipImage = true;  // Default state for image flipping

    // Reset Grid settings
    outlineR = 0;
    outlineG = 0;
    outlineB = 0;
    cellOutlineWidth = 0;
    minRow = 2;
    minCol = 2;
    maxRow = 64;
    maxCol = 64;

    // Reset Animation settings
    gridUpdateInterval = 5000;
    gridUpdateIntervalUncertainty = 20;
    maxGridProbability = 20;
    max1x2Cell = 2;
    max2x1Cell = 2;
    max2x2Cell = 2;
    bigCellScale = 2;
    zoomFactor = 4;
    offsetFactorX = 0.2;
    offsetFactorY = 0.05;
    transDuration = 1000;
    focusFollowSpeed = 0.05;

    // Reset Detection settings
    detectionAreaTopLeftX = 0;
    detectionAreaTopLeftY = 0;
    detectionAreaBottomRightX = inputWidth;
    detectionAreaBottomRightY = inputHeight;

    detectionArea.set(detectionAreaTopLeftX, detectionAreaTopLeftY, detectionAreaWidth, detectionAreaHeight);

    minConfidence = 0.5;
    minSize = 0.2;
    transInTimeout = 0;
    transOutTimeout = 2500;
    inactiveTimeout = 10000;

    // Reset Advanced Settings
    frameGrain = 2;
    pixelsResize = 0.75;
    maxMovementThreshold = 0.75;
    minMovementThreshold = 0.02;
    expiryTime = 60000;
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

    saveSettings();
}
