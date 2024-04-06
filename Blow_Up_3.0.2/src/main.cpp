#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {
	ofSetupOpenGL(1024, 768, OF_FULLSCREEN);
	ofRunApp(new ofApp()); // Start the application

	// //Use ofGLFWWindowSettings for more options like multi-monitor fullscreen
	// ofGLWindowSettings settings;
	// settings.setSize(1024, 768);
	// settings.windowMode = OF_WINDOW; //can also be OF_FULLSCREEN

	// auto window = ofCreateWindow(settings);

	// ofRunApp(window, make_shared<ofApp>());
	// ofRunMainLoop();

}

