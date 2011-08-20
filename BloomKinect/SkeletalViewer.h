/************************************************************************
*                                                                       *
*   SkeletalViewer.h -- Declares of CSkeletalViewerApp class            *
*                                                                       *
* Copyright (c) Microsoft Corp. All rights reserved.                    *
*                                                                       *
* This code is licensed under the terms of the                          *
* Microsoft Kinect for Windows SDK (Beta) from Microsoft Research       *
* License Agreement: http://research.microsoft.com/KinectSDK-ToU        *
*                                                                       *
************************************************************************/

#pragma once

#include "resource.h"
#include "MSR_NuiApi.h"
#include "DrawDevice.h"
#include "DXUT.h"
#include "Serial.h" // Bloom

#define SZ_APPDLG_WINDOW_CLASS        _T("SkeletalViewerAppDlgWndClass")

// Bloom - CTL
#define NUM_SKELETON_HISTORY_FRAMES 60

// custom structs use D3DX vectors to save the work of defining operations (+,-, *, x, etc.)
typedef struct _SkeletonData
{
  NUI_SKELETON_TRACKING_STATE eTrackingState;
  DWORD dwTrackingID;
  DWORD dwEnrollmentIndex;
  DWORD dwUserIndex;
  D3DXVECTOR4 Position;
  D3DXVECTOR4 SkeletonPositions[NUI_SKELETON_POSITION_COUNT];
  NUI_SKELETON_POSITION_TRACKING_STATE eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_COUNT];
  DWORD dwQualityFlags;
} SkeletonData;

typedef struct _SkeletonFrame
{
  LARGE_INTEGER         liTimeStamp;
  DWORD                 dwFrameNumber;
  DWORD                 dwFlags;
  D3DXVECTOR4           vFloorClipPlane;
  D3DXVECTOR4           vNormalToGravity;
  SkeletonData          SkeletonData[NUI_SKELETON_COUNT];
} SkeletonFrame;
// /Bloom

class CSkeletalViewerApp
{
public:

	// Bloom - CTL
	// Constructor
	CSkeletalViewerApp();
	// /Bloom

    HRESULT Nui_Init();
    void                    Nui_UnInit( );
    void                    Nui_GotDepthAlert( );
    void                    Nui_GotVideoAlert( );
    void                    Nui_GotSkeletonAlert( );
    void                    Nui_Zero();
    void                    Nui_BlankSkeletonScreen( HWND hWnd );
    void                    Nui_DoDoubleBuffer(HWND hWnd,HDC hDC);
    void                    Nui_DrawSkeleton( bool bBlank, NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor );
    void                    Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... );

	// Bloom - CTL
	void					ProcessSkeletonForBloom(NUI_SKELETON_FRAME* pSkel);

	// Nui_ShortToQuad_Depth: overloaded to visually render interactivity
    RGBQUAD                 Nui_ShortToQuad_Depth( USHORT s );
	// /Bloom

    static LONG CALLBACK    WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd;

private:
    static DWORD WINAPI     Nui_ProcessThread(LPVOID pParam);


    // NUI and draw stuff
    DrawDevice    m_DrawDepth;
    DrawDevice    m_DrawVideo;

    // thread handling
    HANDLE        m_hThNuiProcess;
    HANDLE        m_hEvNuiProcessStop;

    HANDLE        m_hNextDepthFrameEvent;
    HANDLE        m_hNextVideoFrameEvent;
    HANDLE        m_hNextSkeletonEvent;
    HANDLE        m_pDepthStreamHandle;
    HANDLE        m_pVideoStreamHandle;
    HFONT         m_hFontFPS;
    HPEN          m_Pen[6];
    HDC           m_SkeletonDC;
    HBITMAP       m_SkeletonBMP;
    HGDIOBJ       m_SkeletonOldObj;
    int           m_PensTotal;
    POINT         m_Points[NUI_SKELETON_POSITION_COUNT];
    RGBQUAD       m_rgbWk[640*480];
    int           m_LastSkeletonFoundTime;
    bool          m_bScreenBlanked;
    int           m_FramesTotal;
    int           m_LastFPStime;
    int           m_LastFramesTotal;

	// Bloom - CTL
	//SkeletonFrame m_aSkelHistory[NUM_SKELETON_HISTORY_FRAMES];
	SkeletonData  m_aSkelHistory[NUM_SKELETON_HISTORY_FRAMES];
	int 	      m_iCurSkelFrame;
	int           GetPastHistoryIndex(int iHistoryIndex);

	LARGE_INTEGER m_liLastTimeStamp;

	D3DXVECTOR4   m_vPrevLeftHandPos;
	D3DXVECTOR4   m_vPrevRightHandPos;

	bool          m_bLeftHandUp;
	bool          m_bRightHandUp;
	bool		  m_bLeftHandForward;
	bool		  m_bRightHandForward;
	bool		  m_bMainEffectOn;
	bool		  m_bRedOn;
	bool		  m_bYellowOn;
	bool		  m_bGreenOn;

	float         m_vLeftHandSpeed;
	float         m_vRightHandSpeed;
	float		  m_vLeftHandSpeedY;
	float		  m_vRightHandSpeedY;
	
	float         m_fAdjustableFlameIntensity;

	// serial code
	CSerial       m_serial;
	bool          m_bSerialPortOpen;
	// /Bloom

};

int MessageBoxResource(HWND hwnd,UINT nID,UINT nType);



