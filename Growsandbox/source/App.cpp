/*
 *  App.cpp
 *  Created by Seth Robinson on 3/6/09.
 *  For license info, check the license.txt file that should have come with this.
 *
 */ 
#include "PlatformPrecomp.h"
#include "App.h"
#include "Entity/CustomInputComponent.h" //used for the back button (android)
#include "Entity/FocusInputComponent.h" //needed to let the input component see input messages
#include "Entity/ArcadeInputComponent.h"
#include <cmath>
//#include "util/TextScanner.h"
#include "Manager/MessageManager.h"
MessageManager g_messageManager;
MessageManager * GetMessageManager() {return &g_messageManager;}

FileManager g_fileManager;
FileManager * GetFileManager() {return &g_fileManager;}

#include "Audio/AudioManager.h"
AudioManager g_audioManager; //to disable sound, this is a dummy
AudioManager * GetAudioManager(){return &g_audioManager;}

#ifdef PLATFORM_OSX
// Required by MainController.mm and BaseApp.cpp - defined in SDL2Main.cpp for SDL builds
bool g_bIsFullScreen = false;
#endif

App *g_pApp = NULL;

BaseApp * GetBaseApp() 
{
	if (!g_pApp)
	{
		g_pApp = new App;
	}
	return g_pApp;
}

App * GetApp() 
{
	assert(g_pApp && "GetBaseApp must be called used first");
	return g_pApp;
}

App::App()
	: m_bDidPostInit(false)
	, m_inputLeft(false)
	, m_inputRight(false)
	, m_inputJump(false)
	, m_tilesLoaded(false)
{
}

App::~App()
{
}

bool App::Init()
{
	
	if (m_bInitted)	
	{
		return true;
	}
	
	if (!BaseApp::Init()) return false;
	
	if (GetEmulatedPlatformID() == PLATFORM_ID_IOS || GetEmulatedPlatformID() == PLATFORM_ID_WEBOS)
	{
		//SetLockedLandscape( true); //if we don't allow portrait mode for this game
		//SetManualRotationMode(true); //don't use manual, it may be faster (33% on a 3GS) but we want iOS's smooth rotations
	}

	LogMsg("The Save path is %s", GetSavePath().c_str());
	LogMsg("Region string is %s", GetRegionString().c_str());

#ifdef _DEBUG
	LogMsg("Built in debug mode");
#endif
#ifndef C_NO_ZLIB
	//fonts need zlib to decompress.  When porting a new platform I define C_NO_ZLIB and add zlib support later sometimes
	if (!GetFont(FONT_SMALL)->Load("interface/font_trajan.rtfont")) return false;
#endif

	GetBaseApp()->SetFPSVisible(true);
	return true;
}

void App::Kill()
{
	BaseApp::Kill();
}

void App::OnExitApp(VariantList *pVarList)
{
	LogMsg("Exiting the app");
	OSMessage o;
	o.m_type = OSMessage::MESSAGE_FINISH_APP;
	GetBaseApp()->AddOSMessage(o);
}

#define kFilteringFactor 0.1f
#define C_DELAY_BETWEEN_SHAKES_MS 500

//testing accelerometer readings. To enable the test, search below for "ACCELTEST"
//Note: You'll need to look at the  debug log to see the output. (For android, run PhoneLog.bat from RTBareBones/android)
void App::OnAccel(VariantList *pVList)
{
	
	if ( int(pVList->m_variant[0].GetFloat()) != MESSAGE_TYPE_GUI_ACCELEROMETER) return;

	CL_Vec3f v = pVList->m_variant[1].GetVector3();

	LogMsg("Accel: %s", PrintVector3(v).c_str());

	v.x = v.x * kFilteringFactor + v.x * (1.0f - kFilteringFactor);
	v.y = v.y * kFilteringFactor + v.y * (1.0f - kFilteringFactor);
	v.z = v.z * kFilteringFactor + v.z * (1.0f - kFilteringFactor);

	// Compute values for the three axes of the acceleromater
	float x = v.x - v.x;
	float y = v.y - v.x;
	float z = v.z - v.x;

	//Compute the intensity of the current acceleration 
	if (sqrt(x * x + y * y + z * z) > 2.0f)
	{
		Entity *pEnt = GetEntityRoot()->GetEntityByName("jumble");
		if (pEnt)
		{
			//GetAudioManager()->Play("audio/click.wav");
            VariantList vList(CL_Vec2f(), pEnt);
			pEnt->GetFunction("OnButtonSelected")->sig_function(&vList);
		}
		LogMsg("Shake!");
	}
}


//test for arcade keys.  To enable this test, search for TRACKBALL/ARCADETEST: below and uncomment the stuff under it.
//Note: You'll need to look at the debug log to see the output.  (For android, run PhoneLog.bat from RTBareBones/android)

void App::OnArcadeInput(VariantList *pVList)
{
	int vKey = pVList->Get(0).GetUINT32();
	eVirtualKeyInfo keyInfo = (eVirtualKeyInfo) pVList->Get(1).GetUINT32();
	bool pressed = (keyInfo == VIRTUAL_KEY_PRESS);

	switch (vKey)
	{
		case VIRTUAL_KEY_DIR_LEFT:
			m_inputLeft = pressed;
			break;
		case VIRTUAL_KEY_DIR_RIGHT:
			m_inputRight = pressed;
			break;
		case VIRTUAL_KEY_GAME_JUMP:
			if (pressed) m_inputJump = true;  // edge-trigger; consumed by App::Update
			break;
	}
}

void AppInputRawKeyboard(VariantList *pVList)
{
	 
    int vKey = pVList->Get(0).GetUINT32();
    eVirtualKeyInfo keyInfo = (eVirtualKeyInfo) pVList->Get(1).GetUINT32();
    
    string pressed;

    switch (keyInfo)
    {
        case VIRTUAL_KEY_PRESS:
            pressed = "pressed";
            break;

        case VIRTUAL_KEY_RELEASE:
            pressed = "released";
            break;

        default:
            LogMsg("AppInputRawKeyboard> Bad value of %d", keyInfo);
    }
    
    string keyName = "unknown";

    switch (vKey)
    {
        case VIRTUAL_KEY_DIR_LEFT:
            keyName = "Left";
            break;

        case VIRTUAL_KEY_DIR_UP:
            keyName = "Up";
            break;

        case VIRTUAL_KEY_DIR_RIGHT:
            keyName = "Right";
            break;

        case VIRTUAL_KEY_DIR_DOWN:
            keyName = "Down";
            break;

        case VIRTUAL_DPAD_BUTTON_DOWN:
            keyName = "Button Bottom";
            break;
        case VIRTUAL_DPAD_BUTTON_UP:
            keyName = "Button Top";
            break;
        case VIRTUAL_DPAD_BUTTON_LEFT:
            keyName = "Button Left";
            break;
        case VIRTUAL_DPAD_BUTTON_RIGHT:
            keyName = "Button Right";
            break;
        case VIRTUAL_DPAD_START:
            keyName = "Start";
            break;
        case VIRTUAL_DPAD_SELECT:
            keyName = "Select";
            break;
        case VIRTUAL_DPAD_RBUTTON:
            keyName = "R1";
            break;
        case VIRTUAL_DPAD_RTRIGGER:
            keyName = "R2";
            break;

        case VIRTUAL_DPAD_LBUTTON:
            keyName = "L1";
            break;
        case VIRTUAL_DPAD_LTRIGGER:
            keyName = "L2";
            break;

		case VIRTUAL_KEY_BACK:
		keyName = "Escape";
		GetApp()->OnExitApp(NULL);
		break;

    }
    
    LogMsg("MESSAGE_TYPE_GUI_CHAR_RAW: Hit %d (%s) (%s)", vKey, keyName.c_str(), pressed.c_str());
}

void AppInput(VariantList *pVList)
{

	//0 = message type, 1 = parent coordinate offset, 2 is fingerID
	eMessageType msgType = eMessageType( int(pVList->Get(0).GetFloat()));
	CL_Vec2f pt = pVList->Get(1).GetVector2();
	//pt += GetAlignmentOffset(*m_pSize2d, eAlignment(*m_pAlignment));

	
	uint32 fingerID = 0;
	if ( msgType != MESSAGE_TYPE_GUI_CHAR && pVList->Get(2).GetType() == Variant::TYPE_UINT32)
	{
		fingerID = pVList->Get(2).GetUINT32();
	}

	CL_Vec2f vLastTouchPt = GetBaseApp()->GetTouch(fingerID)->GetLastPos();
	 
	switch (msgType)
	{
	
	case MESSAGE_TYPE_GUI_CLICK_START:
		LogMsg("Touch start: X: %.2f Y: %.2f (Finger %d)", pt.x, pt.y, fingerID);
		break;
	case MESSAGE_TYPE_GUI_MOUSEWHEEL:
		LogMsg("Mouse wheel: Offet: %.2f (Finger %d)", pVList->Get(4).GetVector2().x, fingerID);
		break;

	case MESSAGE_TYPE_GUI_CLICK_MOVE_RAW:
		//LogMsg("Touch raw move: X: %.2f YL %.2f (Finger %d)", pt.x, pt.y, fingerID);
		break;
	case MESSAGE_TYPE_GUI_CLICK_END:
		LogMsg("Touch end: X: %.2f Y: %.2f (Finger %d)", pt.x, pt.y, fingerID);
		break;

	case MESSAGE_TYPE_GUI_CHAR:
		char key = (char)pVList->Get(2).GetUINT32();
		LogMsg("MESSAGE_TYPE_GUI_CHAR sent key %c (%d)", key, (int)key);
		break;
	}	
}


void App::Update()
{
	
	//game can think here.  The baseApp::Update() will run Update() on all entities, if any are added.  The only one
	//we use in this example is one that is watching for the Back (android) or Escape key to quit that we setup earlier.

	BaseApp::Update();

	if (!m_bDidPostInit)
	{
		//stuff I want loaded during the first "Update"
		m_bDidPostInit = true;
		
		//for android, so the back key (or escape on windows) will quit out of the game
		Entity *pEnt = GetEntityRoot()->AddEntity(new Entity);
		EntityComponent *pComp = pEnt->AddComponent(new CustomInputComponent);
		//tell the component which key has to be hit for it to be activated
		pComp->GetVar("keycode")->Set(uint32(VIRTUAL_KEY_BACK));
		//attach our function so it is called when the back key is hit
		pComp->GetFunction("OnActivated")->sig_function.connect(1, boost::bind(&App::OnExitApp, this, _1));

		//nothing will happen unless we give it input focus
		pEnt->AddComponent(new FocusInputComponent);

		//ACCELTEST:  To test the accelerometer uncomment below: (will print values to the debug output)
		//SetAccelerometerUpdateHz(25); //default is 0, disabled
		//GetBaseApp()->m_sig_accel.connect(1, boost::bind(&App::OnAccel, this, _1));

		//TRACKBALL/ARCADETEST: Uncomment below to see log messages on trackball/key movement input

		pComp = pEnt->AddComponent(new ArcadeInputComponent);
		GetBaseApp()->m_sig_arcade_input.connect(1, boost::bind(&App::OnArcadeInput, this, _1));
	
		//these arrow keys will be triggered by the keyboard, if applicable
		AddKeyBinding(pComp, "Left", VIRTUAL_KEY_DIR_LEFT, VIRTUAL_KEY_DIR_LEFT);
		AddKeyBinding(pComp, "Right", VIRTUAL_KEY_DIR_RIGHT, VIRTUAL_KEY_DIR_RIGHT);
		AddKeyBinding(pComp, "Up", VIRTUAL_KEY_DIR_UP, VIRTUAL_KEY_DIR_UP);
		AddKeyBinding(pComp, "Down", VIRTUAL_KEY_DIR_DOWN, VIRTUAL_KEY_DIR_DOWN);
		AddKeyBinding(pComp, "Fire", VIRTUAL_KEY_CONTROL, VIRTUAL_KEY_GAME_FIRE);
		AddKeyBinding(pComp, "Jump", VIRTUAL_KEY_DIR_UP, VIRTUAL_KEY_GAME_JUMP);

		//INPUT TEST - wire up input to some functions to manually handle.  AppInput will use LogMsg to
		//send them to the log.  (Each device has a way to view a debug log in real-time)
		GetBaseApp()->m_sig_input.connect(&AppInput);

		//this one gives raw up and down of keyboard events, where the one above only gives
		//MESSAGE_TYPE_GUI_CHAR which is just the down and includes keyboard repeats from
		//holding the key
		GetBaseApp()->m_sig_raw_keyboard.connect(&AppInputRawKeyboard);

	}

	// Phase 1: Drive Player + Camera per frame
	float dt = GetBaseApp()->GetElapsedTime();  // delta in seconds

	m_player.SetInput(m_inputLeft, m_inputRight, m_inputJump);
	m_player.Update(dt);

	m_camera.SetTarget(m_player.GetPosition());
	m_camera.Update(dt);

	m_inputJump = false;  // consume edge-trigger after Player has read it

	//game is thinking.
}

void App::Draw()
{
	PrepareForGL();

	// Sky — clear to light blue
	glClearColor(0.4f, 0.7f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	CLEAR_GL_ERRORS()

	// Ground — tiled with Kenney pixel-platformer tiles
	if (!m_tilesLoaded)
	{
		m_tileGrass.LoadFile("tile_grass.rttex");
		m_tileDirt.LoadFile("tile_dirt.rttex");
		m_tilesLoaded = true;
	}

	if (m_tileGrass.IsLoaded() && m_tileDirt.IsLoaded())
	{
		const float TILE_SIZE = 36.0f;        // 18x18 native scaled 2x
		const float GROUND_Y_WORLD = 500.0f;  // matches Player::GROUND_Y

		// Native sprite size for sampling
		float texW = (float)m_tileGrass.GetWidth();
		float texH = (float)m_tileGrass.GetHeight();
		rtRectf srcRect(0.0f, 0.0f, texW, texH);

		// Determine which world X column the camera leftmost screen edge is at
		CL_Vec2f camPos = m_camera.GetPosition();
		float screenW = GetScreenSizeXf();
		float screenH = GetScreenSizeYf();

		float worldLeft = camPos.x - screenW * 0.5f;
		float worldRight = camPos.x + screenW * 0.5f;

		// Snap to tile grid: first tile column visible
		int firstCol = (int)std::floor(worldLeft / TILE_SIZE) - 1;  // -1 buffer
		int lastCol  = (int)std::floor(worldRight / TILE_SIZE) + 1;

		// Number of dirt rows below the grass surface (enough to cover bottom of screen)
		int dirtRows = (int)std::ceil((screenH * 0.5f + (screenH * 0.5f - GROUND_Y_WORLD + camPos.y)) / TILE_SIZE) + 2;
		if (dirtRows < 8) dirtRows = 8;  // floor: at least 8 rows worth of dirt below

		for (int col = firstCol; col <= lastCol; col++)
		{
			float worldX = col * TILE_SIZE;

			// Surface tile (grass) at top
			CL_Vec2f surfaceWorldPos(worldX, GROUND_Y_WORLD);
			CL_Vec2f surfaceScreenPos = m_camera.WorldToScreen(surfaceWorldPos);
			rtRectf dstSurface(
				surfaceScreenPos.x, surfaceScreenPos.y,
				surfaceScreenPos.x + TILE_SIZE, surfaceScreenPos.y + TILE_SIZE
			);
			m_tileGrass.BlitEx(dstSurface, srcRect);

			// Dirt fill rows below
			for (int row = 1; row <= dirtRows; row++)
			{
				CL_Vec2f dirtWorldPos(worldX, GROUND_Y_WORLD + row * TILE_SIZE);
				CL_Vec2f dirtScreenPos = m_camera.WorldToScreen(dirtWorldPos);
				rtRectf dstDirt(
					dirtScreenPos.x, dirtScreenPos.y,
					dirtScreenPos.x + TILE_SIZE, dirtScreenPos.y + TILE_SIZE
				);
				m_tileDirt.BlitEx(dstDirt, srcRect);
			}
		}
	}

	// Player — delegates to Player::Draw which uses camera transform
	m_player.Draw(m_camera);

	// Debug overlay
	CL_Vec2f pos = m_player.GetPosition();
	CL_Vec2f vel = m_player.GetVelocity();
	char debugBuf[256];
	snprintf(debugBuf, sizeof(debugBuf),
		"Pos: (%.0f, %.0f)  Vel: (%.0f, %.0f)  OnGround: %d",
		pos.x, pos.y, vel.x, vel.y, m_player.IsOnGround() ? 1 : 0);
	GetFont(FONT_SMALL)->Draw(10.0f, 10.0f, debugBuf);

	// Base handles built-in GUI overlay (FPS counter, etc.)
	BaseApp::Draw();
}

void App::OnScreenSizeChange()
{
	BaseApp::OnScreenSizeChange();
}

void App::OnEnterBackground()
{
	//save your game stuff here, as on some devices (Android <cough>) we never get another notification of quitting.
	LogMsg("Entered background");
	BaseApp::OnEnterBackground();
}

void App::OnEnterForeground()
{
	LogMsg("Entered foreground");
	BaseApp::OnEnterForeground();
}

const char * GetAppName() {return "Growsandbox";}

//the stuff below is for android/webos builds.  Your app needs to be named like this.

//note: these are put into vars like this to be compatible with my command-line parsing stuff that grabs the vars

const char * GetBundlePrefix()
{
	const char * bundlePrefix = "com.rtsoft.";
	return bundlePrefix;
}

const char * GetBundleName()

{
	const char * bundleName = "Growsandbox";
	return bundleName;
}

bool App::OnPreInitVideo()
{
	//only called for desktop systems
	//override in App.* if you want to do something here.  You'd have to
	//extern these vars from main.cpp to change them...

	//SetEmulatedPlatformID(PLATFORM_ID_WINDOWS);
#if defined (_DEBUG) && defined(WINAPI)
	SetupScreenInfo(1024, 768, ORIENTATION_DONT_CARE);
#endif
	
	//g_winVideoScreenY = 768;
	return true; //no error
}
