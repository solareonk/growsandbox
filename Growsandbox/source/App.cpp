/*
 *  App.cpp
 *  Created by Seth Robinson on 3/6/09.
 *  For license info, check the license.txt file that should have come with this.
 *
 */ 
#include "PlatformPrecomp.h"
#include "App.h"
#include "TileRegistry.h"
#include "Autotile.h"
#include "SaveManager.h"
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

// Phase 3a: Show a Windows MessageBox for fatal init errors.
// Keeps the failure message visible even if console is hidden.
static void ShowFatalError(const char* msg)
{
	LogError("FATAL: %s", msg);
#ifdef _WIN32
	MessageBoxA(NULL, msg, "Growsandbox - Fatal Error", MB_ICONERROR | MB_OK);
#else
	fprintf(stderr, "FATAL: %s\n", msg);
#endif
}

App *g_pApp = NULL;

static Surface g_crackOverlay;
static bool    g_crackOverlayLoaded = false;
static bool    g_crackOverlayTried  = false;

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

static Surface* GetCrackOverlay()
{
	if (!g_crackOverlayTried)
	{
		g_crackOverlay.LoadFile("crack_overlay.rttex");
		g_crackOverlayLoaded = g_crackOverlay.IsLoaded();
		g_crackOverlayTried = true;
	}
	return g_crackOverlayLoaded ? &g_crackOverlay : NULL;
}

App::App()
	: m_bDidPostInit(false)
	, m_inputLeft(false)
	, m_inputRight(false)
	, m_inputJump(false)
	, m_worldGenerated(false)
	, m_mousePos(0.0f, 0.0f)
	, m_mouseDown(false)
	, m_handleDragging(false)
	, m_handleDragStartY(0.0f)
	, m_backpackAnim(0.0f)
{
}

// Phase 3b: animated UI positions — both hotbar and backpack lerp from
// closed (bottom of screen) to open (lifted up) using m_backpackAnim.
// Smooth ease: 3t^2 - 2t^3.
int App::GetHotbarY() const
{
	const int CLOSED_Y = 708;          // normal bottom-of-screen position
	const int OPEN_Y   = 540;          // lifted up to make room for backpack above
	float t = m_backpackAnim;
	float ease = t * t * (3.0f - 2.0f * t);
	return (int)(CLOSED_Y + (OPEN_Y - CLOSED_Y) * ease);
}

int App::GetBackpackPanelY() const
{
	// Phase 3b: panel sits BELOW hotbar (Growtopia-style).
	// Hotbar Y_open=540, hotbar height=48, gap=12 → panel top = 600.
	// Panel height = 24 (title) + 144 (grid) = 168 → panel bottom = 768 (screen edge).
	const int CLOSED_Y = 768 + 168;    // offscreen below screen entirely
	const int OPEN_Y   = 600;          // just below hotbar when fully open
	float t = m_backpackAnim;
	float ease = t * t * (3.0f - 2.0f * t);
	return (int)(CLOSED_Y + (OPEN_Y - CLOSED_Y) * ease);
}

// Phase 3b: drag-gesture end handler (Grab-app style).
// Threshold: 20px swipe up = open backpack, 20px swipe down = close.
// Click without drag (delta < threshold) = no-op.
void App::EndHandleDrag(float endY)
{
	if (!m_handleDragging) return;
	float delta = m_handleDragStartY - endY;  // positive = moved up
	const float THRESHOLD = 20.0f;
	if (delta > THRESHOLD)
	{
		if (!m_inventory.IsBackpackOpen()) m_inventory.ToggleBackpack();
	}
	else if (delta < -THRESHOLD)
	{
		if (m_inventory.IsBackpackOpen()) m_inventory.CloseBackpack();
	}
	m_handleDragging = false;
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

	if (!TileRegistry_Load("items.dat"))
	{
		ShowFatalError("Failed to load items.dat.\n\n"
		               "Run: py script/encode_items.py\n"
		               "Then re-launch.");
		return false;
	}

	Autotile::Init();
	if (!Autotile::SelfTest())
	{
		ShowFatalError("Autotile::SelfTest failed. See log for detail.");
		return false;
	}

	// Phase 3d: validate save serializer at startup.
	SaveManager::SelfTest();

	GetBaseApp()->SetFPSVisible(true);
	return true;
}

void App::Kill()
{
	// Phase 3d: persist state before shutdown. Failure logs but does not block close.
	if (m_worldGenerated)
	{
		SaveManager::Save(m_world, m_inventory, m_player);
	}
	TileRegistry_Shutdown();
	if (g_crackOverlayLoaded)
	{
		g_crackOverlay.Kill();
		g_crackOverlayLoaded = false;
		g_crackOverlayTried = false;
	}
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

        // Phase 3b: hotbar slot selection (keys 1-4) and backpack toggle (E).
        // Windows raw keyboard delivers digit keys as ASCII codes ('1'..'4'),
        // which are unchanged by ConvertWindowsKeycodeToProtonVirtualKey().
        case '1':
            if (keyInfo == VIRTUAL_KEY_PRESS) GetApp()->GetInventory().SetSelectedHotbarSlot(0);
            keyName = "1 (slot 0)";
            break;
        case '2':
            if (keyInfo == VIRTUAL_KEY_PRESS) GetApp()->GetInventory().SetSelectedHotbarSlot(1);
            keyName = "2 (slot 1)";
            break;
        case '3':
            if (keyInfo == VIRTUAL_KEY_PRESS) GetApp()->GetInventory().SetSelectedHotbarSlot(2);
            keyName = "3 (slot 2)";
            break;
        case '4':
            if (keyInfo == VIRTUAL_KEY_PRESS) GetApp()->GetInventory().SetSelectedHotbarSlot(3);
            keyName = "4 (slot 3)";
            break;
        case '5':
            if (keyInfo == VIRTUAL_KEY_PRESS) GetApp()->GetInventory().SetSelectedHotbarSlot(4);
            keyName = "5 (slot 4)";
            break;
        case 'R':
        case 'r':
            if (keyInfo == VIRTUAL_KEY_PRESS)
            {
                GetApp()->GetWorld().GenerateInitial();
                GetApp()->GetWorld().ClearDrops();   // Phase 3b ext: also clear drops
                GetApp()->GetInventory().Clear();    // Phase 3b: R also clears inventory
            }
            keyName = "R (Reset)";
            break;

        case VIRTUAL_KEY_BACK:
            keyName = "Escape";
            if (keyInfo == VIRTUAL_KEY_PRESS)
            {
                if (GetApp()->GetInventory().IsBackpackOpen())
                {
                    GetApp()->GetInventory().CloseBackpack();
                }
                else
                {
                    GetApp()->OnExitApp(NULL);
                }
            }
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
	{
		GetApp()->SetMousePos(pt);
		App* app = GetApp();
		Inventory& inv = app->GetInventory();

		if (fingerID == 0)
		{
			// Left click
			int mx = (int)pt.x;
			int my = (int)pt.y;

			// Phase 3b UI: grab handle bar above hotbar — drag up/down to toggle backpack
			if (app->HitTestBackpackHandle(mx, my))
			{
				app->SetHandleDragStart((float)my);
				app->SetMouseDown(false);
				break;
			}

			// Hotbar always clickable
			int hbIdx = app->HitTestHotbarSlot(mx, my);
			if (hbIdx >= 0)
			{
				inv.SetSelectedHotbarSlot(hbIdx);
				app->SetMouseDown(false);
				break;
			}

			if (inv.IsBackpackOpen())
			{
				int bpIdx = app->HitTestBackpackSlot(mx, my);
				if (bpIdx >= 0)
				{
					inv.ClickBackpackSlot(bpIdx);
					app->SetMouseDown(false);
					break;
				}
				// Absorb clicks on panel chrome (title bar, background)
				if (app->HitTestBackpackPanel(mx, my))
				{
					app->SetMouseDown(false);
					break;
				}
				// Click in world area (above panel) — propagate to punch/place
			}

			// No UI element hit → world click
			app->SetMouseDown(true);
		}
		else
		{
			// Right click (fingerID == 1 on Windows)
			int mx = (int)pt.x;
			int my = (int)pt.y;
			int hbIdx = app->HitTestHotbarSlot(mx, my);
			if (hbIdx > 0)  // slot 0 (FIST) is index 0, skip
			{
				inv.RightClickHotbarSlot(hbIdx);
			}
		}
		break;
	}
	case MESSAGE_TYPE_GUI_MOUSEWHEEL:
	{
		// pVList->Get(4) is the wheel delta (positive = scroll up, negative = scroll down)
		float delta = pVList->Get(4).GetFloat();
		if (delta != 0.0f)
		{
			int dir = (delta > 0) ? -1 : 1;  // scroll up = previous slot
			GetApp()->GetInventory().CycleSelected(dir);
		}
		LogMsg("Mouse wheel: delta %.2f", delta);
		break;
	}

	case MESSAGE_TYPE_GUI_CLICK_MOVE_RAW:
		GetApp()->SetMousePos(pt);
		break;
	case MESSAGE_TYPE_GUI_CLICK_END:
		GetApp()->SetMousePos(pt);
		GetApp()->SetMouseDown(false);
		// Phase 3b: finalize handle drag if in progress
		if (GetApp()->IsHandleDragging())
		{
			GetApp()->EndHandleDrag(pt.y);
		}
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

	if (!m_worldGenerated)
	{
		// Phase 3d: try to load saved state; fall back to fresh world if no save exists.
		if (!SaveManager::TryLoad(m_world, m_inventory, m_player))
		{
			m_world.GenerateInitial();
		}
		m_player.SetWorld(&m_world);
		m_worldGenerated = true;
	}

	// Phase 1: Drive Player + Camera per frame
	float dt = GetBaseApp()->GetElapsedTime();  // delta in seconds

	m_player.SetInput(m_inputLeft, m_inputRight, m_inputJump);
	m_player.Update(dt);

	m_camera.SetTarget(m_player.GetPosition());
	m_camera.Update(dt);

	// Phase 3b: interaction always runs — player can punch tiles even with
	// backpack open, as long as the click landed in the world area (not on UI).
	// Click absorption happens in the click handler.
	m_interaction.Update(m_world, m_player, m_camera,
	                     m_mousePos, m_mouseDown, m_inventory, dt);

	// Phase 3b extension: tick floating drops (gravity + auto-pickup on player overlap)
	m_world.UpdateDrops(dt, m_player, m_inventory);

	// Phase 3b: tick backpack slide animation toward target (4.0/sec = 250ms full transition)
	float target = m_inventory.IsBackpackOpen() ? 1.0f : 0.0f;
	const float ANIM_RATE = 4.0f;
	if (m_backpackAnim < target)
	{
		m_backpackAnim += ANIM_RATE * dt;
		if (m_backpackAnim > target) m_backpackAnim = target;
	}
	else if (m_backpackAnim > target)
	{
		m_backpackAnim -= ANIM_RATE * dt;
		if (m_backpackAnim < target) m_backpackAnim = target;
	}

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

	// Phase 2: render World grid (BG + FG layers, camera-culled)
	{
		const float TILE = (float)World::TILE_SIZE_PX;
		CL_Vec2f camPos = m_camera.GetPosition();
		float screenW = GetScreenSizeXf();
		float screenH = GetScreenSizeYf();

		float worldLeft  = camPos.x - screenW * 0.5f;
		float worldRight = camPos.x + screenW * 0.5f;
		float worldTop   = camPos.y - screenH * 0.5f;
		float worldBot   = camPos.y + screenH * 0.5f;

		int firstCol = (int)std::floor(worldLeft  / TILE) - 1;
		int lastCol  = (int)std::floor(worldRight / TILE) + 1;
		int firstRow = (int)std::floor(worldTop   / TILE) - 1;
		int lastRow  = (int)std::floor(worldBot   / TILE) + 1;

		if (firstCol < 0) firstCol = 0;
		if (lastCol  >= World::WIDTH)  lastCol  = World::WIDTH  - 1;
		if (firstRow < 0) firstRow = 0;
		if (lastRow  >= World::HEIGHT) lastRow  = World::HEIGHT - 1;

		for (int y = firstRow; y <= lastRow; y++)
		{
			for (int x = firstCol; x <= lastCol; x++)
			{
				const Cell& c = m_world.GetCell(x, y);
				CL_Vec2f cellWorld = World::CellToWorld(x, y);
				CL_Vec2f cellScreen = m_camera.WorldToScreen(cellWorld);
				rtRectf dst(cellScreen.x, cellScreen.y,
				            cellScreen.x + TILE, cellScreen.y + TILE);

				// 1. BG layer (rendered first, darkened by overlay below)
				if (c.bg.type != TILE_AIR)
				{
					Surface* bgSurf = GetTileSurface(c.bg.type);
					if (bgSurf)
					{
						const TileType& tt = GetTileType(c.bg.type);
						const float CELL = (float)World::TILE_SIZE_PX;
						int cellIdx = c.bg_variant;
						int srcCol = tt.anchor_col + (cellIdx % 8);
						int srcRow = tt.anchor_row + (cellIdx / 8);
						rtRectf src((float)srcCol * CELL, (float)srcRow * CELL,
						            (float)(srcCol + 1) * CELL, (float)(srcRow + 1) * CELL);
						bgSurf->BlitEx(dst, src);
						DrawFilledRect(dst.left, dst.top,
						               dst.right - dst.left, dst.bottom - dst.top,
						               MAKE_RGBA(0, 0, 0, 80));
					}
				}

				// 2. FG layer (drawn on top)
				if (c.fg.type != TILE_AIR)
				{
					Surface* fgSurf = GetTileSurface(c.fg.type);
					if (fgSurf)
					{
						const TileType& tt = GetTileType(c.fg.type);
						const float CELL = (float)World::TILE_SIZE_PX;
						int cellIdx = c.fg_variant;
						int srcCol = tt.anchor_col + (cellIdx % 8);
						int srcRow = tt.anchor_row + (cellIdx / 8);
						rtRectf src((float)srcCol * CELL, (float)srcRow * CELL,
						            (float)(srcCol + 1) * CELL, (float)(srcRow + 1) * CELL);
						fgSurf->BlitEx(dst, src);
					}
				}

				// Crack overlay — render on whichever layer (FG or BG) is being damaged
				const Tile* damaged = NULL;
				if (c.fg.type != TILE_AIR)
				{
					const TileType& fgMeta = GetTileType(c.fg.type);
					if (fgMeta.maxHp > 0 && c.fg.hp < fgMeta.maxHp)
					{
						damaged = &c.fg;
					}
				}
				else if (c.bg.type != TILE_AIR)
				{
					const TileType& bgMeta = GetTileType(c.bg.type);
					if (bgMeta.maxHp > 0 && c.bg.hp < bgMeta.maxHp)
					{
						damaged = &c.bg;
					}
				}

				if (damaged)
				{
					const TileType& meta = GetTileType(damaged->type);
					// hp range 0..maxHp; stage range 1..4 (skip 0 = uncracked)
					int stage = 4 - (int)((float)damaged->hp / (float)meta.maxHp * 4.0f);
					if (stage < 1) stage = 1;
					if (stage > 4) stage = 4;

					Surface* crack = GetCrackOverlay();
					if (crack)
					{
						const float FRAME_W = (float)World::TILE_SIZE_PX;
						const float FRAME_H = (float)World::TILE_SIZE_PX;
						rtRectf src((float)stage * FRAME_W, 0.0f,
						            ((float)stage + 1) * FRAME_W, FRAME_H);
						crack->BlitEx(dst, src);
					}
					else
					{
						// Fallback: dim the cell to show damage
						uint32 alpha = (uint32)(stage * 50);
						DrawFilledRect(dst.left, dst.top, TILE, TILE, MAKE_RGBA(0, 0, 0, alpha));
					}
				}
			}
		}
	}

	// Phase 3b extension: floating world drops (between tiles and player)
	DrawDrops();

	// Player — delegates to Player::Draw which uses camera transform
	m_player.Draw(m_camera);

	// Phase 2: aim outline
	if (m_interaction.HasAim())
	{
		int cx = m_interaction.GetAimX();
		int cy = m_interaction.GetAimY();
		const float TILE = (float)World::TILE_SIZE_PX;

		CL_Vec2f cellWorld = World::CellToWorld(cx, cy);
		CL_Vec2f cellScreen = m_camera.WorldToScreen(cellWorld);

		uint32 color = m_interaction.IsAimInReach()
		    ? MAKE_RGBA(255, 255, 255, 200)
		    : MAKE_RGBA(255, 60, 60, 200);

		// Draw outline as 4 thin filled rects (1 px borders)
		DrawFilledRect(cellScreen.x, cellScreen.y, TILE, 1.0f, color);
		DrawFilledRect(cellScreen.x, cellScreen.y + TILE - 1.0f, TILE, 1.0f, color);
		DrawFilledRect(cellScreen.x, cellScreen.y, 1.0f, TILE, color);
		DrawFilledRect(cellScreen.x + TILE - 1.0f, cellScreen.y, 1.0f, TILE, color);
	}

	DrawHotbar();
	DrawBackpack();

	// Debug overlay
	CL_Vec2f pos = m_player.GetPosition();
	CL_Vec2f vel = m_player.GetVelocity();

	// Phase 3b: show selected item from Inventory in 3-state format
	char selBuf[64];
	int selSlot = m_inventory.GetSelectedHotbarSlot();
	if (selSlot == 0)
	{
		snprintf(selBuf, sizeof(selBuf), "FIST");
	}
	else
	{
		const InventorySlot& s = m_inventory.GetHotbarSlot(selSlot);
		if (s.type == TILE_AIR || s.count == 0)
		{
			snprintf(selBuf, sizeof(selBuf), "FIST (slot %d empty)", selSlot);
		}
		else
		{
			snprintf(selBuf, sizeof(selBuf), "%s (slot %d, count %d)",
			         GetTileType(s.type).name, selSlot, (int)s.count);
		}
	}

	char debugBuf[256];
	snprintf(debugBuf, sizeof(debugBuf),
		"Pos: (%.0f, %.0f)  Vel: (%.0f, %.0f)  OnGround: %d  Selected: %s  Aim: (%d, %d) %s",
		pos.x, pos.y, vel.x, vel.y, m_player.IsOnGround() ? 1 : 0, selBuf,
		m_interaction.GetAimX(), m_interaction.GetAimY(),
		m_interaction.IsAimInReach() ? "REACH" : "OUT");
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
	BaseApp::OnEnterBackground();
	// Phase 3d: mobile lifecycle save (home button, screen lock, etc.)
	if (m_worldGenerated)
	{
		SaveManager::Save(m_world, m_inventory, m_player);
	}
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

int App::HitTestHotbarSlot(int mx, int my)
{
    const int SLOT_SIZE   = 48;
    const int HOTBAR_X    = (1024 - 5 * SLOT_SIZE) / 2;  // 392
    const int HOTBAR_Y    = GetHotbarY();                 // animated
    if (my < HOTBAR_Y || my >= HOTBAR_Y + SLOT_SIZE) return -1;
    if (mx < HOTBAR_X || mx >= HOTBAR_X + 5 * SLOT_SIZE) return -1;
    return (mx - HOTBAR_X) / SLOT_SIZE;
}

// Phase 3b UI: drag handle bar above hotbar (Grab-app style swipe).
// Hit zone follows the animated hotbar position.
bool App::HitTestBackpackHandle(int mx, int my)
{
    const int HANDLE_HIT_W = 200;
    const int HANDLE_HIT_H = 32;
    const int HANDLE_X     = (1024 - HANDLE_HIT_W) / 2;
    const int HOTBAR_Y     = GetHotbarY();
    const int HANDLE_Y     = HOTBAR_Y - HANDLE_HIT_H;     // just above animated hotbar
    if (my < HANDLE_Y || my >= HANDLE_Y + HANDLE_HIT_H) return false;
    if (mx < HANDLE_X || mx >= HANDLE_X + HANDLE_HIT_W) return false;
    return true;
}

int App::HitTestBackpackSlot(int mx, int my)
{
    if (!m_inventory.IsBackpackOpen()) return -1;
    const int SLOT_SIZE = 48;
    const int BP_X      = (1024 - 10 * SLOT_SIZE) / 2;  // 272
    const int TITLE_H   = 24;
    const int BP_Y      = GetBackpackPanelY();           // animated
    int gridY = BP_Y + TITLE_H;
    if (my < gridY || my >= gridY + 3 * SLOT_SIZE) return -1;
    if (mx < BP_X || mx >= BP_X + 10 * SLOT_SIZE) return -1;
    int col = (mx - BP_X) / SLOT_SIZE;
    int row = (my - gridY) / SLOT_SIZE;
    return row * 10 + col;
}

// Phase 3b: any click within the backpack panel rectangle (title bar + grid).
// Used to absorb clicks on chrome (not just slots) so they don't propagate to world.
bool App::HitTestBackpackPanel(int mx, int my)
{
    if (!m_inventory.IsBackpackOpen()) return false;
    const int SLOT_SIZE = 48;
    const int BP_X      = (1024 - 10 * SLOT_SIZE) / 2;
    const int TITLE_H   = 24;
    const int BP_Y      = GetBackpackPanelY();
    const int PANEL_W   = 10 * SLOT_SIZE;
    const int PANEL_H   = TITLE_H + 3 * SLOT_SIZE;
    return mx >= BP_X && mx < BP_X + PANEL_W
        && my >= BP_Y && my < BP_Y + PANEL_H;
}

void App::DrawHotbar()
{
    const int SLOT_SIZE   = 48;
    const int HOTBAR_X    = (1024 - 5 * SLOT_SIZE) / 2;  // 392
    const int HOTBAR_Y    = GetHotbarY();                 // animated

    // Phase 3b UI: grab-handle bar above hotbar (Grab-app style drag-to-toggle).
    // Visual is thick + tray background to make swipe affordance obvious.
    {
        const int HANDLE_W = 160;
        const int HANDLE_X = (1024 - HANDLE_W) / 2;
        const int HANDLE_Y = HOTBAR_Y - 16;     // ~16 px gap above hotbar
        // Tray area (matches hit zone, semi-transparent)
        DrawFilledRect((float)(HANDLE_X - 20), (float)(HANDLE_Y - 8),
                       (float)(HANDLE_W + 40), 28.0f,
                       MAKE_RGBA(0, 0, 0, 140));
        // The handle bar itself — thicker (8px) and brighter when backpack open
        uint32 barColor = m_inventory.IsBackpackOpen()
            ? MAKE_RGBA(255, 220, 60, 255)   // yellow when open
            : MAKE_RGBA(200, 200, 200, 240); // light gray when closed
        DrawFilledRect((float)HANDLE_X, (float)HANDLE_Y,
                       (float)HANDLE_W, 8.0f, barColor);
    }

    // Background panel — semi-transparent dark
    DrawFilledRect((float)HOTBAR_X - 4, (float)HOTBAR_Y - 4,
                   (float)(5 * SLOT_SIZE + 8), (float)(SLOT_SIZE + 8),
                   MAKE_RGBA(0, 0, 0, 180));

    int selected = m_inventory.GetSelectedHotbarSlot();

    for (int i = 0; i < Inventory::HOTBAR_SLOTS; i++)
    {
        int sx = HOTBAR_X + i * SLOT_SIZE;
        int sy = HOTBAR_Y;

        // Slot background
        DrawFilledRect((float)sx, (float)sy,
                       (float)SLOT_SIZE, (float)SLOT_SIZE,
                       MAKE_RGBA(40, 40, 40, 200));

        // Slot border (1px)
        DrawFilledRect((float)sx, (float)sy, (float)SLOT_SIZE, 1.0f, MAKE_RGBA(80, 80, 80, 255));
        DrawFilledRect((float)sx, (float)(sy + SLOT_SIZE - 1), (float)SLOT_SIZE, 1.0f, MAKE_RGBA(80, 80, 80, 255));
        DrawFilledRect((float)sx, (float)sy, 1.0f, (float)SLOT_SIZE, MAKE_RGBA(80, 80, 80, 255));
        DrawFilledRect((float)(sx + SLOT_SIZE - 1), (float)sy, 1.0f, (float)SLOT_SIZE, MAKE_RGBA(80, 80, 80, 255));

        // Slot content
        const InventorySlot& s = m_inventory.GetHotbarSlot(i);
        if (i == 0)
        {
            // FIST slot — magenta-fallback rect with "FIST" label centered
            DrawFilledRect((float)(sx + 8), (float)(sy + 8), 32.0f, 32.0f, MAKE_RGBA(200, 200, 200, 255));
            GetFont(FONT_SMALL)->Draw((float)(sx + 13), (float)(sy + 18), "FIST");
        }
        else if (s.type != TILE_AIR && s.count > 0)
        {
            // Item slot — tile texture + count
            Surface* surf = GetTileSurface(s.type);
            if (surf)
            {
                const TileType& tt = GetTileType(s.type);
                const float CELL = (float)World::TILE_SIZE_PX;
                int srcCol = tt.anchor_col;
                int srcRow = tt.anchor_row;
                rtRectf dst((float)(sx + 8), (float)(sy + 8),
                            (float)(sx + 8 + 32), (float)(sy + 8 + 32));
                rtRectf src((float)srcCol * CELL, (float)srcRow * CELL,
                            (float)(srcCol + 1) * CELL, (float)(srcRow + 1) * CELL);
                surf->BlitEx(dst, src);
            }
            else
            {
                DrawFilledRect((float)(sx + 8), (float)(sy + 8), 32.0f, 32.0f, MAKE_RGBA(255, 0, 255, 255));
            }
            char countBuf[8];
            snprintf(countBuf, sizeof(countBuf), "%d", (int)s.count);
            GetFont(FONT_SMALL)->Draw((float)(sx + SLOT_SIZE - 18), (float)(sy + SLOT_SIZE - 14), countBuf);
        }
        // else empty slot — no content rendered

        // Selected highlight (yellow 2-px outline)
        if (i == selected)
        {
            DrawFilledRect((float)sx, (float)sy, (float)SLOT_SIZE, 2.0f, MAKE_RGBA(255, 220, 60, 255));
            DrawFilledRect((float)sx, (float)(sy + SLOT_SIZE - 2), (float)SLOT_SIZE, 2.0f, MAKE_RGBA(255, 220, 60, 255));
            DrawFilledRect((float)sx, (float)sy, 2.0f, (float)SLOT_SIZE, MAKE_RGBA(255, 220, 60, 255));
            DrawFilledRect((float)(sx + SLOT_SIZE - 2), (float)sy, 2.0f, (float)SLOT_SIZE, MAKE_RGBA(255, 220, 60, 255));
        }
    }
}
void App::DrawBackpack()
{
    // Phase 3b: skip render only when fully closed AND no animation in progress
    if (m_backpackAnim <= 0.001f) return;

    const int SLOT_SIZE = 48;
    const int BP_X      = (1024 - 10 * SLOT_SIZE) / 2;  // 272
    const int TITLE_H   = 24;
    const int bpY       = GetBackpackPanelY();           // animated
    const int GRID_Y    = bpY + TITLE_H;
    const int PANEL_W   = 10 * SLOT_SIZE;
    const int PANEL_H   = TITLE_H + 3 * SLOT_SIZE;

    // Background panel
    DrawFilledRect((float)(BP_X - 4), (float)(bpY - 4),
                   (float)(PANEL_W + 8), (float)(PANEL_H + 8),
                   MAKE_RGBA(0, 0, 0, 200));

    // Title bar
    DrawFilledRect((float)BP_X, (float)bpY,
                   (float)PANEL_W, (float)TITLE_H,
                   MAKE_RGBA(60, 60, 80, 240));
    GetFont(FONT_SMALL)->Draw((float)(BP_X + 8), (float)(bpY + 6), "Backpack");
    GetFont(FONT_SMALL)->Draw((float)(BP_X + PANEL_W - 130), (float)(bpY + 6), "(swipe down to close)");

    // Slot grid
    for (int row = 0; row < Inventory::BACKPACK_ROWS; row++)
    {
        for (int col = 0; col < Inventory::BACKPACK_COLS; col++)
        {
            int idx = row * Inventory::BACKPACK_COLS + col;
            int sx = BP_X + col * SLOT_SIZE;
            int sy = GRID_Y + row * SLOT_SIZE;

            // Slot background
            DrawFilledRect((float)sx, (float)sy,
                           (float)SLOT_SIZE, (float)SLOT_SIZE,
                           MAKE_RGBA(40, 40, 40, 200));

            // Slot border (1-px)
            DrawFilledRect((float)sx, (float)sy, (float)SLOT_SIZE, 1.0f, MAKE_RGBA(80, 80, 80, 255));
            DrawFilledRect((float)sx, (float)(sy + SLOT_SIZE - 1), (float)SLOT_SIZE, 1.0f, MAKE_RGBA(80, 80, 80, 255));
            DrawFilledRect((float)sx, (float)sy, 1.0f, (float)SLOT_SIZE, MAKE_RGBA(80, 80, 80, 255));
            DrawFilledRect((float)(sx + SLOT_SIZE - 1), (float)sy, 1.0f, (float)SLOT_SIZE, MAKE_RGBA(80, 80, 80, 255));

            // Slot content
            const InventorySlot& s = m_inventory.GetBackpackSlot(idx);
            if (s.type != TILE_AIR && s.count > 0)
            {
                Surface* surf = GetTileSurface(s.type);
                if (surf)
                {
                    const TileType& tt = GetTileType(s.type);
                    const float CELL = (float)World::TILE_SIZE_PX;
                    int srcCol = tt.anchor_col;
                    int srcRow = tt.anchor_row;
                    rtRectf dst((float)(sx + 8), (float)(sy + 8),
                                (float)(sx + 8 + 32), (float)(sy + 8 + 32));
                    rtRectf src((float)srcCol * CELL, (float)srcRow * CELL,
                                (float)(srcCol + 1) * CELL, (float)(srcRow + 1) * CELL);
                    surf->BlitEx(dst, src);
                }
                else
                {
                    DrawFilledRect((float)(sx + 8), (float)(sy + 8), 32.0f, 32.0f, MAKE_RGBA(255, 0, 255, 255));
                }
                char countBuf[8];
                snprintf(countBuf, sizeof(countBuf), "%d", (int)s.count);
                GetFont(FONT_SMALL)->Draw((float)(sx + SLOT_SIZE - 18), (float)(sy + SLOT_SIZE - 14), countBuf);
            }
        }
    }
}

// Phase 3b extension: render floating world drops with sine bob animation.
void App::DrawDrops()
{
    const std::vector<WorldDrop>& drops = m_world.GetDrops();
    if (drops.empty()) return;

    const float DROP_SIZE = 22.0f;
    const float DROP_HALF = DROP_SIZE * 0.5f;

    for (size_t i = 0; i < drops.size(); i++)
    {
        const WorldDrop& d = drops[i];

        // Subtle sine bob: 3-px amplitude at ~1.2 Hz once settled
        float bobOffset = sinf(d.bobTimer * 7.5f) * 3.0f;

        CL_Vec2f worldPos(d.x - DROP_HALF, d.y - DROP_HALF + bobOffset);
        CL_Vec2f screenPos = m_camera.WorldToScreen(worldPos);

        // Off-screen cull
        if (screenPos.x < -DROP_SIZE || screenPos.x > 1024.0f) continue;
        if (screenPos.y < -DROP_SIZE || screenPos.y > 768.0f)  continue;

        Surface* surf = GetTileSurface(d.type);
        if (surf)
        {
            const TileType& tt = GetTileType(d.type);
            const float CELL = (float)World::TILE_SIZE_PX;
            int srcCol = tt.anchor_col;
            int srcRow = tt.anchor_row;
            rtRectf dst(screenPos.x, screenPos.y,
                        screenPos.x + DROP_SIZE, screenPos.y + DROP_SIZE);
            rtRectf src((float)srcCol * CELL, (float)srcRow * CELL,
                        (float)(srcCol + 1) * CELL, (float)(srcRow + 1) * CELL);
            surf->BlitEx(dst, src);
        }
    }
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
