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

private:

	bool m_bDidPostInit;
	SurfaceAnim m_surf; //for testing

	// Phase 1 additions
	Player m_player;
	Camera m_camera;
	bool m_inputLeft;
	bool m_inputRight;
	bool m_inputJump;

	// Phase 2: tile world
	World m_world;
	bool m_worldGenerated;
};


App * GetApp();
const char * GetAppName();
const char * GetBundlePrefix();
const char * GetBundleName();
