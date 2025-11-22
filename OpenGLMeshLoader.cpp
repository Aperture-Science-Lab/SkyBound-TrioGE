#include <stdlib.h>
#include <stdio.h>
#include "TextureBuilder.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include "FlightController.h"
#include <Vector3f.h>
#include <glut.h>

FlightController* flightSim;
int lastTime = 0;

int WIDTH = 1280;
int HEIGHT = 720;

GLuint tex;
char title[] = "3D Model Loader Sample";

// 3D Projection Options
GLdouble fovy = 45.0;
GLdouble aspectRatio = (GLdouble)WIDTH / (GLdouble)HEIGHT;
GLdouble zNear = 0.1;
GLdouble zFar = 1000;



Vector3f Eye(20, 5, 20);
Vector3f At(0, 0, 0);
Vector3f Up(0, 1, 0);

int cameraZoom = 0;

// Model Variables
Model_3DS model_house;
Model_3DS model_tree;

// Textures
GLTexture tex_ground;

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
	glClearColor(0.0, 0.0, 0.0, 0.0);

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
// Render Ground Function
//=======================================================================
void RenderGround()
{
	glDisable(GL_LIGHTING);
	glColor3f(0.6f, 0.6f, 0.6f);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, tex_ground.texture[0]);
    
    // Ensure texture repeats
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glPushMatrix();
    
    // Infinite Ground Logic
    float px = 0, pz = 0;
    if (flightSim) {
        px = flightSim->player.position.x;
        pz = flightSim->player.position.z;
    }
    
    // Move ground with player
    glTranslatef(px, 0, pz);
    
    float size = 2000.0f; // Larger than zFar
    float texScale = 0.05f; // Adjust for texture density

	glBegin(GL_QUADS);
	glNormal3f(0, 1, 0);
    
    // Calculate texture coordinates based on world position to simulate movement
	glTexCoord2f((px - size) * texScale, (pz - size) * texScale); 
    glVertex3f(-size, 0, -size);
    
	glTexCoord2f((px + size) * texScale, (pz - size) * texScale); 
    glVertex3f(size, 0, -size);
    
	glTexCoord2f((px + size) * texScale, (pz + size) * texScale); 
    glVertex3f(size, 0, size);
    
	glTexCoord2f((px - size) * texScale, (pz + size) * texScale); 
    glVertex3f(-size, 0, size);
    
	glEnd();
	glPopMatrix();

	glEnable(GL_LIGHTING);
	glColor3f(1, 1, 1);
}

//=======================================================================
// Display Function
//=======================================================================
void myDisplay(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

    // Camera Setup from FlightController
    if (flightSim) {
        flightSim->setupCamera();
    }

	// Sky box
	glPushMatrix();
	GLUquadricObj* qobj;
	qobj = gluNewQuadric();
	
    // Move skybox with camera to make it appear infinite
    if (flightSim) {
        glTranslatef(flightSim->player.position.x, flightSim->player.position.y, flightSim->player.position.z);
    }

	glRotated(90, 1, 0, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	gluQuadricTexture(qobj, true);
	gluQuadricNormals(qobj, GL_SMOOTH);
    
    // Disable depth write so it's always background
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING); // Disable lighting for skybox
	gluSphere(qobj, 800, 100, 100); 
    glEnable(GL_LIGHTING);  // Re-enable lighting
    glDepthMask(GL_TRUE);
    
	gluDeleteQuadric(qobj);
	glPopMatrix();

	GLfloat lightIntensity[] = { 0.7f, 0.7f, 0.7f, 1.0f };
	GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);

	RenderGround();

    // Draw Plane
    if (flightSim) {
        flightSim->drawPlane();
    }

	// Draw Tree Model
	glPushMatrix();
	glTranslatef(10, 0, 0);
	glScalef(0.7f, 0.7f, 0.7f);
	model_tree.Draw();
	glPopMatrix();

	// Draw House Model
	glPushMatrix();
	glRotatef(90.f, 1, 0, 0);
	model_house.Draw();
	glPopMatrix();

	glutSwapBuffers();
}

//=======================================================================
// Animation / Physics Function
//=======================================================================
void Anim() {
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;
    if (deltaTime > 0.1f) deltaTime = 0.1f;

    if (flightSim) {
        flightSim->update(deltaTime);
    }
    
    // Center mouse
    if (WIDTH > 0 && HEIGHT > 0) {
        int centerX = WIDTH / 2;
        int centerY = HEIGHT / 2;
        glutWarpPointer(centerX, centerY);
    }

    glutPostRedisplay();
}

//=======================================================================
// Keyboard Function
//=======================================================================
void myKeyboard(unsigned char button, int x, int y)
{
    if (button == 27) exit(0);
    
    if (flightSim) {
        flightSim->handleInput(button, true);
    }
}

void myKeyboardUp(unsigned char button, int x, int y) {
    if (flightSim) {
        flightSim->handleInput(button, false);
    }
}

//=======================================================================
// Motion Function
//=======================================================================
void myMotion(int x, int y)
{
    int centerX = WIDTH / 2;
    int centerY = HEIGHT / 2;
    if (flightSim) {
        flightSim->handleMouse(x, y, centerX, centerY);
    }
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
// Assets Loading Function
//=======================================================================
void LoadAssets()
{
	model_house.Load("Models/house/house.3DS");
	model_tree.Load("Models/tree/Tree1.3ds");
	tex_ground.Load("Textures/ground.bmp");
	loadBMP(&tex, "Textures/blu-sky-3.bmp", true);
    
    if (flightSim) {
        flightSim->loadModel("models/plane/mitsubishi_a6m2_zero_model_11.3ds");
    }
}

//=======================================================================
// Main Function
//=======================================================================
int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(WIDTH, HEIGHT);
	glutInitWindowPosition(100, 150);
	glutCreateWindow(title);

	glutDisplayFunc(myDisplay);
	glutReshapeFunc(myReshape);
    
    glutKeyboardFunc(myKeyboard);
    glutKeyboardUpFunc(myKeyboardUp);
    
    glutPassiveMotionFunc(myMotion);
    glutMotionFunc(myMotion);
    glutIdleFunc(Anim);
    
    glutSetCursor(GLUT_CURSOR_NONE);

	myInit();
    
    flightSim = new FlightController();
	LoadAssets();

	glutMainLoop();
    return 0;
}