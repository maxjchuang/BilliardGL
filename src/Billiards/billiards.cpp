#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/freeglut.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "ObjLoader.h"
#include "particle.h"
#include "platform_audio.h"
#include "platform_time.h"
#include "resource_path.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define TABLE_OUT_WIDTH 153
#define TABLE_OUT_LENGTH 281
#define TABLE_IN_WIDTH 124.5
#define TABLE_IN_LENGTH 252
#define TABLE_HEIGHT 87
#define CUE_LENGTH 145
#define POCKET_RADIUS 8.5
#define ROOM_WIDTH 1000
#define ROOM_LENGTH 1000
#define ROOM_HEIGHT 400
#define BALL_RADIUS 5.715

#define BMP_Header_Length 54
#define PI 3.1415926
#define L0 GL_LIGHT0
#define L1 GL_LIGHT1

GLuint texture[10];
emitter *e[16];

//±‰¡ø…Í√˜
GLint  width = 1024, height = 768;
GLuint texGround, texWall, texWall1, texWall2, tecCeiling, texTableCloth, texTable, BZD, texCue, texgt, texhe;
int mx = 0, my = 0, i = 0, j = 0, k = 0, ballcnt = 16, BallInNum = 0, AimAt = 0, WaitHit = 0, Hit = 0;
int leftm = 0, rightm = 0;
bool TrackpadOrbit = false;
GLfloat rx, ry, rz, speed = 0;
static GLfloat kx = 0, ky = 0, kz = 0, zoom = 120;
static GLfloat anglex = -PI / 2, angley = PI / 3, nowanglex = 0, nowangley = 0, nowatx = 0, nowaty = 0;
static GLfloat M = 1, U = 0.2, T = 0.1, Radius = 5.715, G = -4;
static GLfloat at[6] = { 0, 200, -TABLE_IN_LENGTH / 4, 0, TABLE_HEIGHT + Radius, -TABLE_IN_LENGTH / 4 };
GLfloat m[16];
GLuint tableVertexVBO, cueVertexVBO, benchVertexVBO, wardVertexVBO, paint1VertexVBO;
GLuint textureIDtest[2];
GLuint textureCue[2], textureWard, texturePaint1, texturePaint2;
ObjLoader tableObj(billiardgl::objectPath("table.obj"));
ObjLoader cueObj(billiardgl::objectPath("cue.obj"));
ObjLoader benchObj(billiardgl::objectPath("bench.obj"));
ObjLoader wardObj(billiardgl::objectPath("wardrobe.obj"));

bool IsMoving = false;
bool transPerc = false;
bool Isrecroded = false;
float record_zoom;
float record_position[3];
int PlayerBall[2];// 0¥˙±Ì¥ø…´«Ú 1-7  1¥˙±Ìª®…´«Ú 9-15
int IsFirstInBall = 1;
int CurrPlayer = 0;
int NextPlayer = 0;
int IsIllegal = 0;
bool updated = false;
bool Hitted = false;
bool IsGameOver = false;
bool Fired[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
bool AllFired = false;
bool WindowedMode = true;

//∂®“Â«Úº∞Œª÷√ ∏¡øΩ·ππÃÂ
struct Point
{
	GLfloat x;
	GLfloat y;
	GLfloat z;
};
struct Ball
{
	struct Point p;
	struct Point v;
	struct Point a;
	GLfloat mv;
	GLfloat ma;
	int isIn;
	GLuint texture;
} Ball[16];

// ‘ÿ»ÎŒ∆¿Ì
int isPowerOfTwo(int n);
GLuint loadTexture(const char* file_name);
void initLoadTexture();
// ≥°æ∞ªÊ÷∆
void initBall();
void parseLaunchOptions(int argc, char* argv[]);
void initWindows(void);
void myReshape(int w, int h);
void initDecoration();
void myDisplay(void);
void set_camera(void);
void renderRoom(void);
void renderTable(void);
void renderBall(void);
void drawBall(GLfloat mybr, GLuint BALL);
void renderCue();
void renderRect();
void renderDecoration();
void myIdle(void);
void collideBalls(int j, int k);
void collideEdge(int j);
int isBallIn(int j);
void drawString();
void myString(float x, float y, void *font, const char* c);
void updatePlayer();
void initTable();
void initCue();
void setMaterial(Material *mat);
// π‚‘¥
void initLight();
//  Û±Íº¸≈Ã≤Ÿ◊˜
static void myKeyboard(unsigned char key, int x, int y);
static void mySpecialKeyboard(int key, int x, int y);
static void mySpecialKeyboard(int key, int x, int y)
{
	const GLfloat orbit_step = 0.08f;
	switch (key)
	{
	case GLUT_KEY_LEFT:
		anglex -= orbit_step;
		break;
	case GLUT_KEY_RIGHT:
		anglex += orbit_step;
		break;
	case GLUT_KEY_UP:
		angley -= orbit_step;
		break;
	case GLUT_KEY_DOWN:
		angley += orbit_step;
		break;
	default:
		break;
	}
	if (angley <= 0)
		angley = 0.1f;
	if (angley > PI / 2)
		angley = PI / 2;
}

static void myMouse(int mbutton, int mstate, int x, int y);
static void mouseMove(int x, int y);
void b_music();

particle* init_flame()
{
	float size = rand() % 90 * 0.02f;
	float speed[] = { float(rand() % 10 - 4) / 1600, float(rand() % 10 - 4) / 800, float(rand() % 10 - 4) / 1600 };
	float acc[] = { 1.0f*(rand() % 3 - 1) / 9000000,4.9 / 4000000 ,1.0f*(rand() % 3 - 1) / 9000000 };
	float angle[] = { static_cast<float>(rand() % 360), static_cast<float>(rand() % 360), static_cast<float>(rand() % 360) };
	particle* p = new particle(vec(size, size, size), vec(speed), vec(acc),
		vec(angle), rand() % 50 + 10, texture[2]);
	return p;
}


int main(int argc, char* argv[])
{
	parseLaunchOptions(argc, argv);
	std::thread t(b_music);
	t.detach();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_STENCIL);
	initWindows();
	initBall();//≥ı ºªØ«ÚµƒŒª÷√
	initLoadTexture();
	initCue();
	initTable();
	initDecoration();

	for (int i = 0; i < ballcnt; i++)
	{
		//		if(i==0)
		e[i] = new emitter(init_flame, 5000, -Radius + Ball[i].p.x, Radius + Ball[i].p.x, Ball[i].p.y, Ball[i].p.y, Ball[i].p.z, Ball[i].p.z);
	}

	glutDisplayFunc(&myDisplay);
	glutIdleFunc(&myIdle); //…Ë÷√¥∞ø⁄À¢–¬µƒªÿµ˜∫Ø ˝
	glutKeyboardFunc(myKeyboard); //…Ë÷√º¸≈Ãªÿµ˜∫Ø ˝
	glutSpecialFunc(mySpecialKeyboard);
	glutMouseFunc(myMouse); //…Ë÷√ Û±Í∆˜∞¥º¸ªÿµ˜∫Ø ˝
	glutMotionFunc(mouseMove); //ÔøΩÔøΩÔøΩÔøΩÔøΩÔøΩÔøΩÔøΩÔøΩÔøΩ∆∂ÔøΩÔøΩÿµÔøΩÔøΩÔøΩÔøΩÔøΩ
	glutReshapeFunc(myReshape);

	initLight(); // π‚’’ƒ£–Õ
	glutMainLoop();
	return 0;
}

// ≥ı ºªØ¥∞ø⁄
void parseLaunchOptions(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--windowed") == 0)
			WindowedMode = true;
		else if (strcmp(argv[i], "--fullscreen") == 0)
			WindowedMode = false;
	}
}

void initWindows(void)
{
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Billards");
	if (!WindowedMode)
		glutFullScreen();
}

void myReshape(int w, int h)
{
	width = w > 0 ? w : WINDOW_WIDTH;
	height = h > 0 ? h : WINDOW_HEIGHT;
	glViewport(0, 0, width, height);
}
// ≥ı ºªØ«ÚŒª÷√
void initBall()
{
	Point s;
	s.x = 0;
	s.z = TABLE_IN_LENGTH / 4;
	s.y = TABLE_HEIGHT + BALL_RADIUS;
	float y_dis = sqrt(3)*BALL_RADIUS;
	float x_dis = BALL_RADIUS;
	//Ball[0] is the white ball
	Ball[0].p.x = s.x; Ball[0].p.z = -s.z; Ball[0].p.y = s.y;
	//first row
	Ball[1].p.x = s.x; Ball[1].p.z = s.z; Ball[1].p.y = s.y;
	//second row
	Ball[2].p.x = s.x - x_dis; Ball[2].p.z = s.z + y_dis; Ball[2].p.y = s.y;
	Ball[3].p.x = s.x + x_dis; Ball[3].p.z = s.z + y_dis; Ball[3].p.y = s.y;

	Ball[4].p.x = s.x - 2 * x_dis; Ball[4].p.z = s.z + 2 * y_dis; Ball[4].p.y = s.y;
	Ball[8].p.x = s.x; Ball[8].p.z = s.z + 2 * y_dis; Ball[8].p.y = s.y;
	Ball[6].p.x = s.x + 2 * x_dis; Ball[6].p.z = s.z + 2 * y_dis; Ball[6].p.y = s.y;

	Ball[7].p.x = s.x - 3 * x_dis; Ball[7].p.z = s.z + 3 * y_dis; Ball[7].p.y = s.y;
	Ball[5].p.x = s.x - x_dis; Ball[5].p.z = s.z + 3 * y_dis; Ball[5].p.y = s.y;
	Ball[9].p.x = s.x + x_dis; Ball[9].p.z = s.z + 3 * y_dis; Ball[9].p.y = s.y;
	Ball[10].p.x = s.x + 3 * x_dis; Ball[10].p.z = s.z + 3 * y_dis; Ball[10].p.y = s.y;

	Ball[11].p.x = s.x - 4 * x_dis; Ball[11].p.z = s.z + 4 * y_dis; Ball[11].p.y = s.y;
	Ball[12].p.x = s.x - 2 * x_dis; Ball[12].p.z = s.z + 4 * y_dis; Ball[12].p.y = s.y;
	Ball[13].p.x = s.x; Ball[13].p.z = s.z + 4 * y_dis; Ball[13].p.y = s.y;
	Ball[14].p.x = s.x + 2 * x_dis; Ball[14].p.z = s.z + 4 * y_dis; Ball[14].p.y = s.y;
	Ball[15].p.x = s.x + 4 * x_dis; Ball[15].p.z = s.z + 4 * y_dis; Ball[15].p.y = s.y;

	for (i = 0; i < ballcnt; i++)
	{
		Ball[i].v.x = 0; Ball[i].v.z = 0; Ball[i].v.y = 0;
		Ball[i].a.x = 0; Ball[i].a.z = 0; Ball[i].a.y = 0;
		Ball[i].isIn = 0;
		Ball[i].mv = 0;
		Ball[i].ma = 0;
	}
}

void initTable() {
	glEnable(GL_TEXTURE_2D);
	const std::string tableTexture0 = billiardgl::texturePath(tableObj.materials[0]->texture);
	const std::string tableTexture1 = billiardgl::texturePath(tableObj.materials[1]->texture);
	textureIDtest[0] = loadTexture(tableTexture0.c_str());
	textureIDtest[1] = loadTexture(tableTexture1.c_str());

	glewInit();
	GLfloat *tableVertex;
	GLfloat *tableNormal;
	tableNormal = new GLfloat[tableObj.vertices.size() * 3];
	tableVertex = new GLfloat[tableObj.vertices.size() * 3];
	GLfloat *tableTexture = new GLfloat[tableObj.vertices.size() * 3];
	for (int i = 0; i < tableObj.vertices.size(); i++) {
		tableVertex[i * 3] = tableObj.vertices[i].position.x;
		tableVertex[i * 3 + 1] = tableObj.vertices[i].position.y;
		tableVertex[i * 3 + 2] = tableObj.vertices[i].position.z;
		tableNormal[i * 3] = tableObj.vertices[i].normal.x;
		tableNormal[i * 3 + 1] = tableObj.vertices[i].normal.y;
		tableNormal[i * 3 + 2] = tableObj.vertices[i].normal.z;
		tableTexture[i * 3] = tableObj.vertices[i].texture.x;
		tableTexture[i * 3 + 1] = tableObj.vertices[i].texture.y;
		tableTexture[i * 3 + 2] = tableObj.vertices[i].texture.z;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	size_t dataSize = sizeof(GLfloat) * tableObj.vertices.size() * 3;
	glGenBuffersARB(1, &tableVertexVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, tableVertexVBO);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, tableVertex);                             // copy vertices starting from 0 offest
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, tableNormal);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, tableTexture);
}
void initDecoration() {
	glEnable(GL_TEXTURE_2D);
	glewInit();
	GLfloat *benchVertex;
	GLfloat *benchNormal;
	benchNormal = new GLfloat[benchObj.vertices.size() * 3];
	benchVertex = new GLfloat[benchObj.vertices.size() * 3];
	GLfloat *benchTexture = new GLfloat[benchObj.vertices.size() * 3];
	for (int i = 0; i < benchObj.vertices.size(); i++) {
		benchVertex[i * 3] = benchObj.vertices[i].position.x / 3;
		benchVertex[i * 3 + 1] = benchObj.vertices[i].position.y / 3;
		benchVertex[i * 3 + 2] = benchObj.vertices[i].position.z / 3;
		benchNormal[i * 3] = benchObj.vertices[i].normal.x;
		benchNormal[i * 3 + 1] = benchObj.vertices[i].normal.y;
		benchNormal[i * 3 + 2] = benchObj.vertices[i].normal.z;
		benchTexture[i * 3] = benchObj.vertices[i].texture.x;
		benchTexture[i * 3 + 1] = benchObj.vertices[i].texture.y;
		benchTexture[i * 3 + 2] = benchObj.vertices[i].texture.z;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	size_t dataSize = sizeof(GLfloat) * benchObj.vertices.size() * 3;
	glGenBuffersARB(1, &benchVertexVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, benchVertexVBO);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, benchVertex);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, benchNormal);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, benchTexture);

	glEnable(GL_TEXTURE_2D);
	glewInit();
	GLfloat *wardVertex;
	GLfloat *wardNormal;
	wardNormal = new GLfloat[wardObj.vertices.size() * 3];
	wardVertex = new GLfloat[wardObj.vertices.size() * 3];
	GLfloat *wardTexture = new GLfloat[wardObj.vertices.size() * 3];
	for (int i = 0; i < wardObj.vertices.size(); i++) {
		wardVertex[i * 3] = wardObj.vertices[i].position.x * 3;
		wardVertex[i * 3 + 1] = wardObj.vertices[i].position.y * 3;
		wardVertex[i * 3 + 2] = wardObj.vertices[i].position.z * 3;
		wardNormal[i * 3] = wardObj.vertices[i].normal.x;
		wardNormal[i * 3 + 1] = wardObj.vertices[i].normal.y;
		wardNormal[i * 3 + 2] = wardObj.vertices[i].normal.z;
		wardTexture[i * 3] = wardObj.vertices[i].texture.x;
		wardTexture[i * 3 + 1] = wardObj.vertices[i].texture.y;
		wardTexture[i * 3 + 2] = wardObj.vertices[i].texture.z;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	dataSize = sizeof(GLfloat) * wardObj.vertices.size() * 3;
	glGenBuffersARB(1, &wardVertexVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, wardVertexVBO);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, wardVertex);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, wardNormal);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, wardTexture);


	/*glEnable(GL_TEXTURE_2D);
	glewInit();
	GLfloat *paint1Vertex;
	GLfloat *paint1Normal;
	paint1Normal = new GLfloat[paint1Obj.vertices.size() * 3];
	paint1Vertex = new GLfloat[paint1Obj.vertices.size() * 3];
	GLfloat *paint1Texture = new GLfloat[paint1Obj.vertices.size() * 3];
	for (int i = 0; i < paint1Obj.vertices.size(); i++) {
	paint1Vertex[i * 3] = paint1Obj.vertices[i].position.x / 5;
	paint1Vertex[i * 3 + 1] = paint1Obj.vertices[i].position.y / 5;
	paint1Vertex[i * 3 + 2] = paint1Obj.vertices[i].position.z / 5;
	paint1Normal[i * 3] = paint1Obj.vertices[i].normal.x;
	paint1Normal[i * 3 + 1] = paint1Obj.vertices[i].normal.y;
	paint1Normal[i * 3 + 2] = paint1Obj.vertices[i].normal.z;
	paint1Texture[i * 3] = paint1Obj.vertices[i].texture.x;
	paint1Texture[i * 3 + 1] = paint1Obj.vertices[i].texture.y;
	paint1Texture[i * 3 + 2] = paint1Obj.vertices[i].texture.z;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	dataSize = sizeof(GLfloat) * paint1Obj.vertices.size() * 3;
	glGenBuffersARB(1, &paint1VertexVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, paint1VertexVBO);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, paint1Vertex);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, paint1Normal);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, paint1Texture);*/
}
void initCue() {
	glEnable(GL_TEXTURE_2D);
	const std::string cueTexture0 = billiardgl::texturePath(cueObj.materials[0]->texture);
	const std::string cueTexture1 = billiardgl::texturePath(cueObj.materials[1]->texture);
	textureCue[0] = loadTexture(cueTexture0.c_str());
	textureCue[1] = loadTexture(cueTexture1.c_str());
	cout << cueObj.materials[1]->texture << endl;
	glewInit();
	GLfloat *cueVertex;
	GLfloat *cueNormal;
	cueNormal = new GLfloat[cueObj.vertices.size() * 3];
	cueVertex = new GLfloat[cueObj.vertices.size() * 3];
	GLfloat *cueTexture = new GLfloat[cueObj.vertices.size() * 3];
	for (int i = 0; i < cueObj.vertices.size(); i++) {
		cueVertex[i * 3] = cueObj.vertices[i].position.x;
		cueVertex[i * 3 + 1] = cueObj.vertices[i].position.y;
		cueVertex[i * 3 + 2] = cueObj.vertices[i].position.z;
		cueNormal[i * 3] = cueObj.vertices[i].normal.x;
		cueNormal[i * 3 + 1] = cueObj.vertices[i].normal.y;
		cueNormal[i * 3 + 2] = cueObj.vertices[i].normal.z;
		cueTexture[i * 3] = cueObj.vertices[i].texture.x;
		cueTexture[i * 3 + 1] = cueObj.vertices[i].texture.y;
		cueTexture[i * 3 + 2] = cueObj.vertices[i].texture.z;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	size_t dataSize = sizeof(GLfloat) * cueObj.vertices.size() * 3;
	glGenBuffersARB(1, &cueVertexVBO);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, cueVertexVBO);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 3, 0, GL_STATIC_DRAW_ARB);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, dataSize, cueVertex);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize, dataSize, cueNormal);
	glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, dataSize * 2, dataSize, cueTexture);
}

void setMaterial(Material *mat) {
	const GLfloat a[4] = { mat->ambient[0], mat->ambient[1],mat->ambient[2],1.0f };
	const GLfloat d[4] = { mat->diffuse[0],mat->diffuse[1],mat->diffuse[2],1.0f };
	const GLfloat s[4] = { mat->specular[0],mat->specular[1],mat->specular[2],1.0f };
	//const GLfloat e[4] = { mat.emission[0],mat.emission[1],mat.emission[2],1.0f };

	glMaterialfv(GL_FRONT, GL_AMBIENT, a);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, d);
	glMaterialfv(GL_FRONT, GL_SPECULAR, s);
	//glMaterialfv(GL_FRONT, GL_EMISSION, e);
	glMaterialf(GL_FRONT, GL_SHININESS, mat->nShininess);
}
// display
void myDisplay(void)
{
	set_camera();
	renderRoom();

	//	glEnable(GL_CULL_FACE);
	renderTable();
	renderBall();
	renderDecoration();

	if (AimAt == 1)
		renderCue();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);

	for (int i = 0; i < 16; i++)
	{
		if (Fired[i] || AllFired)
		{
			e[i]->emit(-Radius + Ball[i].p.x, Radius + Ball[i].p.x, Ball[i].p.y, Ball[i].p.y, Ball[i].p.z, Ball[i].p.z);
			e[i]->show();
		}
	}

	//e->update();

	glDisable(GL_BLEND);


	if (WaitHit == 1)
		renderRect();

	updatePlayer();
	drawString();
	glutSwapBuffers();
}
// …Ë÷√ ”µ„
void set_camera(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, static_cast<GLdouble>(width) / static_cast<GLdouble>(height), 10, 10000.0f);
	glTranslatef(kx, ky, kz);
	glMatrixMode(GL_TEXTURE);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(at[0], at[1], at[2], at[3], at[4], at[5], 0.0, 1.0, 0.0);//0.0,300.0,500.0,0.0,80.0,0.0
}
// ª≠∑øº‰
void renderRoom()
{
	// Neutral floor keeps the table and balls visually dominant.
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glColor3f(0.18f, 0.20f, 0.19f);
	glBegin(GL_QUADS);
	glVertex3f(-ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glEnd();
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	//ÃÏª®∞Â
	glBindTexture(GL_TEXTURE_2D, tecCeiling);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glTexCoord2f(0.0f, 5.0f); glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glTexCoord2f(5.0f, 5.0f); glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glTexCoord2f(5.0f, 0.0f); glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glEnd();
	// Neutral room walls keep the game view focused on the table.
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glColor3f(0.24f, 0.27f, 0.26f);
	glBegin(GL_QUADS);
	glVertex3f(-ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glVertex3f(-ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, ROOM_HEIGHT, -ROOM_LENGTH / 2);
	glVertex3f(ROOM_WIDTH / 2, 0, -ROOM_LENGTH / 2);
	glEnd();
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
}
// ª≠◊¿◊”
void renderTable()
{
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, tableVertexVBO);

	// enable vertex arrays
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	// before draw, specify vertex and index arrays with their offsets
	glNormalPointer(GL_FLOAT, 0, (void*)(sizeof(GLfloat) * tableObj.vertices.size() * 3));
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(3, GL_FLOAT, 0, (void*)((sizeof(GLfloat) * tableObj.vertices.size() * 3) * 2));

	//setMaterial(tableObj.materials[tableObj.mtlIndex[1]]);
	glBindTexture(GL_TEXTURE_2D, textureIDtest[tableObj.mtlIndex[1]]);
	glDrawArrays(GL_TRIANGLES, 0, tableObj.mtlIndex[4]);

	//setMaterial(tableObj.materials[tableObj.mtlIndex[5]]);
	glBindTexture(GL_TEXTURE_2D, textureIDtest[tableObj.mtlIndex[5]]);
	glDrawArrays(GL_TRIANGLES, tableObj.mtlIndex[4], tableObj.mtlIndex[6] / 2);

	//setMaterial(tableObj.materials[tableObj.mtlIndex[1]]);
	glBindTexture(GL_TEXTURE_2D, textureIDtest[tableObj.mtlIndex[1]]);
	glDrawArrays(GL_TRIANGLES, tableObj.mtlIndex[6] / 2, tableObj.vertices.size());

	glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	// it is good idea to release VBOs with ID 0 after use.
	// Once bound with 0, all pointers in gl*Pointer() behave as real
	// pointer, so, normal vertex array operations are re-activated
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void renderDecoration() {
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, benchVertexVBO);

	// enable vertex arrays
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	// before draw, specify vertex and index arrays with their offsets
	glNormalPointer(GL_FLOAT, 0, (void*)(sizeof(GLfloat) * benchObj.vertices.size() * 3));
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(3, GL_FLOAT, 0, (void*)((sizeof(GLfloat) * benchObj.vertices.size() * 3) * 2));

	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glRotatef(90, 0, 1, 0);
	glTranslatef(0, 0, -ROOM_WIDTH / 2 + 50);
	glBindTexture(GL_TEXTURE_2D, texhe);
	//setMaterial(benchObj.materials[benchObj.mtlIndex[0]]);
	glDrawArrays(GL_TRIANGLES, 0, benchObj.vertices.size());
	glPopMatrix();

	glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);




	glBindBufferARB(GL_ARRAY_BUFFER_ARB, wardVertexVBO);

	// enable vertex arrays
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	// before draw, specify vertex and index arrays with their offsets
	glNormalPointer(GL_FLOAT, 0, (void*)(sizeof(GLfloat) * wardObj.vertices.size() * 3));
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(3, GL_FLOAT, 0, (void*)((sizeof(GLfloat) * wardObj.vertices.size() * 3) * 2));

	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glRotatef(-90, 0, 1, 0);
	glTranslatef(0, 0, -ROOM_WIDTH / 2 + 30);
	glBindTexture(GL_TEXTURE_2D, textureWard);
	//setMaterial(wardObj.materials[wardObj.mtlIndex[0]]);
	glDrawArrays(GL_TRIANGLES, 0, wardObj.vertices.size());
	glPopMatrix();

	glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

}
// …Ë÷√ªÊ«Ú≤Œ ˝
void renderBall(void)
{
	for (i = 0; i<ballcnt; i++) {
		if (Ball[i].isIn == 0) {
			for (j = 0; j < 15; j++) m[j] = 0.0;
			m[0] = m[5] = m[10] = 1.0;
			m[7] = (-1.0) / 405;
			glPushMatrix();
			glTranslatef(Ball[i].p.x, Ball[i].p.y, Ball[i].p.z);
			glPushMatrix();
			glTranslatef(0, 400, 0);
			glMultMatrixf(m);
			glTranslatef(0, -400, 0);
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glColor4f(0.3, 0.3, 0.3, 0.5);
			glBlendFunc(GL_SRC_ALPHA, GL_DST_COLOR);
			drawBall(Radius, texhe);
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
			glPopMatrix();
			glRotatef(Ball[i].ma, Ball[i].a.x, Ball[i].a.y, Ball[i].a.z);
			//			glEnable(GL_BLEND);
			//			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			drawBall(Radius, Ball[i].texture);
			//			glDisable(GL_BLEND);
			glPopMatrix();
		}
		else {
			glPushMatrix();
			glTranslatef(Ball[i].p.x, Ball[i].p.y, Ball[i].p.z);
			drawBall(Radius, Ball[i].texture);
			glPopMatrix();
		}
	}
}
// ª≠«Ú
void drawBall(GLfloat mybr, GLuint BALL)
{
	GLUquadricObj *MYBALL;
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, BALL);
		MYBALL = gluNewQuadric();// ¥¥Ω®“ª∏ˆ∂˛¥Œ«˙√ÊŒÔÃÂ
		gluQuadricTexture(MYBALL, GL_TRUE); //∆Ù”√∏√∂˛¥Œ«˙√ÊµƒŒ∆¿Ì
		glBindTexture(GL_TEXTURE_2D, BALL);// ∞Û∂®Œ∆¿Ì
		gluSphere(MYBALL, mybr, 80, 120); // ª≠«Ú
		gluDeleteQuadric(MYBALL);
	}
}
// ª≠«Ú∏À°¢¡¶∂»Ãı°¢∑ΩœÚœﬂ
void renderCue()
{
	GLfloat lxz;
	lxz = sqrt(pow(at[0] - at[3], 2) + pow(at[2] - at[5], 2));

	glPushMatrix();
	glTranslatef(Ball[0].p.x, Ball[0].p.y, Ball[0].p.z);
	glLineWidth(2);
	glBegin(GL_LINES);
	glVertex3f(0, 0, 0);
	glVertex3f(-150 * (at[0] - at[3]) / lxz, 0, -150 * (at[2] - at[5]) / lxz);
	glEnd();
	glPopMatrix();
	//‘⁄’‚∏ˆ∫Ø ˝¿Ôª≠«Ú∏À

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, cueVertexVBO);

	// enable vertex arrays
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);

	// before draw, specify vertex and index arrays with their offsets
	glNormalPointer(GL_FLOAT, 0, (void*)(sizeof(GLfloat) * cueObj.vertices.size() * 3));
	glVertexPointer(3, GL_FLOAT, 0, 0);
	glTexCoordPointer(3, GL_FLOAT, 0, (void*)((sizeof(GLfloat) * cueObj.vertices.size() * 3) * 2));

	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(Ball[0].p.x + (speed + 6) * 0.1 * (at[0] - at[3]) / lxz, Ball[0].p.y, Ball[0].p.z + (speed + 6) * 0.1 * (at[2] - at[5]) / lxz);
	/*float theta = acosf(cos(anglex) / sin(angley)) * 180 / PI;
	if (anglex < -PI) theta = 360 - theta;
	else if (anglex > 0) theta *= -1;
	cout << theta << endl;*/

	glRotatef(180.5 - 90 - anglex * 180 / PI, 0, 1, 0);
	//glRotatef(80 - angley * 180 / PI, (at[2] - at[5]) / lxz, 0, -(at[0] - at[3]) / lxz);
	//setMaterial(cueObj.materials[0]);

	glBindTexture(GL_TEXTURE_2D, textureCue[0]);
	glDrawArrays(GL_TRIANGLES, cueObj.mtlIndex[4], cueObj.vertices.size());
	glBindTexture(GL_TEXTURE_2D, textureCue[1]);
	glDrawArrays(GL_TRIANGLES, cueObj.mtlIndex[2], cueObj.mtlIndex[4]);
	glBindTexture(GL_TEXTURE_2D, Ball[0].texture);
	glDrawArrays(GL_TRIANGLES, 0, cueObj.mtlIndex[2]);


	glPopMatrix();

	glDisableClientState(GL_VERTEX_ARRAY);  // disable vertex arrays
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void renderRect()
{
	//to2d,ª≠¡¶∂»Ãı
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();
	glDepthMask(GL_FALSE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glColor4f(1.0f, 0.82f, 0.12f, 0.72f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	const GLfloat meter_left = 18.0f;
	const GLfloat meter_bottom = height - 260.0f;
	const GLfloat meter_top = meter_bottom + speed;
	glRectf(meter_left, meter_bottom, meter_left + 36.0f, meter_top);
	glDisable(GL_BLEND);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glPopMatrix();
	//to3d,√≤À∆√ª”√
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}
// ∏¸–¬Œª÷√°¢«Ú ˝¡ø°¢ÀŸ∂»
void myIdle(void)
{
	GLfloat vx = 0, vz = 0;
	at[0] = zoom*(cos(anglex)) + at[3];
	at[1] = zoom*(cos(angley)) + at[4];
	at[2] = zoom*(sin(anglex) * sin(angley)) + at[5];
	bool flag = false;
	for (i = 0; i<ballcnt; i++)
	{
		if (Ball[i].isIn == 0)
		{
			for (k = i + 1; k<ballcnt; k++) collideBalls(i, k);// ≈ˆ◊≤ºÏ≤‚
			collideEdge(i);
			isBallIn(i);
			Ball[i].mv = pow(Ball[i].v.x, 2) + pow(Ball[i].v.z, 2);
			if (Ball[i].mv>0.1)
			{
				Ball[i].mv = sqrt(Ball[i].mv);
				vx = Ball[i].v.x / Ball[i].mv;
				vz = Ball[i].v.z / Ball[i].mv;
				Ball[i].mv += G*T;
				Ball[i].v.x = Ball[i].mv*vx;
				Ball[i].v.z = Ball[i].mv*vz;
				Ball[i].p.x += Ball[i].v.x*T;
				Ball[i].p.z += Ball[i].v.z*T;
				Ball[i].a.x = (-1)*vz;
				Ball[i].a.z = vx;
				Ball[i].ma += -180 * Ball[i].mv*T / (Radius*PI);
				flag = true;
			}
			else Ball[i].mv = 0;
		}
	}
	if (!flag)
	{
		IsMoving = false;
	}
	else
		IsMoving = true;
	if (WaitHit == 1)
	{
		if (speed>200) speed -= 200;
		else speed += 2;
	}
	if (Hit == 1)
	{
		GLfloat atxy = 0;
		atxy = sqrt(pow(at[3] - at[0], 2) + pow(at[5] - at[2], 2));
		Ball[0].v.x = speed*(at[3] - at[0]) / atxy;
		Ball[0].v.z = speed*(at[5] - at[2]) / atxy;
		Hit = 0;
		for (i = speed * 100; i > 0; i--) {
			speed -= 0.01;
		}
		billiardgl::playHit();
	}
	if (leftm)
	{
		Ball[0].v.x = 0;
		Ball[0].v.z = 0;
		speed = 0;
	}
	if (transPerc)
	{
		if (!Isrecroded)
		{
			record_zoom = zoom;
			record_position[0] = Ball[0].p.x;
			record_position[1] = Ball[0].p.y;
			record_position[2] = Ball[0].p.z;
			at[3] = record_position[0];
			at[4] = record_position[1];
			at[5] = record_position[2];
			Isrecroded = true;
		}
		else
		{
			if (zoom < record_zoom + 100)
				zoom += 1;
			else
			{
				if (!IsMoving)
				{
					billiardgl::sleepMilliseconds(1000);
					zoom = record_zoom;
					transPerc = false;
					Isrecroded = false;
				}
			}
		}
	}
	if (IsGameOver)
	{
		if (Ball[8].p.x == 100 || Ball[8].p.x == -100)
		{
			at[0] = 0;
			at[1] = 300;
			at[2] = -TABLE_IN_LENGTH;
			at[3] = 0;
			at[4] = TABLE_HEIGHT;
			at[5] = -TABLE_IN_LENGTH / 4;
		}
	}
	myDisplay();
}
// «Ú”Î«Ú≈ˆ◊≤ºÏ≤‚
void collideBalls(int j, int k)
{
	GLfloat dis, Cos, Sin, cCos, cSin, v1c, v1cc, v2c, v2cc;
	dis = sqrt(pow(Ball[k].p.x - Ball[j].p.x, 2) + pow(Ball[k].p.z - Ball[j].p.z, 2));
	if (dis<2 * Radius - 0.5)
	{
		billiardgl::playHit();
		Cos = (Ball[k].p.x - Ball[j].p.x) / dis;
		Sin = (Ball[k].p.z - Ball[j].p.z) / dis;
		cCos = Sin*(-1);
		cSin = Cos;
		v1c = Ball[j].v.x*Cos + Ball[j].v.z*Sin;
		v1cc = Ball[j].v.x*cCos + Ball[j].v.z*cSin;
		v2c = Ball[k].v.x*Cos + Ball[k].v.z*Sin;
		v2cc = Ball[k].v.x*cCos + Ball[k].v.z*cSin;
		Ball[j].v.x = v1cc*cCos + v2c*Cos;
		Ball[j].v.z = v1cc*cSin + v2c*Sin;
		Ball[k].v.x = v1c*Cos + v2cc*cCos;
		Ball[k].v.z = v1c*Sin + v2cc*cSin;
		Ball[k].p.x = Ball[j].p.x + 2 * Radius*Cos;
		Ball[k].p.z = Ball[j].p.z + 2 * Radius*Sin;
	}
}
// «Ú”ÎÃ®±ﬂ≈ˆ◊≤ºÏ≤‚
void collideEdge(int j)
{
	if (fabs(Ball[j].p.x)>TABLE_IN_WIDTH / 2 - Radius)
	{
		if (Ball[j].p.x>0) Ball[j].p.x = TABLE_IN_WIDTH / 2 - Radius;
		if (Ball[j].p.x<0) Ball[j].p.x = -TABLE_IN_WIDTH / 2 + Radius;
		Ball[j].v.x *= (-1);
	}
	if (fabs(Ball[j].p.z)>TABLE_IN_LENGTH / 2 - Radius)
	{
		if (Ball[j].p.z>0) Ball[j].p.z = TABLE_IN_LENGTH / 2 - Radius;
		if (Ball[j].p.z<0) Ball[j].p.z = -TABLE_IN_LENGTH / 2 + Radius;
		Ball[j].v.z *= (-1);
	}
}
// Ω¯«ÚºÏ≤‚
int isBallIn(int j)
{
	GLfloat y_dis;
	y_dis = 2 * Radius *sin(PI / 3);
	if (sqrt(pow(Ball[j].p.x - (-TABLE_IN_WIDTH / 2 + POCKET_RADIUS), 2) + pow(Ball[j].p.z - (-TABLE_IN_LENGTH / 2 + POCKET_RADIUS), 2)) < Radius / 4 ||
		sqrt(pow(Ball[j].p.x - TABLE_IN_WIDTH / 2 + POCKET_RADIUS, 2) + pow(Ball[j].p.z - (-TABLE_IN_LENGTH / 2 + POCKET_RADIUS), 2)) < Radius / 4 ||
		sqrt(pow(Ball[j].p.x - (-TABLE_IN_WIDTH / 2 + POCKET_RADIUS), 2) + pow(Ball[j].p.z - TABLE_IN_LENGTH / 2 + POCKET_RADIUS, 2)) < Radius / 4 ||
		sqrt(pow(Ball[j].p.x - TABLE_IN_WIDTH / 2 + POCKET_RADIUS, 2) + pow(Ball[j].p.z - TABLE_IN_LENGTH / 2 + POCKET_RADIUS, 2)) < Radius / 4 ||
		sqrt(pow(Ball[j].p.x - (-TABLE_IN_WIDTH / 2 + POCKET_RADIUS), 2) + pow(Ball[j].p.z, 2)) < Radius / 4 ||
		sqrt(pow(Ball[j].p.x - -TABLE_IN_WIDTH / 2 + POCKET_RADIUS, 2) + pow(Ball[j].p.z, 2)) < Radius / 4)
	{
		//		Fired[j] = true;
		billiardgl::playBallIn();
		if (j == 8)
		{
			IsGameOver = true;
			billiardgl::playGameOver();
		}
		if (j == 0)
		{
			Ball[0].p.x = 0;
			Ball[0].p.z = -TABLE_IN_LENGTH / 4;
			Ball[0].p.y = TABLE_HEIGHT + Radius;
			IsIllegal = 1;
		}
		//		else if (j == 8) { Ball[8].p.x = 0; Ball[8].p.y = 80; Ball[8].p.z = 70 + 2.0*y_dis; }
		else
		{
			Ball[j].isIn = 1;
			BallInNum += 1;
			Ball[j].p.z = -100 + BallInNum * 20;
			Ball[j].p.y = TABLE_HEIGHT - Radius;
			if (j > 8)
			{
				Ball[j].p.y = -100;
				if (IsFirstInBall)
				{
					PlayerBall[CurrPlayer] = 1;
					PlayerBall[1 - CurrPlayer] = 0;
					IsFirstInBall = false;
					NextPlayer = CurrPlayer;
				}
				else if (PlayerBall[CurrPlayer] == 1) //ª®…´«ÚΩ¯¡À£¨«“µ±«∞ÕÊº“ «ª®…´«ÚÕÊº“
				{
					NextPlayer = CurrPlayer;
				}
				if (PlayerBall[CurrPlayer] == 0) //∑∏πÊ
					IsIllegal = 1;
			}
			else
			{
				Ball[j].p.y = -100;
				if (IsFirstInBall)
				{
					PlayerBall[CurrPlayer] = 0;
					PlayerBall[1 - CurrPlayer] = 1;
					IsFirstInBall = false;
					NextPlayer = CurrPlayer;
				}
				else if (PlayerBall[CurrPlayer] == 0) //¥ø…´«ÚΩ¯¡À£¨«“µ±«∞ÕÊº“ «¥ø…´«ÚÕÊº“
				{
					NextPlayer = CurrPlayer;
				}
				if (PlayerBall[CurrPlayer] == 1) //∑∏πÊ
					IsIllegal = 1;
			}
		}
		Ball[j].v.x = 0;
		Ball[j].v.y = 0;
		Ball[j].v.z = 0;
		return 1;
	}
	return 0;
}
//  Û±Íº¸≈Ã∑¥¿°∫Ø ˝
static void myKeyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
		exit(0);
	case 'd':
	case 'D':
		if (kx>-490) kx -= 10;
		break;
	case 'a':
	case 'A':
		if (kx<490) kx += 10;
		break;
	case 'w':
	case 'W':
		zoom -= 10;
		if (zoom<10) zoom = 10;
		break;
	case 's':
	case 'S':
		zoom += 10;
		if (zoom>500) zoom = 500;
		break;
	case '0':
		Fired[0] = !Fired[0];
		break;
	case '1':
		Fired[1] = !Fired[1];
		break;
	case '2':
		Fired[2] = !Fired[2];
		break;
	case '3':
		Fired[3] = !Fired[3];
		break;
	case '4':
		Fired[4] = !Fired[4];
		break;
	case '5':
		Fired[5] = !Fired[5];
		break;
	case '6':
		Fired[6] = !Fired[6];
		break;
	case '7':
		Fired[7] = !Fired[7];
		break;
	case '8':
		Fired[8] = !Fired[8];
		break;
	case '9':
		Fired[9] = !Fired[9];
		break;
	case 'p':
		Fired[10] = !Fired[10];
		break;
	case 'o':
		Fired[11] = !Fired[11];
		break;
	case 'i':
		Fired[12] = !Fired[12];
		break;
	case 'u':
		Fired[13] = !Fired[13];
		break;
	case 'y':
		Fired[14] = !Fired[14];
		break;
	case 't':
		Fired[15] = !Fired[15];
		break;
	case 'f':
		AllFired = !AllFired;

		//		break;
	}
}
static void myMouse(int mbutton, int mstate, int x, int y)
{
	if (IsMoving)
	{
		return;
	}
	at[3] = Ball[0].p.x;
	at[4] = Ball[0].p.y;
	at[5] = Ball[0].p.z;
	at[0] = zoom*(cos(anglex) * sin(angley)) + at[3];
	at[1] = zoom*(cos(angley)) + at[4];
	at[2] = zoom*(sin(anglex) * sin(angley)) + at[5];
	mx = x; my = y;
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_UP && TrackpadOrbit)
	{
		TrackpadOrbit = false;
		rightm = 0;
		AimAt = 0;
		Hit = 0;
		return;
	}
	const bool shift_drag = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;
	if ((mbutton == GLUT_RIGHT_BUTTON || (mbutton == GLUT_LEFT_BUTTON && shift_drag)) && mstate == GLUT_DOWN)
	{
		nowanglex = anglex; nowangley = angley; leftm = 0; rightm = 1;
		TrackpadOrbit = mbutton == GLUT_LEFT_BUTTON;
		AimAt = 1;
	}
	else { rightm = 0; TrackpadOrbit = false; WaitHit = 0; }
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_DOWN && !shift_drag)
	{
		WaitHit = 1;
		NextPlayer = 1 - CurrPlayer;
		Hitted = false;
		AimAt = 1;
	}
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_UP)
	{
		Hit = 1;
		IsIllegal = 0;
		AimAt = 0;
		IsMoving = true;
		transPerc = true;
		updated = false;
		Hitted = true;
	}
	else Hit = 0;
}
static void mouseMove(int x, int y)
{
	at[3] = Ball[0].p.x;
	at[4] = Ball[0].p.y;
	at[5] = Ball[0].p.z;
	at[0] = zoom*(cos(anglex) * sin(angley)) + at[3];
	at[1] = zoom*(cos(angley)) + at[4];
	at[2] = zoom*(sin(anglex) * sin(angley)) + at[5];

	if (IsMoving)
		return;

	if (leftm) { kx = nowatx + x - mx; ky = nowaty + y - my; }
	if (rightm) { anglex = nowanglex + (x - mx)*0.01; angley = nowangley + (y - my)*0.01; }
	if (angley <= 0)
		angley = 0.1;
	if (angley > PI / 2)
		angley = PI / 2;
}
// ‘ÿ»ÎŒ∆¿Ì
void initLoadTexture()
{
	Ball[1].texture = loadTexture(billiardgl::texturePath("B1.bmp").c_str());
	Ball[2].texture = loadTexture(billiardgl::texturePath("B2.bmp").c_str());
	Ball[3].texture = loadTexture(billiardgl::texturePath("B3.bmp").c_str());
	Ball[4].texture = loadTexture(billiardgl::texturePath("B4.bmp").c_str());
	Ball[5].texture = loadTexture(billiardgl::texturePath("B5.bmp").c_str());
	Ball[6].texture = loadTexture(billiardgl::texturePath("B6.bmp").c_str());
	Ball[7].texture = loadTexture(billiardgl::texturePath("B7.bmp").c_str());
	Ball[8].texture = loadTexture(billiardgl::texturePath("B8.bmp").c_str());
	Ball[9].texture = loadTexture(billiardgl::texturePath("B9.bmp").c_str());
	Ball[10].texture = loadTexture(billiardgl::texturePath("B10.bmp").c_str());
	Ball[11].texture = loadTexture(billiardgl::texturePath("B11.bmp").c_str());
	Ball[12].texture = loadTexture(billiardgl::texturePath("B12.bmp").c_str());
	Ball[13].texture = loadTexture(billiardgl::texturePath("B13.bmp").c_str());
	Ball[14].texture = loadTexture(billiardgl::texturePath("B14.bmp").c_str());
	Ball[15].texture = loadTexture(billiardgl::texturePath("B15.bmp").c_str());
	Ball[0].texture = loadTexture(billiardgl::texturePath("B16.bmp").c_str());
	texGround = loadTexture(billiardgl::texturePath("ground.bmp").c_str());//ground
	texWall = loadTexture(billiardgl::texturePath("wall.bmp").c_str());//wall
	texWall1 = loadTexture(billiardgl::texturePath("wall1.bmp").c_str());
	texWall2 = loadTexture(billiardgl::texturePath("wall2.bmp").c_str());
	tecCeiling = loadTexture(billiardgl::texturePath("ceiling.bmp").c_str());//ÃÏª®∞Â
	BZD = loadTexture(billiardgl::texturePath("black.bmp").c_str());
	texTableCloth = loadTexture(billiardgl::texturePath("green.bmp").c_str());//◊¿√Ê
	texTable = loadTexture(billiardgl::texturePath("wood.bmp").c_str());//«Ú◊¿±ﬂ‘µ
	texCue = loadTexture(billiardgl::texturePath("wood.bmp").c_str());
	texgt = loadTexture(billiardgl::texturePath("green.bmp").c_str());
	texhe = loadTexture(billiardgl::texturePath("black.bmp").c_str());
	textureWard = loadTexture(billiardgl::texturePath("5.bmp").c_str());
	texturePaint1 = loadTexture(billiardgl::texturePath("6.bmp").c_str());
	texturePaint2 = loadTexture(billiardgl::texturePath("7.bmp").c_str());
	texture[2] = loadTexture(billiardgl::texturePath("flame2.bmp").c_str());
}
int isPowerOfTwo(int n)
{
	if (n <= 0)
		return 0;
	return (n & (n - 1)) == 0;
}

int previousPowerOfTwo(int n)
{
	int value = 1;
	while (value * 2 <= n)
		value *= 2;
	return value;
}
GLuint loadTexture(const char* file_name)
{
	GLint width, height, total_bytes;
	GLubyte* pixels = 0;
	GLuint last_texture_ID, texture_ID = 0;
	FILE* pFile = std::fopen(file_name, "rb");
	if (pFile == 0)
		return 0;
	fseek(pFile, 0x0012, SEEK_SET);
	fread(&width, 4, 1, pFile);
	fread(&height, 4, 1, pFile);
	fseek(pFile, BMP_Header_Length, SEEK_SET);
	{
		GLint line_bytes = width * 3;
		while (line_bytes % 4 != 0) ++line_bytes;
		total_bytes = line_bytes * height;
	}
	pixels = (GLubyte*)malloc(total_bytes);
	if (pixels == 0) { fclose(pFile); return 0; }
	if (fread(pixels, total_bytes, 1, pFile) <= 0) { free(pixels); fclose(pFile); return 0; }
	{
		GLint max;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
		if (!isPowerOfTwo(width) || !isPowerOfTwo(height) || width > max || height > max)
		{
			GLint new_width = previousPowerOfTwo(width);
			GLint new_height = previousPowerOfTwo(height);
			if (new_width > max)
				new_width = max;
			if (new_height > max)
				new_height = max;
			GLint new_line_bytes, new_total_bytes;
			GLubyte* new_pixels = 0;
			new_line_bytes = new_width * 3;
			while (new_line_bytes % 4 != 0)
				++new_line_bytes;
			new_total_bytes = new_line_bytes * new_height;
			new_pixels = (GLubyte*)malloc(new_total_bytes);
			if (new_pixels == 0) { free(pixels); fclose(pFile); return 0; }
			gluScaleImage(GL_BGR_EXT,
				width, height, GL_UNSIGNED_BYTE, pixels,
				new_width, new_height, GL_UNSIGNED_BYTE, new_pixels);
			free(pixels);
			pixels = new_pixels;
			width = new_width;
			height = new_height;
		}
	}
	glGenTextures(1, &texture_ID);
	if (texture_ID == 0) { free(pixels); fclose(pFile); return 0; }
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (int*)&last_texture_ID);
	glBindTexture(GL_TEXTURE_2D, texture_ID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, GL_BGR_EXT,
		GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, last_texture_ID);
	free(pixels);
	return texture_ID;
}
// π‚‘¥
void initLight(void)
{
	//π‚’’¥¶¿Ì
	GLfloat light_ambient[] = { 0.65f, 0.65f, 0.65f, 1.0f };
	GLfloat light_diffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
	GLfloat light_specular[] = { 0.55f, 0.55f, 0.55f, 1.0f };
	GLfloat light_position0[] = { 0.0f, 460.0f, 0.0f, 1.0f };
	//∂®“Âπ‚Œª÷√µ√∆Î¥Œ◊¯±Í (x,y,z,w), »Áπ˚w = 1.0, Œ™∂®Œªπ‚‘¥£®“≤Ω–µ„π‚‘¥£© £¨»Áπ˚ w£Ω0£¨Œ™∂®œÚπ‚‘¥£®Œﬁœﬁπ‚‘¥£© £¨∂®œÚπ‚‘¥Œ™Œﬁ«Ó‘∂µ„£¨“Ú∂¯≤˙…˙π‚Œ™∆Ω––π‚
	GLfloat light_direction[] = { 0, -1, 0 };
	glLightfv(L0, GL_AMBIENT, light_ambient); // ª∑æ≥π‚
	glLightfv(L0, GL_DIFFUSE, light_diffuse); // ¬˛…‰π‚
	glLightfv(L0, GL_SPECULAR, light_specular); // æµ√Ê∑¥…‰
	glLightfv(L0, GL_POSITION, light_position0); // π‚’’Œª÷√
	glLightfv(L0, GL_SPOT_DIRECTION, light_direction);//æ€π‚∑ΩœÚ
	glLightf(L0, GL_SPOT_CUTOFF, 180.0f);//æ€π‚Ωÿ÷¡Ω«
	glLightf(L0, GL_SPOT_EXPONENT, 0);//æ€π‚÷∏ ˝
	glEnable(GL_LIGHTING); // ∆Ù∂Øπ‚’’
	glEnable(L0); //  πµ⁄“ª’µµ∆”––ß
				  //≤ƒ÷ ¥¶¿Ì
	GLfloat mat_ambient[] = { 0.35f, 0.35f, 0.35f, 1.0f };
	GLfloat mat_diffuse[] = { 0.9f, 0.9f, 0.9f, 1.0f };
	GLfloat mat_specular[] = { 0.55f, 0.55f, 0.55f, 1.0f };
	GLfloat mat_shininess[] = { 45.0f }; // ≤ƒ÷  RGBA æµ√Ê÷∏ ˝£¨ ˝÷µ‘⁄ 0°´128 ∑∂Œßƒ⁄
	glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_DEPTH_TEST); // ≤‚ ‘…Ó∂»ª∫¥Ê
	glShadeModel(GL_SMOOTH);
}
void b_music()
{
	billiardgl::playBackgroundLoop();
}

void drawString()
{
	std::string player_text = "Current Player:  Player " + std::to_string(CurrPlayer + 1);
	myString(18, height - 68, GLUT_BITMAP_TIMES_ROMAN_24, player_text.c_str());
}
void myString(float x, float y, void *font, const char* c)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();
	glDepthMask(GL_FALSE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glRasterPos2f(x, y);
	for (; *c != '\0'; c++) {
		glutBitmapCharacter(font, *c);
	}

	glDisable(GL_BLEND);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}
void updatePlayer()
{
	if (updated)
		return;
	if ((!transPerc) && Hitted)
	{
		if ((!IsIllegal) && (NextPlayer == CurrPlayer));
		else
			CurrPlayer = 1 - CurrPlayer;
		updated = true;
	}
}