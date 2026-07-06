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
#include "particle_resources.h"
#include "physics.h"
#include "rules.h"
#include "platform_audio.h"
#include "platform_time.h"
#include "resource_path.h"
#include "renderer.h"
#include "render_resources.h"
#include "screenshot.h"
#include "shot.h"

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

static billiardgl::GameState Game;
static billiardgl::RenderResources Render;

//变量申明
int& width = Game.config.width;
int& height = Game.config.height;
int i = 0, j = 0, k = 0, ballcnt = 16;
static GLfloat M = 1, U = 0.2, T = 0.1, Radius = 5.715, G = -4;
GLfloat m[16];


//定义球及位置矢量结构体
struct Point
{
	GLfloat x;
	GLfloat y;
	GLfloat z;
};

// 载入纹理
// 场景绘制
void initBall();
void parseLaunchOptions(int argc, char* argv[]);
void prepareScreenshotScene();
void initWindows(void);
void myReshape(int w, int h);
void myDisplay(void);
void myIdle(void);
void updatePlayer();
// 光源
// 鼠标键盘操作
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

int main(int argc, char* argv[])
{
	parseLaunchOptions(argc, argv);
	std::thread t(b_music);
	t.detach();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_STENCIL);
	initWindows();
	initBall();//初始化球的位置
	if (!billiardgl::initializeRenderResources(Render, Game))
	{
		std::fprintf(stderr, "Failed to initialize render resources\n");
		return 1;
	}
	billiardgl::initializeParticleEmitters(Render, Game);

	prepareScreenshotScene();

	glutDisplayFunc(&myDisplay);
	glutIdleFunc(&myIdle); //设置窗口刷新的回调函数
	glutKeyboardFunc(myKeyboard); //设置键盘回调函数
	glutSpecialFunc(mySpecialKeyboard);
	glutMouseFunc(myMouse); //设置鼠标器按键回调函数
	glutMotionFunc(mouseMove); // mouse drag callback
	glutPassiveMotionFunc(mouseMove); // mouse/trackpad move callback
	glutReshapeFunc(myReshape);

	billiardgl::setupLights();
	glutMainLoop();
	return 0;
}

// 初始化窗口
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
			else if (strcmp(scene, "aim") == 0)
				Game.config.screenshotScene = billiardgl::ScreenshotScene::Aim;
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

	if (Game.config.screenshotScene == billiardgl::ScreenshotScene::Aim)
	{
		Game.aim.mode = billiardgl::AimMode::Aim;
		Game.players.aimingAtCueBall = true;
		Game.aim.yaw = billiardgl::kPi / 2.0f;
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
// 初始化球位置
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
	billiardgl::setupCameraFromGameState(Game);
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
	billiardgl::setupCameraFromGameState(Game);
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
// 设置视点
// 画房间
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
		const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(Game.aim.yaw, Game.input.shotPower);
		billiardgl::setBallVelocity(Game.balls[0], velocity.x, velocity.y, velocity.z);
		Game.players.shotTaken = true;
		Game.players.updatedAfterShot = false;
		Game.ballsMoving = true;
		Game.aim.mode = billiardgl::AimMode::Observe;
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
			Game.camera.recordedZoom = Game.camera.zoom;
			Game.camera.recordedTarget[0] = Game.balls[0].position.x;
			Game.camera.recordedTarget[1] = Game.balls[0].position.y;
			Game.camera.recordedTarget[2] = Game.balls[0].position.z;
			Game.camera.target[0] = Game.camera.recordedTarget[0];
			Game.camera.target[1] = Game.camera.recordedTarget[1];
			Game.camera.target[2] = Game.camera.recordedTarget[2];
			Game.perspectiveRecorded = true;
		}
		else
		{
			if (Game.camera.zoom < Game.camera.recordedZoom + 100)
				Game.camera.zoom += 1;
			else if (!Game.ballsMoving)
			{
				billiardgl::sleepMilliseconds(1000);
				Game.camera.zoom = Game.camera.recordedZoom;
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
// 球与球碰撞检测
static void myKeyboard(unsigned char key, int x, int y)
{
	if (key == 'h' || key == 'H')
	{
		billiardgl::handleHelpKey(Game);
		return;
	}
	if (key == '\t')
	{
		Game.input.mouseX = x;
		Game.input.mouseY = y;
		billiardgl::handleAimToggleKey(Game);
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
		billiardgl::toggleFired(Render, 0);
		break;
	case '1':
		billiardgl::toggleFired(Render, 1);
		break;
	case '2':
		billiardgl::toggleFired(Render, 2);
		break;
	case '3':
		billiardgl::toggleFired(Render, 3);
		break;
	case '4':
		billiardgl::toggleFired(Render, 4);
		break;
	case '5':
		billiardgl::toggleFired(Render, 5);
		break;
	case '6':
		billiardgl::toggleFired(Render, 6);
		break;
	case '7':
		billiardgl::toggleFired(Render, 7);
		break;
	case '8':
		billiardgl::toggleFired(Render, 8);
		break;
	case '9':
		billiardgl::toggleFired(Render, 9);
		break;
	case 'p':
		billiardgl::toggleFired(Render, 10);
		break;
	case 'o':
		billiardgl::toggleFired(Render, 11);
		break;
	case 'i':
		billiardgl::toggleFired(Render, 12);
		break;
	case 'u':
		billiardgl::toggleFired(Render, 13);
		break;
	case 'y':
		billiardgl::toggleFired(Render, 14);
		break;
	case 't':
		billiardgl::toggleFired(Render, 15);
		break;
	case 'f':
		billiardgl::toggleAllFired(Render);

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
	if (Game.ballsMoving)
		return;

	billiardgl::handleMouseMove(Game, x, y);
}
// 锟斤拷锟斤拷锟斤拷锟斤拷
void b_music()
{
	billiardgl::playBackgroundLoop();
}

void updatePlayer()
{
	billiardgl::updatePlayerAfterShot(Game);
}