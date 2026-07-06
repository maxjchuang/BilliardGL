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

#include "game_state.h"
#include "hud.h"
#include "input.h"
#include "particle.h"
#include "physics.h"
#include "rules.h"
#include "platform_audio.h"
#include "platform_time.h"
#include "resource_path.h"
#include "renderer.h"
#include "render_resources.h"
#include "screenshot.h"

#include <cmath>
#include <array>
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
static billiardgl::GameState Game;
static billiardgl::RenderResources Render;

//±‰¡ø…Í√˜
int& width = Game.config.width;
int& height = Game.config.height;
int i = 0, j = 0, k = 0, ballcnt = 16;
static GLfloat M = 1, U = 0.2, T = 0.1, Radius = 5.715, G = -4;
GLfloat m[16];

float record_zoom;
float record_position[3];
bool Fired[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
bool AllFired = false;

//∂®“Â«Úº∞Œª÷√ ∏¡øΩ·ππÃÂ
struct Point
{
	GLfloat x;
	GLfloat y;
	GLfloat z;
};

// ‘ÿ»ÎŒ∆¿Ì
// ≥°æ∞ªÊ÷∆
void initBall();
void parseLaunchOptions(int argc, char* argv[]);
void prepareScreenshotScene();
void initWindows(void);
void myReshape(int w, int h);
void myDisplay(void);
void set_camera(void);
void myIdle(void);
void drawString();
void drawHelpPrompt();
void drawHelpOverlay();
void drawScreenRect(float left, float bottom, float right, float top, GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void myString(float x, float y, void *font, const char* c);
void updatePlayer();
// π‚‘¥
void initLight();
//  Û±Íº¸≈Ã≤Ÿ◊˜
static void myKeyboard(unsigned char key, int x, int y);
static void mySpecialKeyboard(int key, int x, int y);
static void mySpecialKeyboard(int key, int x, int y)
{
	Game.camera.angleX = Game.camera.angleX;
	Game.camera.angleY = Game.camera.angleY;
	billiardgl::handleSpecialKey(Game, GLUT_KEY_LEFT, GLUT_KEY_RIGHT, GLUT_KEY_UP, GLUT_KEY_DOWN, key);
	Game.camera.angleX = Game.camera.angleX;
	Game.camera.angleY = Game.camera.angleY;
}

static void myMouse(int mbutton, int mstate, int x, int y);
static void mouseMove(int x, int y);
void b_music();

particle* init_flame()
{
	float size = rand() % 90 * 0.02f;
	float particleSpeed[] = { float(rand() % 10 - 4) / 1600, float(rand() % 10 - 4) / 800, float(rand() % 10 - 4) / 1600 };
	float acc[] = { 1.0f*(rand() % 3 - 1) / 9000000,4.9 / 4000000 ,1.0f*(rand() % 3 - 1) / 9000000 };
	float angle[] = { static_cast<float>(rand() % 360), static_cast<float>(rand() % 360), static_cast<float>(rand() % 360) };
	particle* p = new particle(vec(size, size, size), vec(particleSpeed), vec(acc),
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
	if (!billiardgl::initializeRenderResources(Render, Game))
	{
		std::fprintf(stderr, "Failed to initialize render resources\n");
		return 1;
	}
	texture[2] = Render.flameTexture;

	for (int i = 0; i < ballcnt; i++)
	{
		e[i] = new emitter(init_flame, 5000, -Radius + Game.balls[i].position.x, Radius + Game.balls[i].position.x, Game.balls[i].position.y, Game.balls[i].position.y, Game.balls[i].position.z, Game.balls[i].position.z);
		Render.emitters[i] = e[i];
	}

	prepareScreenshotScene();

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
			Game.config.windowedMode = true;
		else if (strcmp(argv[i], "--fullscreen") == 0)
			Game.config.windowedMode = false;
		else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
		{
			Game.config.screenshotPath = argv[++i];
			Game.config.windowedMode = true;
		}
		else if (strcmp(argv[i], "--screenshot-scene") == 0 && i + 1 < argc)
		{
			const char* scene = argv[++i];
			if (strcmp(scene, "default") == 0)
				Game.config.screenshotScene = billiardgl::ScreenshotScene::Default;
			else if (strcmp(scene, "help") == 0)
				Game.config.screenshotScene = billiardgl::ScreenshotScene::Help;
			else if (strcmp(scene, "after-shot") == 0)
				Game.config.screenshotScene = billiardgl::ScreenshotScene::AfterShot;
			else
			{
				std::fprintf(stderr, "Unknown screenshot scene: %s\n", scene);
				std::exit(1);
			}
		}
	}
}

void prepareScreenshotScene()
{
	if (Game.config.screenshotPath.empty())
		return;

	if (Game.config.screenshotScene == billiardgl::ScreenshotScene::Help)
	{
		Game.hud.showHelp = true;
		return;
	}

	if (Game.config.screenshotScene == billiardgl::ScreenshotScene::AfterShot)
	{
		billiardgl::setBallVelocity(Game.balls[0], 30.0f, 0.0f, 0.0f);
		Game.players.shotTaken = true;
		Game.ballsMoving = true;
		for (int step = 0; step < 30; ++step)
			billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep);
		billiardgl::updateCameraFromCueBall(Game);
		Game.camera.target[0] = Game.camera.target[0];
		Game.camera.target[1] = Game.camera.target[1];
		Game.camera.target[2] = Game.camera.target[2];
	}
}

void initWindows(void)
{
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Billards");
	if (!Game.config.windowedMode)
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
	billiardgl::initializeBalls(Game);
	Point s;
	s.x = 0;
	s.z = TABLE_IN_LENGTH / 4;
	s.y = TABLE_HEIGHT + BALL_RADIUS;
	float y_dis = sqrt(3)*BALL_RADIUS;
	float x_dis = BALL_RADIUS;
	Game.balls[0].position.x = s.x; Game.balls[0].position.z = -s.z; Game.balls[0].position.y = s.y;
	//first row
	Game.balls[1].position.x = s.x; Game.balls[1].position.z = s.z; Game.balls[1].position.y = s.y;
	//second row
	Game.balls[2].position.x = s.x - x_dis; Game.balls[2].position.z = s.z + y_dis; Game.balls[2].position.y = s.y;
	Game.balls[3].position.x = s.x + x_dis; Game.balls[3].position.z = s.z + y_dis; Game.balls[3].position.y = s.y;

	Game.balls[4].position.x = s.x - 2 * x_dis; Game.balls[4].position.z = s.z + 2 * y_dis; Game.balls[4].position.y = s.y;
	Game.balls[8].position.x = s.x; Game.balls[8].position.z = s.z + 2 * y_dis; Game.balls[8].position.y = s.y;
	Game.balls[6].position.x = s.x + 2 * x_dis; Game.balls[6].position.z = s.z + 2 * y_dis; Game.balls[6].position.y = s.y;

	Game.balls[7].position.x = s.x - 3 * x_dis; Game.balls[7].position.z = s.z + 3 * y_dis; Game.balls[7].position.y = s.y;
	Game.balls[5].position.x = s.x - x_dis; Game.balls[5].position.z = s.z + 3 * y_dis; Game.balls[5].position.y = s.y;
	Game.balls[9].position.x = s.x + x_dis; Game.balls[9].position.z = s.z + 3 * y_dis; Game.balls[9].position.y = s.y;
	Game.balls[10].position.x = s.x + 3 * x_dis; Game.balls[10].position.z = s.z + 3 * y_dis; Game.balls[10].position.y = s.y;

	Game.balls[11].position.x = s.x - 4 * x_dis; Game.balls[11].position.z = s.z + 4 * y_dis; Game.balls[11].position.y = s.y;
	Game.balls[12].position.x = s.x - 2 * x_dis; Game.balls[12].position.z = s.z + 4 * y_dis; Game.balls[12].position.y = s.y;
	Game.balls[13].position.x = s.x; Game.balls[13].position.z = s.z + 4 * y_dis; Game.balls[13].position.y = s.y;
	Game.balls[14].position.x = s.x + 2 * x_dis; Game.balls[14].position.z = s.z + 4 * y_dis; Game.balls[14].position.y = s.y;
	Game.balls[15].position.x = s.x + 4 * x_dis; Game.balls[15].position.z = s.z + 4 * y_dis; Game.balls[15].position.y = s.y;

	for (i = 0; i < ballcnt; i++)
	{
		billiardgl::resetBallMotion(Game.balls[i]);
		Game.balls[i].pocketed = false;
	}
}

// display
void myDisplay(void)
{
	if (!Game.config.screenshotPath.empty())
	{
		billiardgl::updateCameraFromCueBall(Game);
	}
	set_camera();
	Render.cameraEye[0] = Game.camera.eye[0];
	Render.cameraEye[1] = Game.camera.eye[1];
	Render.cameraEye[2] = Game.camera.eye[2];
	Render.cameraTarget[0] = Game.camera.target[0];
	Render.cameraTarget[1] = Game.camera.target[1];
	Render.cameraTarget[2] = Game.camera.target[2];
	Render.shotPower = Game.input.shotPower;
	Render.showCue = Game.players.aimingAtCueBall == 1;
	Render.showPowerMeter = Game.input.waitingForHit == 1;
	Render.viewportWidth = width;
	Render.viewportHeight = height;
	Render.allFired = AllFired;
	for (int i = 0; i < ballcnt; ++i)
		Render.fired[i] = Fired[i];
	billiardgl::renderScene(Game, Render);

	updatePlayer();
	Game.config.width = width;
	Game.config.height = height;
	billiardgl::drawHud(Game);
	if (!Game.config.screenshotPath.empty())
	{
		const bool saved = billiardgl::saveFramebufferToPpm(Game.config.screenshotPath, width, height);
		glutSwapBuffers();
		std::exit(saved ? 0 : 2);
	}
	glutSwapBuffers();
}
// …Ë÷√ ”µ„
void set_camera(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, static_cast<GLdouble>(width) / static_cast<GLdouble>(height), 10, 10000.0f);
	glTranslatef(Game.camera.panX, Game.camera.panY, Game.camera.panZ);
	glMatrixMode(GL_TEXTURE);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(Game.camera.eye[0], Game.camera.eye[1], Game.camera.eye[2], Game.camera.target[0], Game.camera.target[1], Game.camera.target[2], 0.0, 1.0, 0.0);//0.0,300.0,500.0,0.0,80.0,0.0
}
// ª≠∑øº‰
void myIdle(void)
{
	Game.camera.target[0] = Game.balls[0].position.x;
	Game.camera.target[1] = Game.balls[0].position.y;
	Game.camera.target[2] = Game.balls[0].position.z;
	Game.camera.eye[0] = Game.camera.zoom * (cos(Game.camera.angleX)) + Game.camera.target[0];
	Game.camera.eye[1] = Game.camera.zoom * (cos(Game.camera.angleY)) + Game.camera.target[1];
	Game.camera.eye[2] = Game.camera.zoom * (sin(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[2];

	billiardgl::chargeShotPower(Game, 200.0f, 2.0f);

	if (Game.input.hitRequested == 1)
	{
		const GLfloat atxy = sqrt(pow(Game.camera.target[0] - Game.camera.eye[0], 2) + pow(Game.camera.target[2] - Game.camera.eye[2], 2));
		if (atxy > 0)
		{
			billiardgl::setBallVelocity(Game.balls[0], Game.input.shotPower * (Game.camera.target[0] - Game.camera.eye[0]) / atxy, 0.0f, Game.input.shotPower * (Game.camera.target[2] - Game.camera.eye[2]) / atxy);
			Game.players.shotTaken = true;
			Game.players.updatedAfterShot = false;
			Game.ballsMoving = true;
		}
		Game.input.hitRequested = 0;
		Game.input.shotPower = 0;
		billiardgl::playHit();
	}

	if (Game.input.leftMouseDown)
	{
		Game.balls[0].velocity.x = 0;
		Game.balls[0].velocity.z = 0;
		Game.input.shotPower = 0;
	}

	billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep);
	if (Game.events.ballCollision || Game.events.railCollision)
		billiardgl::playHit();
	if (Game.events.ballPocketed || Game.events.cueBallPocketed)
		billiardgl::playBallIn();
	if (Game.events.eightBallPocketed)
		billiardgl::playGameOver();

	if (Game.transitionPerspective)
	{
		if (!Game.perspectiveRecorded)
		{
			record_zoom = Game.camera.zoom;
			record_position[0] = Game.balls[0].position.x;
			record_position[1] = Game.balls[0].position.y;
			record_position[2] = Game.balls[0].position.z;
			Game.camera.target[0] = record_position[0];
			Game.camera.target[1] = record_position[1];
			Game.camera.target[2] = record_position[2];
			Game.perspectiveRecorded = true;
		}
		else
		{
			if (Game.camera.zoom < record_zoom + 100)
				Game.camera.zoom += 1;
			else if (!Game.ballsMoving)
			{
				billiardgl::sleepMilliseconds(1000);
				Game.camera.zoom = record_zoom;
				Game.transitionPerspective = false;
				Game.perspectiveRecorded = false;
			}
		}
	}

	if (Game.gameOver)
	{
		if (Game.balls[8].position.x == 100 || Game.balls[8].position.x == -100)
		{
			Game.camera.eye[0] = 0;
			Game.camera.eye[1] = 300;
			Game.camera.eye[2] = -TABLE_IN_LENGTH;
			Game.camera.target[0] = 0;
			Game.camera.target[1] = TABLE_HEIGHT;
			Game.camera.target[2] = -TABLE_IN_LENGTH / 4;
		}
	}

	if (Game.events.shotEnded || (!Game.transitionPerspective && Game.players.shotTaken))
		billiardgl::updatePlayerAfterShot(Game);

	myDisplay();
}
// «Ú”Î«Ú≈ˆ◊≤ºÏ≤‚
static void myKeyboard(unsigned char key, int x, int y)
{
	if (key == 'h' || key == 'H')
	{
		billiardgl::handleHelpKey(Game);
		return;
	}
	if (Game.hud.showHelp && key != 27)
		return;
	switch (key)
	{
	case 27:
		exit(0);
	case 'd':
	case 'D':
		if (Game.camera.panX>-490) Game.camera.panX -= 10;
		break;
	case 'a':
	case 'A':
		if (Game.camera.panX<490) Game.camera.panX += 10;
		break;
	case 'w':
	case 'W':
		Game.camera.zoom -= 10;
		if (Game.camera.zoom<10) Game.camera.zoom = 10;
		break;
	case 's':
	case 'S':
		Game.camera.zoom += 10;
		if (Game.camera.zoom>500) Game.camera.zoom = 500;
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
	if (Game.hud.showHelp)
	{
		Game.input.waitingForHit = 0;
		Game.input.hitRequested = 0;
		Game.input.trackpadOrbit = false;
		Game.input.rightMouseDown = 0;
		return;
	}
	if (Game.ballsMoving)
	{
		return;
	}
	Game.camera.target[0] = Game.balls[0].position.x;
	Game.camera.target[1] = Game.balls[0].position.y;
	Game.camera.target[2] = Game.balls[0].position.z;
	Game.camera.eye[0] = Game.camera.zoom*(cos(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[0];
	Game.camera.eye[1] = Game.camera.zoom*(cos(Game.camera.angleY)) + Game.camera.target[1];
	Game.camera.eye[2] = Game.camera.zoom*(sin(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[2];
	Game.input.mouseX = x; Game.input.mouseY = y;
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_UP && Game.input.trackpadOrbit)
	{
		Game.input.trackpadOrbit = false;
		Game.input.rightMouseDown = 0;
		Game.players.aimingAtCueBall = 0;
		Game.input.hitRequested = 0;
		return;
	}
	const bool shift_drag = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;
	if ((mbutton == GLUT_RIGHT_BUTTON || (mbutton == GLUT_LEFT_BUTTON && shift_drag)) && mstate == GLUT_DOWN)
	{
		Game.camera.previousAngleX = Game.camera.angleX; Game.camera.previousAngleY = Game.camera.angleY; Game.input.leftMouseDown = 0; Game.input.rightMouseDown = 1;
		Game.input.trackpadOrbit = mbutton == GLUT_LEFT_BUTTON;
		Game.players.aimingAtCueBall = 1;
	}
	else { Game.input.rightMouseDown = 0; Game.input.trackpadOrbit = false; Game.input.waitingForHit = 0; }
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_DOWN && !shift_drag)
	{
		Game.input.waitingForHit = 1;
		Game.players.nextPlayer = 1 - Game.players.currentPlayer;
		Game.players.shotTaken = false;
		Game.players.aimingAtCueBall = 1;
	}
	if (mbutton == GLUT_LEFT_BUTTON && mstate == GLUT_UP)
	{
		Game.input.hitRequested = 1;
		Game.players.illegalShot = 0;
		Game.players.aimingAtCueBall = 0;
		Game.ballsMoving = true;
		Game.transitionPerspective = true;
		Game.players.updatedAfterShot = false;
		Game.players.shotTaken = true;
	}
	else Game.input.hitRequested = 0;
}
static void mouseMove(int x, int y)
{
	Game.camera.target[0] = Game.balls[0].position.x;
	Game.camera.target[1] = Game.balls[0].position.y;
	Game.camera.target[2] = Game.balls[0].position.z;
	Game.camera.eye[0] = Game.camera.zoom*(cos(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[0];
	Game.camera.eye[1] = Game.camera.zoom*(cos(Game.camera.angleY)) + Game.camera.target[1];
	Game.camera.eye[2] = Game.camera.zoom*(sin(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[2];

	if (Game.ballsMoving)
		return;

	if (Game.input.leftMouseDown) { Game.camera.panX = Game.camera.previousTargetX + x - Game.input.mouseX; Game.camera.panY = Game.camera.previousTargetY + y - Game.input.mouseY; }
	if (Game.input.rightMouseDown) { Game.camera.angleX = Game.camera.previousAngleX + (x - Game.input.mouseX)*0.01; Game.camera.angleY = Game.camera.previousAngleY + (y - Game.input.mouseY)*0.01; }
	if (Game.camera.angleY <= 0)
		Game.camera.angleY = 0.1;
	if (Game.camera.angleY > PI / 2)
		Game.camera.angleY = PI / 2;
}
// ‘ÿ»ÎŒ∆¿Ì
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

void drawHelpPrompt()
{
	myString(18, height - 94, GLUT_BITMAP_HELVETICA_18, "Press H for help");
}

void drawHelpOverlay()
{
	const float panel_width = 520.0f;
	const float panel_height = 330.0f;
	const float left = (width - panel_width) * 0.5f;
	const float bottom = (height - panel_height) * 0.5f;
	const float top = bottom + panel_height;
	const float text_left = left + 34.0f;
	float y = top - 44.0f;

	drawScreenRect(left, bottom, left + panel_width, top, 0.04f, 0.06f, 0.05f, 0.82f);
	myString(text_left, y, GLUT_BITMAP_TIMES_ROMAN_24, "BilliardGL Help");
	y -= 42.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Camera");
	y -= 28.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "W / S                 Zoom in / out");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "A / D                 Pan left / right");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Arrow keys            Orbit view");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Right mouse drag      Orbit view");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Shift + trackpad drag Orbit view");
	y -= 38.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Play");
	y -= 28.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Left mouse hold       Charge shot");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Left mouse release    Game.input.hitRequested cue ball");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "H                     Toggle help");
	y -= 24.0f;
	myString(text_left, y, GLUT_BITMAP_HELVETICA_18, "Esc                   Quit");
}

void drawScreenRect(float left, float bottom, float right, float top, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, width, 0, height);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDepthMask(GL_FALSE);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glColor4f(r, g, b, a);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	glVertex2f(left, bottom);
	glVertex2f(right, bottom);
	glVertex2f(right, top);
	glVertex2f(left, top);
	glEnd();
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDepthMask(GL_TRUE);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void drawString()
{
	std::string player_text = "Current Player:  Player " + std::to_string(Game.players.currentPlayer + 1);
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
	billiardgl::updatePlayerAfterShot(Game);
}