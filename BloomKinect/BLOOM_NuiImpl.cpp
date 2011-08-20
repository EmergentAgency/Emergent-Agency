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

//// BLOOM
// Config vars
static const float fHeightAboveShoulderForUpInMeters = -0.1f;	// 20cm below shoulder = less tiring
static const float numSecondsPerFrame = 0.0333f;	//TEMP_CL: hack until we get proper timestamps working
static const int numPastFrames = 4;		// go back this many frames in the past for velocity data, etc.
static const float fHandVelocityFactor = 0.4f;  // scale down velocity for use in depth feed display
static const float fHandVelocityScaleLow = 0.2f;  // low threshold for scaling luminence
static const float fHandVelocityScaleHigh = 1.0f; // high threshold for scaling luminence
static const float minUpSpeedForFlame = 5.0;	  // min upward/downward speed to trigger flame on/off (meters/sec)
static const float fBufferForwardInMeters = 0.2f; // distance to be in front of shoulder to be considered forward

#include <math.h>
#include "DXUT.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include <XInput.h>
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
	: m_bLeftHandUp(false)
	, m_bRightHandUp(false)
	, m_bLeftHandForward(false)
	, m_bRightHandForward(false)
	, m_bMainEffectOn(false)
	, m_bRedOn(false)
	, m_bYellowOn(false)
	, m_bGreenOn(false)
	, m_iCurSkelFrame(0)
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
    m_PensTotal = 6;
    ZeroMemory(m_Points,sizeof(m_Points));
    m_LastSkeletonFoundTime = -1;
    m_bScreenBlanked = false;
    m_FramesTotal = 0;
    m_LastFPStime = -1;
    m_LastFramesTotal = 0;
}



HRESULT CSkeletalViewerApp::Nui_Init()
{
	// Bloom - setup serial communication
	m_bSerialPortOpen = m_serial.Open(0, 9600); // first param is ignored
	if (m_bSerialPortOpen)	
		_tprintf(_T("Serial Port Open!\n"));
	else
		_tprintf(_T("Serial Port Fail!\n"));
	// /Bloom
    HRESULT                hr;
    RECT                rc;

    m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextVideoFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

    GetWindowRect(GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rc );
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    HDC hdc = GetDC(GetDlgItem( m_hWnd, IDC_SKELETALVIEW));
    m_SkeletonBMP = CreateCompatibleBitmap( hdc, width, height );
    m_SkeletonDC = CreateCompatibleDC( hdc );
    ::ReleaseDC(GetDlgItem(m_hWnd,IDC_SKELETALVIEW), hdc );
    m_SkeletonOldObj = SelectObject( m_SkeletonDC, m_SkeletonBMP );

    // TEMP_CL hr = m_DrawDepth.CreateDevice( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ) );
    hr = m_DrawDepth.CreateDevice( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ) );
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

    // TEMP_CL hr = m_DrawVideo.CreateDevice( GetDlgItem( m_hWnd, IDC_VIDEOVIEW ) );
    hr = m_DrawVideo.CreateDevice( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ) );
    if( FAILED( hr ) )
    {
        MessageBoxResource( m_hWnd,IDS_ERROR_D3DCREATE,MB_OK | MB_ICONHAND);
        return hr;
    }

    hr = m_DrawVideo.SetVideoType( 640, 480, 640 * 4 );
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
            int FrameDelta = pthis->m_FramesTotal - pthis->m_LastFramesTotal;
            pthis->m_LastFramesTotal = pthis->m_FramesTotal;
            //SetDlgItemInt( pthis->m_hWnd, IDC_FPS, FrameDelta,FALSE );
        }

        // Perform skeletal panel blanking
        if( pthis->m_LastSkeletonFoundTime == -1 )
            pthis->m_LastSkeletonFoundTime = t;
        dt = t - pthis->m_LastSkeletonFoundTime;
        if( dt > 250 )
        {
            if( !pthis->m_bScreenBlanked )
            {
                pthis->Nui_BlankSkeletonScreen( GetDlgItem( pthis->m_hWnd, IDC_SKELETALVIEW ) );
                pthis->m_bScreenBlanked = true;
            }
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

        m_DrawDepth.DrawFrame( (BYTE*) m_rgbWk );
    }
    else
    {
        OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
    }

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

void CSkeletalViewerApp::Nui_DrawSkeleton( bool bBlank, NUI_SKELETON_DATA * pSkel, HWND hWnd, int WhichSkeletonColor )
{
    HGDIOBJ hOldObj = SelectObject(m_SkeletonDC,m_Pen[WhichSkeletonColor % m_PensTotal]);
    
    RECT rct;
    GetClientRect(hWnd, &rct);
    int width = rct.right;
    int height = rct.bottom;

    if( m_Pen[0] == NULL )
    {
        m_Pen[0] = CreatePen( PS_SOLID, width / 80, RGB(255, 0, 0) );
        m_Pen[1] = CreatePen( PS_SOLID, width / 80, RGB( 0, 255, 0 ) );
        m_Pen[2] = CreatePen( PS_SOLID, width / 80, RGB( 64, 255, 255 ) );
        m_Pen[3] = CreatePen( PS_SOLID, width / 80, RGB(255, 255, 64 ) );
        m_Pen[4] = CreatePen( PS_SOLID, width / 80, RGB( 255, 64, 255 ) );
        m_Pen[5] = CreatePen( PS_SOLID, width / 80, RGB( 128, 128, 255 ) );
    }

    if( bBlank )
    {
        PatBlt( m_SkeletonDC, 0, 0, width, height, BLACKNESS );
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
    }

    SelectObject(m_SkeletonDC,m_Pen[WhichSkeletonColor%m_PensTotal]);
    
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
    NUI_SKELETON_FRAME SkeletonFrame;

    HRESULT hr = NuiSkeletonGetNextFrame( 0, &SkeletonFrame );

    bool bFoundSkeleton = false;
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
            bFoundSkeleton = true;
        }
    }

    // no skeletons!
    //
    if( !bFoundSkeleton )
    {
        return;
    }

    // smooth out the skeleton data
    NuiTransformSmooth(&SkeletonFrame,NULL);

    // we found a skeleton, re-start the timer
    m_bScreenBlanked = false;
    m_LastSkeletonFoundTime = -1;

    // draw each skeleton color according to the slot within they are found.
    //
    bool bBlank = true;
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        if( SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
            Nui_DrawSkeleton( bBlank, &SkeletonFrame.SkeletonData[i], GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), i );
            bBlank = false;
        }
    }

	// Bloom - CTL
	// do detections for Bloom
	ProcessSkeletonForBloom(&SkeletonFrame);
	// /Bloom

    Nui_DoDoubleBuffer(GetDlgItem(m_hWnd,IDC_SKELETALVIEW), m_SkeletonDC);
}
// ^ calls ProcessSkeletonForBloom


// TEMP_CL -- is this still necessary?
inline D3DXVECTOR4 GetRealVect(Vector4 vInVect)
{
	return D3DXVECTOR4(vInVect.x, vInVect.y, vInVect.z, vInVect.w);
}

ID3DXFont*                  g_pFont = NULL;             // Font for drawing text
ID3DXSprite*                g_pSprite = NULL;           // Sprite for batching draw text calls

int CSkeletalViewerApp::GetPastHistoryIndex(int iHistoryIndex)
{
	assert(iHistoryIndex <= 0);
	int iIndex = m_iCurSkelFrame + iHistoryIndex;
	if(iIndex < 0)
		iIndex += NUM_SKELETON_HISTORY_FRAMES;
	return iIndex;
}

char ComputeEffectState(bool bMainEffectOn, bool bRedOn, bool bGreenOn, bool bYellowOn, float iAdjustableFlameIntensity)
{
	char state = 0;

	state |= bMainEffectOn ? 0x1 : 0;
	state |= bRedOn        ? 0x2 : 0;
	state |= bGreenOn      ? 0x4 : 0;
	state |= bYellowOn     ? 0x8 : 0;

	int iFlameIntensity = iAdjustableFlameIntensity * 0xF; // map 0.0-1.0 to 0-15
	state |= iFlameIntensity << 4;

	return state;
}

// fixed all the 'Skelton's
void CSkeletalViewerApp::ProcessSkeletonForBloom(NUI_SKELETON_FRAME* pSkelFrame)
{
	// Find the first valid skel
	NUI_SKELETON_DATA* pSkel = NULL;
    for( int i = 0 ; i < NUI_SKELETON_COUNT ; i++ )
    {
        if( pSkelFrame->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
        {
			pSkel = &(pSkelFrame->SkeletonData[i]);
			break;
        }
    }

    // no skeletons!
    if( !pSkel )
    {
        return;
    }

	// Increment skel history and save copy of newest data
	m_iCurSkelFrame++;
	if(m_iCurSkelFrame >= NUM_SKELETON_HISTORY_FRAMES)
		m_iCurSkelFrame = 0;
	// REVISIT
	// memcpy here requires NUI_SKELETON_DATA and SkeletonData to be EXACTLY the same in
	// terms of size AND order of variables.  The assert below is a safegaurd against size
	// mismatch but does not check for variable order.
	assert(sizeof(SkeletonData) == sizeof(NUI_SKELETON_DATA));
	memcpy(&m_aSkelHistory[m_iCurSkelFrame], pSkel, sizeof(NUI_SKELETON_DATA));

	// Get joint positions this frame
	int iCurIndex = GetPastHistoryIndex(0);
	int iPastIndex = GetPastHistoryIndex(-1*numPastFrames); 
	D3DXVECTOR4 vShoulderCenterPos = m_aSkelHistory[iCurIndex].SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
	D3DXVECTOR4 vLeftHandPos       = m_aSkelHistory[iCurIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	D3DXVECTOR4 vRightHandPos      = m_aSkelHistory[iCurIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	D3DXVECTOR4 vLeftHandPosPast       = m_aSkelHistory[iPastIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
	D3DXVECTOR4 vRightHandPosPast      = m_aSkelHistory[iPastIndex].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
	D3DXVECTOR4 vLeftElbowPos       = m_aSkelHistory[iCurIndex].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
	D3DXVECTOR4 vRightElbowPos      = m_aSkelHistory[iCurIndex].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];

	// Check to see if hands are above and/or in front of shoulder center
	m_bLeftHandUp  = vLeftHandPos.y  > vShoulderCenterPos.y + fHeightAboveShoulderForUpInMeters;
	m_bRightHandUp = vRightHandPos.y > vShoulderCenterPos.y + fHeightAboveShoulderForUpInMeters;
	// positive z  = away from camera
	m_bLeftHandForward = vLeftHandPos.z < vShoulderCenterPos.z - fBufferForwardInMeters;
	m_bRightHandForward = vRightHandPos.z < vShoulderCenterPos.z - fBufferForwardInMeters;

	// Get delta time in seconds
	float fDeltaSeconds = numSecondsPerFrame * numPastFrames; // TEMP_CL - hack since we aren't storing all the time stamps back in time;
	m_liLastTimeStamp = pSkelFrame->liTimeStamp;

	// Get hand velocity (absolute)
	D3DXVECTOR4 vDiff = vLeftHandPos - vLeftHandPosPast;
	m_vLeftHandSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;
	vDiff = vRightHandPos - vRightHandPosPast;
	m_vRightHandSpeed = D3DXVec4Length(&vDiff) / fDeltaSeconds;

	// Get hand velocity (Y-component only)
	float yDiff = vLeftHandPos.y - vLeftHandPosPast.y;
	m_vLeftHandSpeedY = yDiff / fDeltaSeconds;
	yDiff = vRightHandPos.y - vRightHandPosPast.y;	
	m_vRightHandSpeedY = yDiff / fDeltaSeconds;

	// log?
	// display fastest Y-component of hand speed in numbers large enough to see variation
	// SetDlgItemInt cannot handle negative numbers: 4294967
	// add scaling by arm length to account for big and small people
	float fastestHandSpeedUp = max(m_vRightHandSpeedY, m_vLeftHandSpeedY) * 3.28f;
	float fastestHandSpeedDown = min(m_vRightHandSpeedY, m_vLeftHandSpeedY) * -3.28f;
	SetDlgItemInt( m_hWnd, IDC_FPS, (int)fastestHandSpeedUp,FALSE );

	// if either hand is up and moving up fast enough, trigger flame on
	if( (m_vRightHandSpeedY >= minUpSpeedForFlame && m_bRightHandUp) ||
		(m_vLeftHandSpeedY  >= minUpSpeedForFlame && m_bLeftHandUp) )
	{
		m_bMainEffectOn = true;
	}
	// if either hand is down and moving down fast enough, trigger flame off
	else if( (m_vRightHandSpeedY <= -1.0f*minUpSpeedForFlame && !m_bRightHandUp) ||
		     (m_vLeftHandSpeedY  <= -1.0f*minUpSpeedForFlame && !m_bLeftHandUp) )
	{
		m_bMainEffectOn = false;
	}
	
	bool bMainEffectOn = false;
	bool bRedOn = false;
	bool bGreenOn = false;
	bool bYellowOn = false;
	float iNewAdjustableFlameIntensity = 0;

	if(m_bLeftHandUp && m_bRightHandUp)
		bYellowOn = true;
	else if(m_bLeftHandUp)
		bRedOn = true;
	else if(m_bRightHandUp)
		bGreenOn = true;

	if(bYellowOn || bRedOn || bGreenOn)
		bMainEffectOn = true;

	iNewAdjustableFlameIntensity = (m_vLeftHandSpeed + m_vRightHandSpeed) * 0.2f;
	float fFlameSmoothing = 0.8;
	m_fAdjustableFlameIntensity = m_fAdjustableFlameIntensity * fFlameSmoothing +
		                          iNewAdjustableFlameIntensity * (1.f - fFlameSmoothing);
	if(m_fAdjustableFlameIntensity > 1.f)
	{
		m_fAdjustableFlameIntensity = 1.f;
	}

	// Serial out
	if(m_bSerialPortOpen)
	{
		char iEffectState = ComputeEffectState(m_bMainEffectOn, m_bRedOn, m_bGreenOn, m_bYellowOn, m_fAdjustableFlameIntensity);
		m_serial.SendData(&iEffectState, 1);
	}

	//// TEMP_CL - draw text?
	//if(!g_pFont)
	//{
	//	HRESULT hr;
	//	V_RETURN( D3DXCreateFont( pd3dDevice, 30, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
	//							  OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
	//							  L"Arial", &g_pFont ) );
	//	V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite ) );
	//}

 //   CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 25 );
 //   txtHelper.Begin();
 //   txtHelper.SetInsertionPos( DXUTGetD3D9BackBufferSurfaceDesc()->Width / 2 - 256,
 //                               DXUTGetD3D9BackBufferSurfaceDesc()->Height / 2 - 50 );
 //   txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
 //   txtHelper.DrawTextLine( L"Testing..." );
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

	// Bloom - CTL - Change color of the depth feed based on which hands are up
	float fHandVelocityScale = (m_vLeftHandSpeed + m_vRightHandSpeed) * fHandVelocityFactor;
	if(fHandVelocityScale < fHandVelocityScaleLow)
	{
		fHandVelocityScale = fHandVelocityScaleLow;
	}
	else if(fHandVelocityScale > fHandVelocityScaleHigh)
	{
		fHandVelocityScale = fHandVelocityScaleHigh;
	}
	lumens = (BYTE)(lumens * fHandVelocityScale);

	// only render depth field if "flame" is on, otherwise use flat colors
	if(Player != NUI_SKELETON_INVALID_TRACKING_ID && m_bMainEffectOn)
	{
		// both hands forward = yellow
		if(m_bLeftHandForward && m_bRightHandForward)
		{
			q.rgbRed =   ((s & 0x00f0) >> 4)  * 16;
			//q.rgbRed =   ((s >> 3) & 0x0ff);
			q.rgbBlue =  0;
			q.rgbGreen = q.rgbRed;
			return q;
		}
		else if(m_bLeftHandForward)
		{
			q.rgbRed =   ((s & 0x00f0) >> 4)  * 16;
			q.rgbBlue =  0;
			q.rgbGreen = 0;
			return q;
		}
		else if(m_bRightHandForward)
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


	//// TEMP_CL
	//if(Player != NUI_SKELETON_INVALID_TRACKING_ID)
	//{
 //       q.rgbRed =   ((s & 0xf000) >> 12) * 16;
 //       q.rgbBlue =  ((s & 0x0f00) >> 8)  * 16;
 //       q.rgbGreen = ((s & 0x00f0) >> 4)  * 16;
	//	return q;
	//}
	// /Bloom

	// non-flame color to identify players, intensity still driven by velocity
	switch( Player )
    {
    case 0:
        q.rgbRed = lumens / 2;
        q.rgbBlue = lumens / 2;
        q.rgbGreen = lumens / 2;
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
