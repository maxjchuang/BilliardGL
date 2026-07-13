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
#include "game_runtime.h"
#include "launch_options.h"
#include "automation_controller.h"
#include "automation_runner.h"
#include "stdio_transport.h"
#include "frame_timing.h"
#include "hud.h"
#include "input.h"
#include "particle_resources.h"
#include "platform_scroll.h"
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
#include <iostream>

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define TABLE_OUT_WIDTH 153
#define TABLE_OUT_LENGTH 281
#define TABLE_IN_WIDTH billiardgl::kTableInWidth
#define TABLE_IN_LENGTH billiardgl::kTableInLength
#define TABLE_HEIGHT billiardgl::kTableHeight
#define CUE_LENGTH 145.0f
#define POCKET_RADIUS billiardgl::kPocketRadius
#define ROOM_WIDTH 1000.0f
#define ROOM_LENGTH 1000.0f
#define ROOM_HEIGHT 400.0f
#define BALL_RADIUS billiardgl::kBallRadius

#define BMP_Header_Length 54
#define PI 3.1415926
#define L0 GL_LIGHT0
#define L1 GL_LIGHT1

static billiardgl::GameState Game;
static billiardgl::RenderResources Render;
static double LastIdleTimeSeconds = 0.0;
static float PhysicsTimeAccumulatorSeconds = 0.0f;
static const int kMaxPhysicsStepsPerIdle = 5;

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
static void platformScroll(int direction);
static void mouseMove(int x, int y);
void b_music();

int main(int argc, char* argv[])
{
	const billiardgl::LaunchOptions options = billiardgl::parseLaunchOptions(argc, argv);
	if (!options.ok) { std::fprintf(stderr, "%s\n", options.error.c_str()); return 2; }
	Game.config = options.runtime;
	if (options.mode == billiardgl::RunMode::AutomationHeadless)
	{
		billiardgl::GameRuntime runtime;
		billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Headless);
		billiardgl::StdioTransport transport(std::cin, std::cout);
		return billiardgl::runAutomation(transport, controller, "headless");
	}
	if (options.mode == billiardgl::RunMode::AutomationRendered)
	{
		int glutArgc = 1;
		char* glutArgv[] = {argv[0], nullptr};
		glutInit(&glutArgc, glutArgv);
		glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_STENCIL);
		initWindows();
		glutDisplayFunc(&myDisplay);
		billiardgl::GameRuntime runtime;
		if (!billiardgl::initializeRenderResources(Render, runtime.mutableState())) return 1;
		billiardgl::initializeParticleEmitters(Render, runtime.mutableState());
		billiardgl::setupLights();
		billiardgl::AutomationController controller(runtime, billiardgl::AutomationMode::Rendered);
		billiardgl::StdioTransport transport(std::cin, std::cout);
		const auto capture = [&runtime](const std::string& path) {
			Game = runtime.state();
			glViewport(0, 0, Game.config.width, Game.config.height);
			billiardgl::setupCameraFromGameState(Game);
			Render.shotPower = Game.input.shotPower;
			Render.showCue = Game.players.aimingAtCueBall;
			Render.showPowerMeter = Game.aim.mode == billiardgl::AimMode::Aim;
			Render.viewportWidth = Game.config.width;
			Render.viewportHeight = Game.config.height;
			billiardgl::renderScene(Game, Render);
			billiardgl::drawHud(Game);
			glFinish();
			const bool saved = billiardgl::saveFramebufferToPpm(path, Game.config.width, Game.config.height);
			glutSwapBuffers();
			return saved;
		};
		return billiardgl::runAutomation(transport, controller, "rendered", capture);
	}
	std::thread t(b_music);
	t.detach();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_STENCIL);
	initWindows();
	glutDisplayFunc(&myDisplay);
	billiardgl::installPlatformScrollHandler(platformScroll);
	initBall();//初始化球的位置
	if (!billiardgl::initializeRenderResources(Render, Game))
	{
		std::fprintf(stderr, "Failed to initialize render resources\n");
		return 1;
	}
	billiardgl::initializeParticleEmitters(Render, Game);
	LastIdleTimeSeconds = billiardgl::monotonicSeconds();

	prepareScreenshotScene();

	glutIdleFunc(&myIdle); //设置窗口刷新的回调函数
	glutKeyboardFunc(myKeyboard); //设置键盘回调函数
	glutSpecialFunc(mySpecialKeyboard);
	glutMouseFunc(myMouse); // mouse button callback
	glutMotionFunc(mouseMove); // mouse drag callback
	glutPassiveMotionFunc(mouseMove); // mouse/trackpad move callback
	glutReshapeFunc(myReshape);

	billiardgl::setupLights();
	myReshape(width, height);
	if (!Game.config.screenshotPath.empty())
	{
		myDisplay();
		return 0;
	}
	glutMainLoop();
	return 0;
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
		Game.aim.showGuideLine = true;
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
	Render.showPowerMeter = Game.aim.mode == billiardgl::AimMode::Aim;
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
	if (Game.camera.anchorMode == billiardgl::CameraAnchorMode::FollowCueBall)
	{
		Game.camera.target[0] = Game.balls[0].position.x;
		Game.camera.target[1] = Game.balls[0].position.y;
		Game.camera.target[2] = Game.balls[0].position.z;
	}
	Game.camera.eye[0] = Game.camera.zoom * (cos(Game.camera.angleX)) + Game.camera.target[0];
	Game.camera.eye[1] = Game.camera.zoom * (cos(Game.camera.angleY)) + Game.camera.target[1];
	Game.camera.eye[2] = Game.camera.zoom * (sin(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[2];


	if (Game.input.hitRequested == 1)
	{
		const billiardgl::Point3 velocity = billiardgl::shotVelocityFromAim(Game.aim.yaw, Game.input.shotPower);
		billiardgl::setBallVelocity(Game.balls[0], velocity.x, velocity.y, velocity.z);
		Game.players.nextPlayer = 1 - Game.players.currentPlayer;
		Game.players.illegalShot = false;
		Game.players.shotTaken = true;
		Game.players.updatedAfterShot = false;
		Game.ballsMoving = true;
		Game.camera.anchorMode = billiardgl::CameraAnchorMode::FreeLook;
		Game.transitionPerspective = false;
		Game.perspectiveRecorded = false;
		Game.aim.mode = billiardgl::AimMode::Observe;
		Game.players.aimingAtCueBall = false;
		Game.input.hitRequested = 0;
		billiardgl::playHit();
	}

	if (Game.input.leftMouseDown)
	{
		Game.balls[0].velocity.x = 0;
		Game.balls[0].velocity.z = 0;
	}

	const double currentIdleTimeSeconds = billiardgl::monotonicSeconds();
	const float elapsedSeconds = static_cast<float>(currentIdleTimeSeconds - LastIdleTimeSeconds);
	LastIdleTimeSeconds = currentIdleTimeSeconds;
	const billiardgl::FrameStepResult frameSteps = billiardgl::advanceFixedStepAccumulator(
		PhysicsTimeAccumulatorSeconds,
		elapsedSeconds,
		billiardgl::kDefaultTimeStep,
		kMaxPhysicsStepsPerIdle);
	for (int step = 0; step < frameSteps.steps; ++step)
	{
		billiardgl::updatePhysics(Game, billiardgl::kDefaultTimeStep);
		if (Game.events.ballCollision || Game.events.railCollision)
			billiardgl::playHit();
		if (Game.events.ballPocketed || Game.events.cueBallPocketed)
			billiardgl::playBallIn();
		if (Game.events.eightBallPocketed)
			billiardgl::playGameOver();
	}

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

	glutPostRedisplay();
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
	case 'c':
	case 'C':
		billiardgl::handleCameraAnchorToggleKey(Game);
		break;
	case ' ':
		billiardgl::handleCameraReturnToCueBallKey(Game);
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
	case '+':
		if (Game.aim.mode == billiardgl::AimMode::Aim)
			billiardgl::handleMouseWheel(Game, 1, 10.0f, 20.0f, 200.0f);
		break;
	case '-':
		if (Game.aim.mode == billiardgl::AimMode::Aim)
			billiardgl::handleMouseWheel(Game, -1, 10.0f, 20.0f, 200.0f);
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
	if (Game.ballsMoving)
	{
		return;
	}
	if (mbutton == 3 || mbutton == 4)
	{
		billiardgl::handleMouseWheel(Game, mbutton == 3 ? 1 : -1, 10.0f, 20.0f, 200.0f);
		return;
	}
	if (Game.camera.anchorMode == billiardgl::CameraAnchorMode::FollowCueBall)
	{
		Game.camera.target[0] = Game.balls[0].position.x;
		Game.camera.target[1] = Game.balls[0].position.y;
		Game.camera.target[2] = Game.balls[0].position.z;
	}
	Game.camera.eye[0] = Game.camera.zoom*(cos(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[0];
	Game.camera.eye[1] = Game.camera.zoom*(cos(Game.camera.angleY)) + Game.camera.target[1];
	Game.camera.eye[2] = Game.camera.zoom*(sin(Game.camera.angleX) * sin(Game.camera.angleY)) + Game.camera.target[2];
	if (mbutton == GLUT_LEFT_BUTTON && Game.input.cameraPan && mstate == GLUT_UP)
	{
		billiardgl::endCameraPan(Game);
		return;
	}
	const bool shiftDrag = (glutGetModifiers() & GLUT_ACTIVE_SHIFT) != 0;
	if (mbutton == GLUT_LEFT_BUTTON && shiftDrag)
	{
		if (mstate == GLUT_DOWN)
			billiardgl::beginCameraPan(Game, x, y);
		else
			billiardgl::endCameraPan(Game);
		return;
	}
	billiardgl::MouseButton button = billiardgl::MouseButton::Other;
	if (mbutton == GLUT_LEFT_BUTTON)
		button = billiardgl::MouseButton::Left;
	else if (mbutton == GLUT_RIGHT_BUTTON)
		button = billiardgl::MouseButton::Right;

	const billiardgl::ButtonState buttonState = mstate == GLUT_DOWN ? billiardgl::ButtonState::Down : billiardgl::ButtonState::Up;
	billiardgl::handleMouseButton(Game, button, buttonState, x, y);
}
static void platformScroll(int direction)
{
	if (Game.ballsMoving)
		return;

	billiardgl::handleMouseWheel(Game, direction, 10.0f, 20.0f, 200.0f);
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
