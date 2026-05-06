/*
 *  App.h
 *  Created by Seth Robinson on 3/6/09.
 *  For license info, check the license.txt file that should have come with this.
 *
 */

#pragma once

#include "BaseApp.h"
#include "Player.h"
#include "Camera.h"
#include "World.h"
#include "Inventory.h"
#include "Interaction.h"

class App: public BaseApp
{
public:

	App();
	virtual ~App();
	virtual bool Init();
	virtual void Kill();
	virtual void Draw();
	virtual void OnScreenSizeChange();
	virtual void OnEnterBackground();
	virtual void OnEnterForeground();
	virtual bool OnPreInitVideo();
	virtual void Update();
	void OnExitApp(VariantList *pVarList);


	//we'll wire these to connect to some signals we care about
	void OnAccel(VariantList *pVList);
	void OnArcadeInput(VariantList *pVList);
	Inventory& GetInventory() { return m_inventory; }
	World& GetWorld() { return m_world; }
	void SetMousePos(const CL_Vec2f& p) { m_mousePos = p; }
	void SetMouseDown(bool d)            { m_mouseDown = d; }

	// Phase 3b: UI hit-testing (public so AppInput free-function can call them)
	int  HitTestHotbarSlot(int mx, int my);
	int  HitTestBackpackSlot(int mx, int my);
	bool HitTestBackpackHandle(int mx, int my);
	bool HitTestBackpackPanel(int mx, int my);  // any part of panel (title bar, chrome, slots)

	// Phase 3b: animated UI Y positions (lerp closed↔open via m_backpackAnim)
	int  GetHotbarY() const;          // top of hotbar slot row
	int  GetBackpackPanelY() const;   // top of backpack panel (title bar)

	// Phase 3b: drag gesture state (Grab-app style swipe)
	void SetHandleDragStart(float y) { m_handleDragging = true; m_handleDragStartY = y; }
	void EndHandleDrag(float endY);   // checks delta, opens/closes backpack
	bool IsHandleDragging() const     { return m_handleDragging; }

private:

	bool m_bDidPostInit;

	// Phase 1 additions
	Player m_player;
	Camera m_camera;
	bool m_inputLeft;
	bool m_inputRight;
	bool m_inputJump;

	// Phase 2: tile world
	World m_world;
	bool m_worldGenerated;
	Inventory m_inventory;   // Phase 3b: replaces Selection
	Interaction m_interaction;

	// Phase 2: mouse state
	CL_Vec2f m_mousePos;
	bool     m_mouseDown;

	// Phase 3b: handle drag tracking
	bool     m_handleDragging;
	float    m_handleDragStartY;

	// Phase 3b: backpack slide-up animation (0=closed/offscreen, 1=fully open)
	float    m_backpackAnim;

	// Phase 3b: draw helpers (stubs until Tasks 6-7)
	void DrawHotbar();
	void DrawBackpack();
};


App * GetApp();
const char * GetAppName();
const char * GetBundlePrefix();
const char * GetBundleName();
