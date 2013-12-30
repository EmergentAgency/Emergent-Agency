/************************************************************************
*                                                                       *
*   NuiImpl.cpp -- Implementation of CSkeletalViewerApp methods dealing *
*                  with NUI processing                                  *
*                                                                       *
* Copyright (c) Microsoft Corp. All rights reserved.                    *
*                                                                       *
* This code is licensed under the terms of the                          *
* Microsoft Kinect for Windows SDK (Beta) from Microsoft Research       *
* License Agreement: http://research.microsoft.com/KinectSDK-ToU        *
*                                                                       *
************************************************************************/

#include "stdafx.h"
#include "SkeletalViewer.h"
#include "resource.h"
#include <mmsystem.h>

// BLOOM
// Config vars
static const float fHeightAboveShoulderForUpInMeters = -0.1f;	// 10cm below shoulder = less tiring
static const float fHeightAboveOtherFootForUp = 0.25f; // Height one foot needs to be over the other for "up" detection
static const float numSecondsPerFrame = 0.0333f;  //TEMP_CL: hack until we get proper timestamps working
static const int   numPastFrames = 4;		      // go back this many frames in the past for velocity data, etc.
static const float fHandVelocityFactor = 0.3f;    // scale down velocity for overall speed ratio
static const float fSpeedSmoothing = 0.8f;        // The amount of smoothing we apply to overall speed ratio
static const float fMinSpeedRatio = 1.0f;         // The min speed ratio.  0.0 - 1.0.
static const float fHandVelocityScaleLow = 0.2f;  // low threshold for scaling luminence
static const float fHandVelocityScaleHigh = 1.0f; // high threshold for scaling luminence
static const float minUpSpeedForFlame = 5.0;	  // min upward/downward speed to trigger flame on/off (meters/sec)
static const float fBufferForwardInMeters = 0.2f; // distance to be in front of shoulder to be considered forward
static const float fMinSpeedRatioForColor = 0.3f; // You need to have a speed ratio of at least this for color to happen
static const int   iMaxSolenoidFrequency = 6;    // max rate we can turn the color solenoids on and off
static const float fMinFootSpeedForColorFlash = 1.f; // must be moving your foot this fast for color cycles

#include <math.h>
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include <XInput.h>

// Hack to pass SkeletonFrame between functions
NUI_SKELETON_FRAME SkeletonFrame;
// /Bloom


static const COLORREF g_JointColorTable[NUI_SKELETON_POSITION_COUNT] = 
{
    RGB(169, 176, 155), // NUI_SKELETON_POSITION_HIP_CENTER
    RGB(169, 176, 155), // NUI_SKELETON_POSITION_SPINE
    RGB(168, 230, 29), // NUI_SKELETON_POSITION_SHOULDER_CENTER
    RGB(200, 0,   0), // NUI_SKELETON_POSITION_HEAD
    RGB(79,  84,  33), // NUI_SKELETON_POSITION_SHOULDER_LEFT
    RGB(84,  33,  42), // NUI_SKELETON_POSITION_ELBOW_LEFT
    RGB(255, 126, 0), // NUI_SKELETON_POSITION_WRIST_LEFT
    RGB(215,  86, 0), // NUI_SKELETON_POSITION_HAND_LEFT
    RGB(33,  79,  84), // NUI_SKELETON_POSITION_SHOULDER_RIGHT
    RGB(33,  33,  84), // NUI_SKELETON_POSITION_ELBOW_RIGHT
    RGB(77,  109, 243), // NUI_SKELETON_POSITION_WRIST_RIGHT
    RGB(37,   69, 243), // NUI_SKELETON_POSITION_HAND_RIGHT
    RGB(77,  109, 243), // NUI_SKELETON_POSITION_HIP_LEFT
    RGB(69,  33,  84), // NUI_SKELETON_POSITION_KNEE_LEFT
    RGB(229, 170, 122), // NUI_SKELETON_POSITION_ANKLE_LEFT
    RGB(255, 126, 0), // NUI_SKELETON_POSITION_FOOT_LEFT
    RGB(181, 165, 213), // NUI_SKELETON_POSITION_HIP_RIGHT
    RGB(71, 222,  76), // NUI_SKELETON_POSITION_KNEE_RIGHT
    RGB(245, 228, 156), // NUI_SKELETON_POSITION_ANKLE_RIGHT
    RGB(77,  109, 243) // NUI_SKELETON_POSITION_FOOT_RIGHT
};

// Bloom - CTL
CSkeletalViewerApp::CSkeletalViewerApp()
	// initialize useful variables
	: m_iCurSkelFrame(0)
	, m_iCurSkelIndex(-1)
	, m_iOtherSkelIndex(-1)
	, m_bGotNewDepthFrame(false)
	, m_bGotNewSkelFrame(false)
	, m_bLeftHandUp(false)
	, m_bRightHandUp(false)
	, m_bLeftHandForward(false)
	, m_bRightHandForward(false)
	, m_vLeftHandSpeed(0.f)
	, m_vRightHandSpeed(0.f)
	, m_vLeftFootSpeed(0.f)
	, m_vRightFootSpeed(0.f)
	, m_vLeftHandSpeedY(0.f)
	, m_vRightHandSpeedY(0.f)
	, m_fSpeedRatio(fMinSpeedRatio)
	, m_bMainEffectOn(false)
	, m_bRedOn(false)
	, m_bYellowOn(false)
	, m_bGreenOn(false)
	, m_fAdjustableFlameIntensity(fMinSpeedRatio)
{
}
// /Bloom

void CSkeletalViewerApp::Nui_Zero()
{
    m_hNextDepthFrameEvent = NULL;
    m_hNextVideoFrameEvent = NULL;
    m_hNextSkeletonEvent = NULL;
    m_pDepthStreamHandle = NULL;
    m_pVideoStreamHandle = NULL;
    m_hThNuiProcess=NULL;
    m_hEvNuiProcessStop=NULL;
    ZeroMemory(m_Pen,sizeof(m_Pen));
    m_SkeletonDC = NULL;
    m_SkeletonBMP = NULL;
    m_SkeletonOldObj = NULL;
    m_PensTotal = 6*2;
    ZeroMemory(m_Points,sizeof(m_Points));
    m_LastSkeletonFoundTime = -1;
    m_bNoSkeleton = false;
    m_FramesTotal = 0;
    m_LastFPStime = -1;
    m_LastFramesTotal = 0;
}

// Bloom
char ComputeEffectState(bool bMainEffectOn, bool bRedOn, bool bGreenOn, bool bYellowOn, float fAdjustableFlameIntensity)
{
	char state = 0;

	state |= bMainEffectOn ? 0x1 : 0;
	state |= bRedOn        ? 0x2 : 0;
	state |= bGreenOn      ? 0x4 : 0;
	state |= bYellowOn     ? 0x8 : 0;

	int iFlameIntensity = (int)(fAdjustableFlameIntensity * 0xF); // map 0.0-1.0 to 0-15
	state |= iFlameIntensity << 4;

	return state;
}
// /Bloom

HRESULT CSkeletalViewerApp::Nui_Init()
{
	// Bloom - setup serial communication
	m_bSerialPortOpen = m_serial.Open(0, 9600) ? true : false; // first param is ignored
	if (m_bSerialPortOpen)	
		_tprintf(_T("Serial Port Open!\n"));
	else
		_tprintf(_T("Serial Port Fail!\n"));
	// /Bloom

    HRESULT             hr;
    RECT                rc;

    m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextVideoFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

    GetWindowRect(GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ), &rc );
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    HDC hdc = GetDC(GetDlgItem( m_hWnd, IDC_DEPTHVIEWER));
    m_SkeletonBMP = CreateCompatibleBitmap( hdc, width, height );
    m_SkeletonDC = hdc;
    ::ReleaseDC(GetDlgItem(m_hWnd,IDC_DEPTHVIEWER), hdc );
    m_SkeletonOldObj = SelectObject( m_SkeletonDC, m_SkeletonBMP );

    hr = m_DrawDepth.CreateDevice( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ) );
    if( FAILED( hr ) )
    {
        MessageBoxResource( m_hWnd,IDS_ERROR_D3DCREATE,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = m_DrawDepth.SetVideoType( 320, 240, 320 * 4 );
    if( FAILED( hr ) )
    {
        MessageBoxResource( m_hWnd,IDS_ERROR_D3DVIDEOTYPE,MB_OK | MB_ICONHAND);
        return hr;
    }
    
    hr = NuiInitialize( 
        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR);
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_NUIINIT,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, 0 );
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_SKELETONTRACKING,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = NuiImageStreamOpen(
        NUI_IMAGE_TYPE_COLOR,
        NUI_IMAGE_RESOLUTION_640x480,
        0,
        2,
        m_hNextVideoFrameEvent,
        &m_pVideoStreamHandle );
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_VIDEOSTREAM,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = NuiImageStreamOpen(
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
        NUI_IMAGE_RESOLUTION_320x240,
        0,
        2,
        m_hNextDepthFrameEvent,
        &m_pDepthStreamHandle );
    if( FAILED( hr ) )
    {
        MessageBoxResource(m_hWnd,IDS_ERROR_DEPTHSTREAM,MB_OK | MB_ICONHAND);
        return hr;
    }

    // Start the Nui processing thread
    m_hEvNuiProcessStop=CreateEvent(NULL,FALSE,FALSE,NULL);
    m_hThNuiProcess=CreateThread(NULL,0,Nui_ProcessThread,this,0,NULL);

    return hr;
}



void CSkeletalViewerApp::Nui_UnInit( )
{
    ::SelectObject( m_SkeletonDC, m_SkeletonOldObj );
    DeleteDC( m_SkeletonDC );
    DeleteObject( m_SkeletonBMP );

    if( m_Pen[0] != NULL )
    {
        DeleteObject(m_Pen[0]);
        DeleteObject(m_Pen[1]);
        DeleteObject(m_Pen[2]);
        DeleteObject(m_Pen[3]);
        DeleteObject(m_Pen[4]);
        DeleteObject(m_Pen[5]);
        DeleteObject(m_Pen[6]);
        DeleteObject(m_Pen[7]);
        DeleteObject(m_Pen[8]);
        DeleteObject(m_Pen[9]);
        DeleteObject(m_Pen[10]);
        DeleteObject(m_Pen[11]);
        ZeroMemory(m_Pen,sizeof(m_Pen));
    }

    // Stop the Nui processing thread
    if(m_hEvNuiProcessStop!=NULL)
    {
        // Signal the thread
        SetEvent(m_hEvNuiProcessStop);

        // Wait for thread to stop
        if(m_hThNuiProcess!=NULL)
        {
            WaitForSingleObject(m_hThNuiProcess,INFINITE);
            CloseHandle(m_hThNuiProcess);
        }
        CloseHandle(m_hEvNuiProcessStop);
    }

    NuiShutdown( );
    if( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextSkeletonEvent );
        m_hNextSkeletonEvent = NULL;
    }
    if( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextDepthFrameEvent );
        m_hNextDepthFrameEvent = NULL;
    }
    if( m_hNextVideoFrameEvent && ( m_hNextVideoFrameEvent != INVALID_HANDLE_VALUE ) )
    {
        CloseHandle( m_hNextVideoFrameEvent );
        m_hNextVideoFrameEvent = NULL;
    }
    m_DrawDepth.DestroyDevice( );
    m_DrawVideo.DestroyDevice( );
}



DWORD WINAPI CSkeletalViewerApp::Nui_ProcessThread(LPVOID pParam)
{
    CSkeletalViewerApp *pthis=(CSkeletalViewerApp *) pParam;
    HANDLE                hEvents[4];
    int                    nEventIdx,t,dt;

    // Configure events to be listened on
    hEvents[0]=pthis->m_hEvNuiProcessStop;
    hEvents[1]=pthis->m_hNextDepthFrameEvent;
    hEvents[2]=pthis->m_hNextVideoFrameEvent;
    hEvents[3]=pthis->m_hNextSkeletonEvent;

    // Main thread loop
    while(1)
    {
        // Wait for an event to be signalled
        nEventIdx=WaitForMultipleObjects(sizeof(hEvents)/sizeof(hEvents[0]),hEvents,FALSE,100);

        // If the stop event, stop looping and exit
        if(nEventIdx==0)
            break;            

        // Perform FPS processing
        t = timeGetTime( );
        if( pthis->m_LastFPStime == -1 )
        {
            pthis->m_LastFPStime = t;
            pthis->m_LastFramesTotal = pthis->m_FramesTotal;
        }
        dt = t - pthis->m_LastFPStime;
        if( dt > 1000 )
        {
            pthis->m_LastFPStime = t;
            //int FrameDelta = pthis->m_FramesTotal - pthis->m_LastFramesTotal;
            pthis->m_LastFramesTotal = pthis->m_FramesTotal;
            //SetDlgItemInt( pthis->m_hWnd, IDC_FPS, FrameDelta,FALSE );
        }

        // Perform skeletal panel blanking
        if( pthis->m_LastSkeletonFoundTime == -1 )
            pthis->m_LastSkeletonFoundTime = t;
        dt = t - pthis->m_LastSkeletonFoundTime;
        if( dt > 250 )
        {
            if( !pthis->m_bNoSkeleton )
            {
                pthis->m_bNoSkeleton = true;
 
				// Bloom
				// Turn off effects if we don't have any skel
				if(pthis->m_bSerialPortOpen)
				{
					char iEffectState = ComputeEffectState(false, false, false, false, 0);
					pthis->m_serial.SendData(&iEffectState, 1);
				}
				// /Bloom 
			}
        }

		// Draw the depth feed and the skeletons if there are any
		if(pthis->m_bGotNewSkelFrame || pthis->m_bNoSkeleton)
		{
			pthis->m_DrawDepth.DrawFrame( (BYTE*) pthis->m_rgbWk );

			if(!pthis->m_bNoSkeleton)
			{
				HDC hdc = GetDC(GetDlgItem( pthis->m_hWnd, IDC_DEPTHVIEWER));
				pthis->m_SkeletonDC = hdc;

				// draw each skeleton color according to the slot within they are found.
				for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
				{
					if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
					{
						pthis->Nui_DrawSkeleton(&SkeletonFrame.SkeletonData[i], GetDlgItem( pthis->m_hWnd, IDC_DEPTHVIEWER ), i );
					}
				}

				// clear the drawing window
				pthis->Nui_DoDoubleBuffer(GetDlgItem(pthis->m_hWnd,IDC_DEPTHVIEWER), pthis->m_SkeletonDC);

				::ReleaseDC(GetDlgItem(pthis->m_hWnd,IDC_DEPTHVIEWER), hdc );
			}

			pthis->m_bGotNewDepthFrame = false;
			pthis->m_bGotNewSkelFrame = false;
		}

        // Process signal events
        switch(nEventIdx)
        {
            case 1:
                pthis->Nui_GotDepthAlert();
                pthis->m_FramesTotal++;
                break;

            case 2:
                pthis->Nui_GotVideoAlert();
                break;

            case 3:
                pthis->Nui_GotSkeletonAlert( );
                break;
        }
    }

    return (0);
}

void CSkeletalViewerApp::Nui_GotVideoAlert( )
{
    const NUI_IMAGE_FRAME * pImageFrame = NULL;

    HRESULT hr = NuiImageStreamGetNextFrame(
        m_pVideoStreamHandle,
        0,
        &pImageFrame );
    if( FAILED( hr ) )
    {
        return;
    }

    NuiImageBuffer * pTexture = pImageFrame->pFrameTexture;
    KINECT_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( LockedRect.Pitch != 0 )
    {
        BYTE * pBuffer = (BYTE*) LockedRect.pBits;

        m_DrawVideo.DrawFrame( (BYTE*) pBuffer );

    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

    NuiImageStreamReleaseFrame( m_pVideoStreamHandle, pImageFrame );
}


void CSkeletalViewerApp::Nui_GotDepthAlert( )
{
    const NUI_IMAGE_FRAME * pImageFrame = NULL;

    HRESULT hr = NuiImageStreamGetNextFrame(
        m_pDepthStreamHandle,
        0,
        &pImageFrame );

    if( FAILED( hr ) )
    {
        return;
    }

    NuiImageBuffer * pTexture = pImageFrame->pFrameTexture;
    KINECT_LOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, 0 );
    if( LockedRect.Pitch != 0 )
    {
        BYTE * pBuffer = (BYTE*) LockedRect.pBits;

        // draw the bits to the bitmap
        RGBQUAD * rgbrun = m_rgbWk;
        USHORT * pBufferRun = (USHORT*) pBuffer;
        for( int y = 0 ; y < 240 ; y++ )
        {
            for( int x = 0 ; x < 320 ; x++ )
            {
                RGBQUAD quad = Nui_ShortToQuad_Depth( *pBufferRun );
                pBufferRun++;
                *rgbrun = quad;
                rgbrun++;
            }
        }
    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

	m_bGotNewDepthFrame = true;

    NuiImageStreamReleaseFrame( m_pDepthStreamHandle, pImageFrame );
}



void CSkeletalViewerApp::Nui_BlankSkeletonScreen(HWND hWnd)
{
    HDC hdc = GetDC( hWnd );
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;
    PatBlt( hdc, 0, 0, width, height, BLACKNESS );
    ReleaseDC( hWnd, hdc );
}

void CSkeletalViewerApp::Nui_DrawSkeletonSegment( NUI_SKELETON_DATA * pSkel, int numJoints, ... )
{
    va_list vl;
    va_start(vl,numJoints);
    POINT segmentPositions[NUI_SKELETON_POSITION_COUNT];

    for (int iJoint = 0; iJoint < numJoints; iJoint++)
    {
        NUI_SKELETON_POSITION_INDEX jointIndex = va_arg(vl,NUI_SKELETON_POSITION_INDEX);
        segmentPositions[iJoint].x = m_Points[jointIndex].x;
        segmentPositions[iJoint].y = m_Points[jointIndex].y;
    }

    Polyline(m_SkeletonDC, segmentPositions, numJoints);

    va_end(vl);
}

void CSkeletalViewerApp::Nui_DrawSkeleton(NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor )
{
	int iPenIndex = WhichSkeletonColor % m_PensTotal;
	if(WhichSkeletonColor != m_iCurSkelIndex)
	{
		iPenIndex += 6;
	}
    HGDIOBJ hOldObj = SelectObject(m_SkeletonDC,m_Pen[iPenIndex]);
    
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;

    if( m_Pen[0] == NULL )
    {
		// Skel controlling effect
        m_Pen[0] = CreatePen( PS_SOLID, width / 40, RGB(255, 0, 0) );
        m_Pen[1] = CreatePen( PS_SOLID, width / 40, RGB( 0, 255, 0 ) );
        m_Pen[2] = CreatePen( PS_SOLID, width / 40, RGB( 64, 255, 255 ) );
        m_Pen[3] = CreatePen( PS_SOLID, width / 40, RGB(255, 255, 64 ) );
        m_Pen[4] = CreatePen( PS_SOLID, width / 40, RGB( 255, 64, 255 ) );
        m_Pen[5] = CreatePen( PS_SOLID, width / 40, RGB( 128, 128, 255 ) );

		// Skel not controlling effect
        m_Pen[6]  = CreatePen( PS_SOLID, width / 120, RGB(255, 0, 0) );
        m_Pen[7]  = CreatePen( PS_SOLID, width / 120, RGB( 0, 255, 0 ) );
        m_Pen[8]  = CreatePen( PS_SOLID, width / 120, RGB( 64, 255, 255 ) );
        m_Pen[9]  = CreatePen( PS_SOLID, width / 120, RGB(255, 255, 64 ) );
        m_Pen[10] = CreatePen( PS_SOLID, width / 120, RGB( 255, 64, 255 ) );
        m_Pen[11] = CreatePen( PS_SOLID, width / 120, RGB( 128, 128, 255 ) );
    }

    int scaleX = width; //scaling up to image coordinates
    int scaleY = height;
    float fx=0,fy=0;
    int i;
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
    {
        NuiTransformSkeletonToDepthImageF( pSkel->SkeletonPositions[i], &fx, &fy );
        m_Points[i].x = (int) ( fx * scaleX + 0.5f );
        m_Points[i].y = (int) ( fy * scaleY + 0.5f );

		// Range check
		if(m_Points[i].x < 0) m_Points[i].x = 0;
		if(m_Points[i].y < 0) m_Points[i].y = 0;
		if(m_Points[i].x > width)  m_Points[i].x = width;
		if(m_Points[i].y > height) m_Points[i].y = height;
    }

    SelectObject(m_SkeletonDC,m_Pen[iPenIndex]);
    
    Nui_DrawSkeletonSegment(pSkel,4,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_HEAD);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT);
    Nui_DrawSkeletonSegment(pSkel,5,NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT);
    
    // Draw the joints in a different color
    for (i = 0; i < NUI_SKELETON_POSITION_COUNT ; i++)
    {
        HPEN hJointPen;
        
        hJointPen=CreatePen(PS_SOLID,9, g_JointColorTable[i]);
        hOldObj=SelectObject(m_SkeletonDC,hJointPen);

        MoveToEx( m_SkeletonDC, m_Points[i].x, m_Points[i].y, NULL );
        LineTo( m_SkeletonDC, m_Points[i].x, m_Points[i].y );

        SelectObject( m_SkeletonDC, hOldObj );
        DeleteObject(hJointPen);
    }

    return;

}

void CSkeletalViewerApp::Nui_DoDoubleBuffer(HWND hWnd,HDC hDC)
{
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;

    HDC hdc = GetDC( hWnd );

    BitBlt( hdc, 0, 0, width, height, hDC, 0, 0, SRCCOPY );

    ReleaseDC( hWnd, hdc );

}

void CSkeletalViewerApp::Nui_GotSkeletonAlert( )
{
    HRESULT hr = NuiSkeletonGetNextFrame( 0, &SkeletonFrame );
    if( FAILED( hr ) )
    {
        return;
    }

    // smooth out the skeleton data
    NuiTransformSmooth(&SkeletonFrame,NULL);

	// Increment skel history and save copy of newest data
	m_iCurSkelFrame++;
	if(m_iCurSkelFrame >= NUM_SKELETON_HISTORY_FRAMES)
		m_iCurSkelFrame = 0;
	// memcpy here requires BOTH NUI_SKELETON_FRAME / BloomSkeletonFrame AND NUI_SKELETON_DATA / BloomSkeletonData 
	// to be EXACTLY the same in size AND order of variables.  The asserts below are a safegaurd against size
	// mismatch but do not check for variable order.
	assert(sizeof(BloomSkeletonData) == sizeof(NUI_SKELETON_DATA));
	assert(sizeof(BloomSkeletonFrame) == sizeof(NUI_SKELETON_FRAME));
	memcpy(&m_aSkelHistory[m_iCurSkelFrame], &SkeletonFrame, sizeof(NUI_SKELETON_FRAME));

	// Update the tracking state of all the skeletons.  We want to do this
	// even if we don't have any tracked skeletons because in that case we need to reset the state
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
		UpdateTrackingData(i);
	}

	// Check to see if we found any skeletons
    bool bFoundSkeleton = false;
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
            bFoundSkeleton = true;
        }
    }

    // no skeletons!
    if( !bFoundSkeleton )
    {
        return;
    }

    // we found a skeleton, re-start the timer
    m_bNoSkeleton = false;
    m_LastSkeletonFoundTime = -1;

	// Signal that we got new skel data so we can draw it in the main loop
	m_bGotNewSkelFrame = true;

	// do detections for Bloom
	ProcessSkeletonForBloom(&SkeletonFrame);
}

int CSkeletalViewerApp::GetPastHistoryIndex(int iHistoryIndex)
{
	assert(iHistoryIndex <= 0);
	int iIndex = m_iCurSkelFrame + iHistoryIndex;
	if(iIndex < 0)
		iIndex += NUM_SKELETON_HISTORY_FRAMES;
	return iIndex;
}

float Clamp(float fInput, float fMin, float fMax)
{
	if(fInput < fMin)
	{
		return fMin;
	}
	else if(fInput > fMax)
	{
		return fMax;
	}

	return fInput;
}

void CSkeletalViewerApp::UpdateTrackingData(int iSkelIndex)
{
	// Get index into the history buffer for the current time
	int iCurIndex = GetPastHistoryIndex(0);

	// If this skel isn't tracked, reset its tracking vars and return
	if( m_aSkelHistory[iCurIndex].SkeletonData[iSkelIndex].eTrackingState != NUI_SKELETON_TRACKED)
	{
		m_afSkelCenteredness[iSkelIndex] = 0;
		m_afTotalJointQuality[iSkelIndex] = 0;
		m_afMovementAmount[iSkelIndex] = 0;
		return;
	}

	// Centeredness
	D3DXVECTOR4 vShoulderCenterPos = m_aSkelHistory[iCurIndex].SkeletonData[iSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
	static const D3DXVECTOR4 kvIdealCenter(0.0, 0.6, 2.5, 1.0);
	D3DXVECTOR4 vDiff = vShoulderCenterPos - kvIdealCenter;
	static const float kMaxDist = 4.f;
	float fCurCenteredness = Clamp (1.f - D3DXVec4Length(&vDiff) / kMaxDist, 0.f, 1.f);
	m_afSkelCenteredness[iSkelIndex] = fCurCenteredness; // TEMP_CL - smooth this over time!

	//// debug
	//static char buf[2048];
	//sprintf(buf,"skel %d  vShoulderCenterPos=(%f,%f,%f)  fCurCenteredness=%f\n",
	//	iSkelIndex, vShoulderCenterPos.x, vShoulderCenterPos.y, vShoulderCenterPos.z, fCurCenteredness);
	//OutputDebugStringA(buf);

	// TotalJointQuality
	m_afTotalJointQuality[iSkelIndex] = 1.0; // TEMP_CL

	// MovementAmount
	m_afMovementAmount[iSkelIndex] = 1.0; // TEMP_CL
}

void CSkeletalViewerApp::ProcessSkeletonForBloom(NUI_SKELETON_FRAME* pSkelFrame)
{
	// Check to see if our currently tracked skeleton is still tracked.
	// If not assign it to invalid
	if(m_iCurSkelIndex >= 0 && pSkelFrame->SkeletonData[m_iCurSkelIndex].eTrackingState != NUI_SKELETON_TRACKED)
	{
		m_iCurSkelIndex =  -1;
	}

	// If the current skel is tracked, see if anyone else is tracked and see if maybe they should take over
	if(m_iCurSkelIndex >= 0)
	{
		for(int i = 0 ; i < NUI_SKELETON_COUNT ; i++)
		{
			// don't compare m_iCurSkelIndex to itself
			if(i == m_iCurSkelIndex)
			{
				continue;
			}

			// If if some other skel is reasonably better
			static const float kBetterRatio = 1.05;
			if( m_afSkelCenteredness[i] + m_afTotalJointQuality[i] + m_afMovementAmount[i] >
				kBetterRatio * (m_afSkelCenteredness[m_iCurSkelIndex] + m_afTotalJointQuality[m_iCurSkelIndex] + m_afMovementAmount[m_iCurSkelIndex]) )
			{
				// Switch
				m_iCurSkelIndex = i;
			}
		}
	}
	// If cur skel isn't tracked, pick the best skel
	else
	{
		int iBestIndex = -1;
		for(int i = 0 ; i < NUI_SKELETON_COUNT ; i++)
		{
			// if this skel isn't track, don't mess with it
			if(pSkelFrame->SkeletonData[i].eTrackingState != NUI_SKELETON_TRACKED)
			{
				continue;
			}

			// If we find any tracked skeleton and we don't have a best index yet, pick that as the best
			if(iBestIndex < 0)
			{
				iBestIndex = i;
				continue;
			}

			// If if some other skel is better
			if( m_afSkelCenteredness[i] + m_afTotalJointQuality[i] + m_afMovementAmount[i] >
				m_afSkelCenteredness[iBestIndex] + m_afTotalJointQuality[iBestIndex] + m_afMovementAmount[iBestIndex] )
			{
				// Switch
				iBestIndex = i;
			}
		}

		// Set the current index to the best one found
		m_iCurSkelIndex = iBestIndex;
	}

	// Total random safety check that shouldn't be needed but will help debugging if something
	// goes wrong
	if(m_iCurSkelIndex < 0)
	{
		OutputDebugStringA("ERROR!  No valid skeleton in ProcessSkeletonForBloom.\n");
		return;
	}

	// Get the skel data for the current index
	NUI_SKELETON_DATA* pSkel = &(pSkelFrame->SkeletonData[m_iCurSkelIndex]);
    if( !pSkel )
    {
		OutputDebugStringA("ERROR!  pSkel is NULL.\n");
		return;
    }

	// Get joint positions this frame
	int iCurIndex = GetPastHistoryIndex(0);
	int iPastIndex = GetPastHistoryIndex(-1*numPastFrames); 
	D3DXVECTOR4 vShoulderCenterPos = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
	D3DXVECTOR4 vLeftHandPos       = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	D3DXVECTOR4 vRightHandPos      = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	D3DXVECTOR4 vLeftHandPosPast   = m_aSkelHistory[iPastIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	D3DXVECTOR4 vRightHandPosPast  = m_aSkelHistory[iPastIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	D3DXVECTOR4 vLeftFootPos       = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_FOOT_LEFT];
	D3DXVECTOR4 vRightFootPos      = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_FOOT_RIGHT];
	D3DXVECTOR4 vLeftFootPosPast   = m_aSkelHistory[iPastIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_FOOT_LEFT];
	D3DXVECTOR4 vRightFootPosPast  = m_aSkelHistory[iPastIndex].SkeletonData[m_iCurSkelIndex].SkeletonPositions[NUI_SKELETON_POSITION_FOOT_RIGHT];
	int iCurTimeStamp              = m_aSkelHistory[iCurIndex].liTimeStamp.LowPart;
	int iPastTimeStamp             = m_aSkelHistory[iPastIndex].liTimeStamp.LowPart;

	// Check to see if hands are above and/or in front of shoulder center
	m_bLeftHandUp  = vLeftHandPos.y  > vShoulderCenterPos.y + fHeightAboveShoulderForUpInMeters;
	m_bRightHandUp = vRightHandPos.y > vShoulderCenterPos.y + fHeightAboveShoulderForUpInMeters;
	// positive z  = away from camera
	m_bLeftHandForward = vLeftHandPos.z < vShoulderCenterPos.z - fBufferForwardInMeters;
	m_bRightHandForward = vRightHandPos.z < vShoulderCenterPos.z - fBufferForwardInMeters;

	// Check if feet are up by comparing on foot to the other - but only if the signal is good
	bool bLeftFootGood  = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_FOOT_LEFT] == NUI_SKELETON_POSITION_TRACKED;
	bool bRightFootGood = m_aSkelHistory[iCurIndex].SkeletonData[m_iCurSkelIndex].eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_FOOT_RIGHT] == NUI_SKELETON_POSITION_TRACKED;
	m_bLeftFootUp  = bLeftFootGood  ? (vLeftFootPos.y > vRightFootPos.y + fHeightAboveOtherFootForUp) : false;
	m_bRightFootUp = bRightFootGood ? (vRightFootPos.y > vLeftFootPos.y + fHeightAboveOtherFootForUp) : false;

	//// debug
	//static char buf[2048];
	//sprintf(buf,"bLeftFootGood:%d  bRightFootGood:%d\n", bLeftFootGood, bRightFootGood);
	//OutputDebugStringA(buf);

	// Get delta time in seconds (timestamps are in units of milliseconds)
	float fDeltaSeconds = (iCurTimeStamp - iPastTimeStamp) * 0.001f; 

	// Get hand and foot velocity (absolute)
	D3DXVECTOR4 vDiff = vLeftHandPos - vLeftHandPosPast;
	m_vLeftHandSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;
	vDiff = vRightHandPos - vRightHandPosPast;
	m_vRightHandSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;
	vDiff = vLeftFootPos - vLeftFootPosPast;
	m_vLeftFootSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;
	vDiff = vRightFootPos - vRightFootPosPast;
	m_vRightFootSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;

	// Calculte total speed ratio (0.0-1.0)
	float fNewSpeedRatio = (m_vLeftHandSpeed + m_vRightHandSpeed) * fHandVelocityFactor;
	m_fSpeedRatio = m_fSpeedRatio * fSpeedSmoothing +
		            fNewSpeedRatio * (1.f - fSpeedSmoothing);
	if(m_fSpeedRatio > 1.f)
	{
		m_fSpeedRatio = 1.f;
	}
	else if(m_fSpeedRatio < fMinSpeedRatio)
	{
		m_fSpeedRatio = fMinSpeedRatio;
	}

	// Hand speed trigger on an off - we aren't using this for now

	//// Get hand velocity (Y-component only)
	//float yDiff = vLeftHandPos.y - vLeftHandPosPast.y;
	//m_vLeftHandSpeedY = yDiff / fDeltaSeconds;
	//yDiff = vRightHandPos.y - vRightHandPosPast.y;	
	//m_vRightHandSpeedY = yDiff / fDeltaSeconds;

	//// Charlotte's interactivity
	//// display fastest Y-component of hand speed in numbers large enough to see variation
	//// SetDlgItemInt cannot handle negative numbers: 4294967
	//// add scaling by arm length to account for big and small people
	//float fastestHandSpeedUp = max(m_vRightHandSpeedY, m_vLeftHandSpeedY) * 3.28f;
	//float fastestHandSpeedDown = min(m_vRightHandSpeedY, m_vLeftHandSpeedY) * -3.28f;
	//SetDlgItemInt( m_hWnd, IDC_FPS, (int)fastestHandSpeedUp,FALSE );

	//// if either hand is up and moving up fast enough, trigger flame on
	//if( (m_vRightHandSpeedY >= minUpSpeedForFlame && m_bRightHandUp) ||
	//	(m_vLeftHandSpeedY  >= minUpSpeedForFlame && m_bLeftHandUp) )
	//{
	//	m_bMainEffectOn = true;
	//}
	//// if either hand is down and moving down fast enough, trigger flame off
	//else if( (m_vRightHandSpeedY <= -1.0f*minUpSpeedForFlame && !m_bRightHandUp) ||
	//	     (m_vLeftHandSpeedY  <= -1.0f*minUpSpeedForFlame && !m_bLeftHandUp) )
	//{
	//	m_bMainEffectOn = false;
	//}
	
	// setup timer so if some code below want to cycle color as fast as the solenoids go (about 10hz)
	// there is an easy hook for that
	bool bCycleColorsNow(false);
	static int iColorCycleTimer = 0;
	iColorCycleTimer++;
	if(iColorCycleTimer % (30 / iMaxSolenoidFrequency)) // assume we are running at 30 fps
	{
		iColorCycleTimer = 0;
		bCycleColorsNow = true;
	}

	// Turn main effect on if either hand is up
	m_bMainEffectOn = m_bLeftHandUp || m_bRightHandUp || m_bLeftFootUp || m_bRightFootUp;

	// Only deal with colors if the main effect is on
	m_bYellowOn = m_bRedOn = m_bGreenOn = false;
	if(m_bMainEffectOn)
	{
		//// Turn colors on only if moving fast enough.  Colors correspond to which hands are up.
		//if(m_fSpeedRatio > fMinSpeedRatioForColor)
		//{
		//	if(m_bLeftHandUp && m_bRightHandUp)
		//		m_bYellowOn = true;
		//	else if(m_bLeftHandUp)
		//		m_bRedOn = true;
		//	else if(m_bRightHandUp)
		//		m_bGreenOn = true;
		//}

		// Turn colors on if hands are up and forward
		if(m_bLeftHandUp && m_bRightHandUp && m_bLeftHandForward && m_bRightHandForward)
			m_bYellowOn = true;
		else if(m_bLeftHandUp && m_bLeftHandForward)
			m_bRedOn = true;
		else if(m_bRightHandUp && m_bRightHandForward)
			m_bGreenOn = true;

		// Turn on colors if feet are up (only one foot can be up at a time)
		if(m_bLeftFootUp && m_vLeftFootSpeed > fMinFootSpeedForColorFlash)
		{
			if(bCycleColorsNow)
			{
				float fRand = (float)rand() / (float)RAND_MAX;
				if(fRand < 0.333f)
				{
					m_bRedOn = false;
					m_bYellowOn = false;
				}
				else if(fRand < 0.666f)
				{
					m_bRedOn = true;
					m_bYellowOn = false;
				}
				else
				{
					m_bRedOn = false;
					m_bYellowOn = true;
				}
			}
		}
		else if(m_bRightFootUp && m_vRightFootSpeed > fMinFootSpeedForColorFlash)
		{
			if(bCycleColorsNow)
			{
				float fRand = (float)rand() / (float)RAND_MAX;
				if(fRand < 0.333f)
				{
					m_bGreenOn = false;
					m_bYellowOn = false;
				}
				else if(fRand < 0.666f)
				{
					m_bGreenOn = true;
					m_bYellowOn = false;
				}
				else
				{
					m_bGreenOn = false;
					m_bYellowOn = true;
				}
			}
		}
	}


	// Set the adjustable effect to the speed ratio if the main effect isn't on
	// otherwise, just run it full on
	if(!m_bMainEffectOn)
		m_fAdjustableFlameIntensity = m_fSpeedRatio;
	else
		m_fAdjustableFlameIntensity = 1.f;

	// Serial out
	if(m_bSerialPortOpen)
	{
		char iEffectState = ComputeEffectState(m_bMainEffectOn, m_bRedOn, m_bGreenOn, m_bYellowOn, m_fAdjustableFlameIntensity);
		m_serial.SendData(&iEffectState, 1);
	}
}

RGBQUAD CSkeletalViewerApp::Nui_ShortToQuad_Depth( USHORT s )
{
    USHORT RealDepth = (s & 0xfff8) >> 3;
    USHORT Player = s & 7;

    // transform 13-bit depth information into an 8-bit intensity appropriate
    // for display (we disregard information in most significant bit)
    BYTE lumens = 255 - (BYTE)(256*RealDepth/0x0fff);  // not technically in units of lumens but whatever

    RGBQUAD q;
    q.rgbRed = q.rgbBlue = q.rgbGreen = 0;

	// Change color of the depth feed to show bloom's state

	// show adjustable flame intensity with general brightness of the depth feed
	float fAdjustableEffectScale = m_fAdjustableFlameIntensity;
	if(fAdjustableEffectScale < fHandVelocityScaleLow)
	{
		fAdjustableEffectScale = fHandVelocityScaleLow;
	}
	else if(fAdjustableEffectScale > fHandVelocityScaleHigh)
	{
		fAdjustableEffectScale = fHandVelocityScaleHigh;
	}
	lumens = (BYTE)(lumens * fAdjustableEffectScale);

	// only render depth field for person controlling the effect, otherwise use flat colors
	if(Player == (m_iCurSkelIndex + 1) && m_bMainEffectOn)
	{
		if(m_bYellowOn)
		{
			q.rgbRed =   ((s & 0x00f0) >> 4)  * 16;
			//q.rgbRed =   ((s >> 3) & 0x0ff);
			q.rgbBlue =  0;
			q.rgbGreen = q.rgbRed;
			return q;
		}
		else if(m_bRedOn)
		{
			q.rgbRed =   ((s & 0x00f0) >> 4)  * 16;
			q.rgbBlue =  0;
			q.rgbGreen = 0;
			return q;
		}
		else if(m_bGreenOn)
		{
			q.rgbRed =   0;
			q.rgbBlue =  0;
			q.rgbGreen = ((s & 0x00f0) >> 4)  * 16;
			return q;
		}
		else
		{
			// white if neither hand is forward
			q.rgbRed =   ((s & 0x00f0) >> 4)  * 16;
			q.rgbGreen = q.rgbRed;
			q.rgbBlue = q.rgbRed;
			return q;
		}
	}

	// non-flame color to identify players, intensity still driven by velocity
	switch( Player )
    {
    case 0:
		if(m_bSerialPortOpen)
		{
			q.rgbRed = lumens / 2;
			q.rgbBlue = lumens / 2;
			q.rgbGreen = lumens / 2;
		}
		else
		{
			q.rgbRed = lumens / 2;
			q.rgbBlue = 0;
			q.rgbGreen = 0;
		}
        break;
    case 1:
        q.rgbRed = lumens;
        break;
    case 2:
        q.rgbGreen = lumens;
        break;
    case 3:
        q.rgbRed = lumens / 4;
        q.rgbGreen = lumens;
        q.rgbBlue = lumens;
        break;
    case 4:
        q.rgbRed = lumens;
        q.rgbGreen = lumens;
        q.rgbBlue = lumens / 4;
        break;
    case 5:
        q.rgbRed = lumens;
        q.rgbGreen = lumens / 4;
        q.rgbBlue = lumens;
        break;
    case 6:
        q.rgbRed = lumens / 2;
        q.rgbGreen = lumens / 2;
        q.rgbBlue = lumens;
        break;
    case 7:
        q.rgbRed = 255 - ( lumens / 2 );
        q.rgbGreen = 255 - ( lumens / 2 );
        q.rgbBlue = 255 - ( lumens / 2 );
    }

    return q;
}
