#include "TextureBuilder.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include "FlightController.h"
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
GLdouble zFar = 100;

class Vector
{
public:
	GLdouble x, y, z;
	Vector() : x(0), y(0), z(0) {}
	Vector(GLdouble _x, GLdouble _y, GLdouble _z) : x(_x), y(_y), z(_z) {}
	void operator +=(float value)
	{
		x += value;
		y += value;
		z += value;
	}
};

Vector Eye(20, 5, 20);
Vector At(0, 0, 0);
Vector Up(0, 1, 0);

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

	glPushMatrix();
	glBegin(GL_QUADS);
	glNormal3f(0, 1, 0);
	glTexCoord2f(0, 0); glVertex3f(-20, 0, -20);
	glTexCoord2f(5, 0); glVertex3f(20, 0, -20);
	glTexCoord2f(5, 5); glVertex3f(20, 0, 20);
	glTexCoord2f(0, 5); glVertex3f(-20, 0, 20);
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

	// Sky box
	glPushMatrix();
	GLUquadricObj* qobj;
	qobj = gluNewQuadric();
	glTranslated(50, 0, 0);
	glRotated(90, 1, 0, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	gluQuadricTexture(qobj, true);
	gluQuadricNormals(qobj, GL_SMOOTH);
	gluSphere(qobj, 100, 100, 100);
	gluDeleteQuadric(qobj);
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
	LoadAssets();
    
    flightSim = new FlightController();

	glutMainLoop();
    return 0;
}