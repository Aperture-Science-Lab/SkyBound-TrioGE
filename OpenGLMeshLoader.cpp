#include <stdlib.h>
#include <stdio.h>
#include "TextureBuilder.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include "GameManager.h"
#include "PlaneSelectionLevel.h"
#include "OptionsMenu.h"
#include "Level1.h"
#include "Level2.h"
#include <Vector3f.h>
#include <glut.h>

int lastTime = 0;

int WIDTH = 1280;
int HEIGHT = 720;

char title[] = "SkyBound - Flight Simulator";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 1000;

//=======================================================================
// Lighting Configuration Function
//=======================================================================
void InitLightSource()
{
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	GLfloat ambient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

	GLfloat diffuse[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);

	GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

	GLfloat light_position[] = { 0.0f, 10.0f, 0.0f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
}

//=======================================================================
// Material Configuration Function
//=======================================================================
void InitMaterial()
{
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	GLfloat specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular);

	GLfloat shininess[] = { 96.0f };
	glMaterialfv(GL_FRONT, GL_SHININESS, shininess);
}

//=======================================================================
// OpengGL Configuration Function
//=======================================================================
void myInit(void)
{
	glClearColor(0.35, 0.45, 0.65, 1.0);  // Sky blue default

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovy, aspectRatio, zNear, zFar);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	InitLightSource();
	InitMaterial();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
}

//=======================================================================
// Display Function
//=======================================================================
void myDisplay(void)
{
	// Delegate rendering to GameManager
	GameManager::getInstance().render();
	glFlush();
    glutSwapBuffers();
}

//=======================================================================
// Animation / Physics Function
//=======================================================================
void Anim() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    
    // Update through GameManager
    GameManager::getInstance().update(deltaTime);

    glutPostRedisplay();
}

//=======================================================================
// Keyboard Function
//=======================================================================
void myKeyboard(unsigned char button, int x, int y)
{
    // Forward input to GameManager
    GameManager::getInstance().handleKeyboard(button, true);
}

void myKeyboardUp(unsigned char button, int x, int y) {
    // Forward input to GameManager
    GameManager::getInstance().handleKeyboardUp(button);
}

void mySpecial(int key, int x, int y) {
    // Handle special keys (arrow keys) for plane selection
    Level* currentLevel = GameManager::getInstance().getCurrentLevel();
    PlaneSelectionLevel* planeSelect = dynamic_cast<PlaneSelectionLevel*>(currentLevel);
    if (planeSelect) {
        planeSelect->handleSpecialKeys(key, true);
    }
}

void mySpecialUp(int key, int x, int y) {
    // Handle special key release
    Level* currentLevel = GameManager::getInstance().getCurrentLevel();
    PlaneSelectionLevel* planeSelect = dynamic_cast<PlaneSelectionLevel*>(currentLevel);
    if (planeSelect) {
        planeSelect->handleSpecialKeys(key, false);
    }
}

//=======================================================================
// Motion Function
//=======================================================================
void myMotion(int x, int y)
{
    // Forward mouse input to GameManager
    GameManager::getInstance().handleMouse(x, y);
}

//=======================================================================
// Reshape Function
//=======================================================================
void myReshape(int w, int h)
{
	if (h == 0) h = 1;
	WIDTH = w;
	HEIGHT = h;

	glViewport(0, 0, w, h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovy, (GLdouble)WIDTH / (GLdouble)HEIGHT, zNear, zFar);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

//=======================================================================
// Main Function
//=======================================================================
int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutInitWindowPosition(100, 100);
	glutCreateWindow(title);

	glutDisplayFunc(myDisplay);
	glutReshapeFunc(myReshape);
    
    glutKeyboardFunc(myKeyboard);
    glutKeyboardUpFunc(myKeyboardUp);
    glutSpecialFunc(mySpecial);
    glutSpecialUpFunc(mySpecialUp);
    
    glutPassiveMotionFunc(myMotion);
    glutMotionFunc(myMotion);
    glutIdleFunc(Anim);

    myInit();
    
    // Initialize GameManager and register levels
    PlaneSelectionLevel* planeSelectLevel = new PlaneSelectionLevel();
    Level1* carrierLevel = new Level1();
    Level2* flightLevel = new Level2();
    OptionsMenu* optionsMenu = new OptionsMenu();
    
    GameManager::getInstance().registerLevel("planeselect", planeSelectLevel);
    GameManager::getInstance().registerLevel("level1", carrierLevel);
    GameManager::getInstance().registerLevel("level2", flightLevel);
    GameManager::getInstance().registerLevel("options", optionsMenu);
    
    // Initialize all levels
    planeSelectLevel->init();
    carrierLevel->init();
    flightLevel->init();
    optionsMenu->init();
    
    // Start with plane selection screen
    GameManager::getInstance().switchToLevel("planeselect");
    lastTime = glutGet(GLUT_ELAPSED_TIME);

	glutMainLoop();
    
    // Cleanup
    GameManager::getInstance().cleanup();
    
    return 0;
}