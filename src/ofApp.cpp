#include "ofApp.h"

void ofApp::setup(){
    ofSetFrameRate(60); // Set rendering framerate to 60fps
    
    // Setup GUI
    showGui = true;
    gui.setup();
    gui.setSize(540, 540);
    gui.setDefaultWidth(500); // Slightly less than panel width to fit nicely
    gui.setDefaultHeight(45); // Taller elements for better interaction
    gui.add(setupWebcamButton.setup("Setup Webcam"));
    gui.add(webcamWidth.setup("Width", 1920, 1920, 3840));
    gui.add(webcamHeight.setup("Height", 1080, 1080, 2160));
    gui.add(timerMedian.setup("Timer Median", 2000, 500, 3000));
    gui.add(timerUncertainty.setup("Timer Uncertainty", 500, 0, 1500));
    gui.add(morphInDuration.setup("Morph In Duration", 2000, 1000, 5000));
    gui.add(morphOutDuration.setup("Morph Out Duration", 2000, 1000, 5000));
    gui.add(untilMorphIn.setup("Until Morph In", 2000, 1000, 5000));
    gui.add(untilMorphOut.setup("Until Morph Out", 1000, 500, 3000));
    gui.add(mode2OffsetRange.setup("Mode 2 Offset Factor", 0.1, 0.0, 1.0));
    gui.add(detectionZoomFactor.setup("Detection Zoom Factor", 1.2, 1.0, 5.0));
    gui.add(bigCellZoomFactor.setup("Big Cell Zoom Factor", 0.1, 0.0, 1.0));
    setupWebcamButton.addListener(this, &ofApp::setupWebcam);

    setupWebcam();

    // Initialize YOLO model
    if(!yolo.setup("model", "classes.txt")) {
        ofLogError() << "Failed to setup YOLO model!";
        std::exit(EXIT_FAILURE);
    }
    yolo.setNormalize(true); // Normalize object bounding box coordinates
    
    isObjectDetected = false;
    mode = 1;

    // Initialize timer
    changeTime = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);

    // Initial grid dimensions
    updateGridDimensions();
}

void ofApp::setupWebcam(){
    webcam.setDesiredFrameRate(30); // Set webcam framerate to 30fps
    webcam.initGrabber(webcamWidth, webcamHeight);
}

void ofApp::processFrame(ofPixels pixels){

    pixels.resize(960, 540); // Adjust resizing directly on ofPixels
    yolo.setInput(pixels);
    yolo.update();

    // Retrieve and log detected objects
    auto detectedObjects = yolo.getObjects();
    for(auto& object : detectedObjects){
        std::string logMessage = "Detected: " + object.ident.text + " with confidence: " + ofToString(object.confidence);
        ofLogNotice("Object Detection") << logMessage;

        // Example additional action: draw bounding boxes or labels on the display based on object.bbox
    }
}

void ofApp::updateGridDimensions(){
    float prob = ofRandom(1.0);
    if(prob < maxGridProbability){
        gridRows = gridCols = 256; // Maximum grid size with given probability
    } else {
        gridRows = ofRandom(1, 256);
        gridCols = ofRandom(1, 256);
    }
    
    // Log the new grid dimensions
    // ofLogNotice() << "Updated Grid Dimensions: Rows = " << gridRows << ", Cols = " << gridCols;
}


void ofApp::update(){
    static int frameCount = 0; 
    int processEveryNthFrame = 3; 
    webcam.update();

    if(webcam.isFrameNew()){
        // Skip frames to reduce the frame rate
        if(++frameCount % processEveryNthFrame == 0){
            // Reset the frame counter in a safe range to avoid overflow
            frameCount = 0;

            // Process the frame
            if(futureResult.valid() ? futureResult.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready : true){
                futureResult = std::async(std::launch::async, &ofApp::processFrame, this, webcam.getPixels());
            }
        }
    }

    // Check if it's time to update grid dimensions
    if(ofGetElapsedTimeMillis() > changeTime){
        updateGridDimensions();
        changeTime = ofGetElapsedTimeMillis() + timerMedian + ofRandom(-timerUncertainty, timerUncertainty);
    }
}

void ofApp::displayGrid(){
    ofPixels & pixels = webcam.getPixels();
    pixels.resize(ofGetWidth(), ofGetHeight()); // Resize webcam input to match window size

    float cellWidth = ofGetWidth() / static_cast<float>(gridCols);
    float cellHeight = ofGetHeight() / static_cast<float>(gridRows);

    // Log calculated cellWidth and cellHeight
    // ofLogNotice() << "Cell Dimensions: Width = " << cellWidth << ", Height = " << cellHeight;

    // for(int row = 0; row < gridRows; ++row){
    //     for(int col = 0; col < gridCols; ++col){
    //         // Calculate the average color of the current cell
    //         ofColor avgColor = getAverageColor(pixels, col * cellWidth, row * cellHeight, cellWidth, cellHeight);
    //         ofSetColor(avgColor);
    //         // Draw the cell at the appropriate location with the calculated average color
    //         ofDrawRectangle(col * cellWidth, row * cellHeight, cellWidth, cellHeight);
    //     }
    // }

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

// ofColor ofApp::getAverageColor(ofPixels & pixels, int x, int y, int width, int height){
//     long long totalR = 0, totalG = 0, totalB = 0;
//     int count = 0;
//     for(int i = x; i < x + width; i+=4){
//         for(int j = y; j < y + height; j+=4){
//             ofColor c = pixels.getColor(i, j);
//             totalR += c.r;
//             totalG += c.g;
//             totalB += c.b;
//             ++count;
//         }
//     }
//     return ofColor(totalR / count, totalG / count, totalB / count);
// }

void ofApp::draw(){
    ofSetFullscreen(true);

    displayGrid();
    // Gui drawing

    if(showGui){
        gui.draw();
    }
}

void ofApp::keyPressed(int key){
    if(key == 'g' || key == 'G'){
        showGui = !showGui; // Toggle GUI visibility
    }
}

int ofApp::roundSliderValue(int currentValue, int increment){
    int remainder = currentValue % increment;
    if (remainder != 0) {
        int newValue = remainder >= (increment / 2) ? (currentValue + (increment - remainder)) : (currentValue - remainder);
        return newValue;
    }
    return currentValue;
}