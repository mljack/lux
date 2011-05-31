/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#include <boost/bind.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>
#include <boost/cast.hpp>

#include <sstream>
#include <clocale>

#include "api.h"

#include <wx/app.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/dcbuffer.h>
#include <wx/splash.h>
#include <wx/clipbrd.h> // CF
#include <boost/date_time/posix_time/posix_time.hpp>

#include "wxluxgui.h"
#include "wxglviewer.h"
#include "wximages.h"

/*** LuxGui ***/

//////////////////////////////////////////////////////////////////////////////////////////////////

#define FLOAT_SLIDER_RES 512.f

#define TM_REINHARD_YWA_RANGE 1.0f
#define TM_REINHARD_PRESCALE_RANGE 8.0f
#define TM_REINHARD_POSTSCALE_RANGE 8.0f
#define TM_REINHARD_BURN_RANGE 12.0f

#define TM_LINEAR_EXPOSURE_LOG_MIN -3.f
#define TM_LINEAR_EXPOSURE_LOG_MAX 2.f
#define TM_LINEAR_SENSITIVITY_RANGE 1000.0f
#define TM_LINEAR_FSTOP_RANGE 64.0f
#define TM_LINEAR_GAMMA_RANGE 5.0f

#define TM_CONTRAST_YWA_LOG_MIN -4.f
#define TM_CONTRAST_YWA_LOG_MAX 4.f

#define TORGB_XWHITE_RANGE 1.0f
#define TORGB_YWHITE_RANGE 1.0f
#define TORGB_XRED_RANGE 1.0f
#define TORGB_YRED_RANGE 1.0f
#define TORGB_XGREEN_RANGE 1.0f
#define TORGB_YGREEN_RANGE 1.0f
#define TORGB_XBLUE_RANGE 1.0f
#define TORGB_YBLUE_RANGE 1.0f

#define TORGB_GAMMA_RANGE 5.0f

#define BLOOMRADIUS_RANGE 1.0f
#define BLOOMWEIGHT_RANGE 1.0f

#define VIGNETTING_SCALE_RANGE 1.0f
#define ABERRATION_AMOUNT_RANGE 1.0f
#define ABERRATION_AMOUNT_FACTOR 0.01f

#define GLARE_AMOUNT_RANGE 0.3f
#define GLARE_RADIUS_RANGE 0.2f
#define GLARE_BLADES_MIN 3
#define GLARE_BLADES_MAX 100

#define GREYC_AMPLITUDE_RANGE 200.0f
#define GREYC_SHARPNESS_RANGE 2.0f
#define GREYC_ANISOTROPY_RANGE 1.0f
#define GREYC_ALPHA_RANGE 12.0f
#define GREYC_SIGMA_RANGE 12.0f
#define GREYC_GAUSSPREC_RANGE 12.0f
#define GREYC_DL_RANGE 1.0f
#define GREYC_DA_RANGE 90.0f
#define GREYC_NB_ITER_RANGE 16.0f

#define CHIU_RADIUS_MIN 1
#define CHIU_RADIUS_MAX 9

#define LG_SCALE_LOG_MIN -4.f
#define LG_SCALE_LOG_MAX 4.f
#define LG_TEMPERATURE_MIN 1000.f
#define LG_TEMPERATURE_MAX 10000.f

//////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_ERROR)
#if defined(__WXOSX_COCOA__) // wx 2.9 and later //
wxDEFINE_EVENT(EVT_LUX_PARSEERROR, wxCommandEvent);
wxDEFINE_EVENT(EVT_LUX_FINISHED, wxCommandEvent);
wxDEFINE_EVENT(EVT_LUX_TONEMAPPED, wxCommandEvent);
wxDEFINE_EVENT(EVT_LUX_FLMLOADERROR, wxCommandEvent);
wxDEFINE_EVENT(EVT_LUX_SAVEDFLM, wxCommandEvent);
#else
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_PARSEERROR)
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_FINISHED)
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_TONEMAPPED)
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_FLMLOADERROR)
DEFINE_LOCAL_EVENT_TYPE(wxEVT_LUX_SAVEDFLM)
#endif

BEGIN_EVENT_TABLE(LuxGui, wxFrame)
	EVT_LUX_ERROR (wxID_ANY, LuxGui::OnError)
	EVT_LUX_VIEWER_SELECTION (wxID_ANY, LuxGui::OnSelection)
	EVT_TIMER     (wxID_ANY, LuxGui::OnTimer)
#if defined(__WXOSX_COCOA__)
	EVT_COMMAND   (wxID_ANY, EVT_LUX_TONEMAPPED, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, EVT_LUX_PARSEERROR, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, EVT_LUX_FINISHED, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, EVT_LUX_FLMLOADERROR, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, EVT_LUX_SAVEDFLM, LuxGui::OnCommand)
#else
	EVT_COMMAND   (wxID_ANY, wxEVT_LUX_TONEMAPPED, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, wxEVT_LUX_PARSEERROR, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, wxEVT_LUX_FINISHED, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, wxEVT_LUX_FLMLOADERROR, LuxGui::OnCommand)
	EVT_COMMAND   (wxID_ANY, wxEVT_LUX_SAVEDFLM, LuxGui::OnCommand)
#endif

#if defined (__WXMSW__) ||  defined (__WXGTK__)
	EVT_ICONIZE   (LuxGui::OnIconize)
#endif
END_EVENT_TABLE()

#if defined(__WXOSX_COCOA__) // wx 2.9 and later //
#define wxEVT_LUX_TONEMAPPED EVT_LUX_TONEMAPPED
#define wxEVT_LUX_PARSEERROR EVT_LUX_PARSEERROR
#define wxEVT_LUX_FINISHED EVT_LUX_FINISHED
#define wxEVT_LUX_FLMLOADERROR EVT_LUX_FLMLOADERROR
#define wxEVT_LUX_SAVEDFLM EVT_LUX_SAVEDFLM
#endif

template<class T> inline T Clamp(T val, T low, T high) {
	return val > low ? (val < high ? val : high) : low;
}

inline int Floor2Int(float val) {
	return static_cast<int>(floorf(val));
}

// Dade - global variable used by LuxGuiErrorHandler()
bool copyLog2Console = false;

LuxGui::LuxGui(wxWindow* parent, bool opengl, bool copylog2console) :
	LuxMainFrame(parent), m_opengl(opengl), m_copyLog2Console(copylog2console), m_showWarningDialog(true) {
	// Load images and icons from header.
	LoadImages();

	// Add histogram image window
	m_HistogramWindow = new LuxHistogramWindow(m_Tab_Control_HistogramPanel, wxID_ANY);
	m_Tab_Control_HistogramPanel->GetSizer()->GetItem(1)->GetSizer()->Add(m_HistogramWindow, 1, wxEXPAND);
	m_Tab_Control_HistogramPanel->Layout();

	// Tabs hidden by default
	m_Tab_Control_ColorSpacePanel->Hide();
	m_Tab_Control_GammaPanel->Hide();
	m_Tab_Control_HistogramPanel->Hide();
	m_Tab_Control_NoiseReductionPanel->Hide();

	m_Tonemap->GetSizer()->Layout();
	m_Tonemap->GetSizer()->FitInside(m_Tonemap);

	// Add custom output viewer window
	if(m_opengl)
		m_renderOutput = new LuxGLViewer(m_renderPage);
	else
		m_renderOutput = new LuxOutputWin(m_renderPage);

	m_renderPage->GetSizer()->GetItem( 1 )->GetSizer()->GetItem( 1 )->GetSizer()->Add( m_renderOutput->GetWindow(), 1, wxALL | wxEXPAND, 5 );
	m_renderPage->Layout();

	m_renderOutput->SetLogoData(luxlogo_png, sizeof(luxlogo_png));

	// Trick to generate resize event and show output window
	// http://lists.wxwidgets.org/pipermail/wx-users/2007-February/097829.html
	SetSize(GetSize());
	m_renderOutput->GetWindow()->Update();

	// Create render output update timer
	m_renderTimer = new wxTimer(this, ID_RENDERUPDATE);
	m_statsTimer = new wxTimer(this, ID_STATSUPDATE);
	m_loadTimer = new wxTimer(this, ID_LOADUPDATE);
	m_saveTimer = new wxTimer(this, ID_SAVEUPDATE);
	m_netTimer = new wxTimer(this, ID_NETUPDATE);

	m_numThreads = 0;
	m_engineThread = NULL;
	m_updateThread = NULL;
	m_flmloadThread = NULL;
	m_flmsaveThread = NULL;

	// Dade - I should use boost bind to avoid global variable
	copyLog2Console = m_copyLog2Console;
	luxErrorHandler(&LuxGuiErrorHandler);

	ChangeRenderState(WAITING);
	m_guiWindowState = SHOWN;

	m_serverUpdateSpin->SetValue( luxGetNetworkServerUpdateInterval() );
	m_auinotebook->SetSelection( 0 );

	m_Lenseffects_enabled = true;
	m_Gamma_enabled = true;
	m_Noisereduction_enabled = true;

	m_auto_tonemap = false; // used in ResetToneMapping
	ResetToneMapping();
	m_auto_tonemap = true;
	ResetLightGroups();

	wxTextValidator vt( wxFILTER_NUMERIC );

	m_TM_Reinhard_prescaleText->SetValidator( vt );
	m_TM_Reinhard_postscaleText->SetValidator( vt );
	m_TM_Reinhard_burnText->SetValidator( vt );

	m_TM_Linear_sensitivityText->SetValidator( vt );
	m_TM_Linear_exposureText->SetValidator( vt );
	m_TM_Linear_fstopText->SetValidator( vt );
	m_TM_Linear_gammaText->SetValidator( vt );

	m_TM_contrast_ywaText->SetValidator( vt );

	m_TORGB_xwhiteText->SetValidator( vt );
	m_TORGB_ywhiteText->SetValidator( vt );
	m_TORGB_xredText->SetValidator( vt );
	m_TORGB_yredText->SetValidator( vt );
	m_TORGB_xgreenText->SetValidator( vt );
	m_TORGB_ygreenText->SetValidator( vt );
	m_TORGB_xblueText->SetValidator( vt );
	m_TORGB_yblueText->SetValidator( vt );

	m_TORGB_gammaText->SetValidator( vt );

	m_bloomradiusText->SetValidator( vt );
	m_bloomweightText->SetValidator( vt );

	m_vignettingamountText->SetValidator( vt );
	m_aberrationamountText->SetValidator( vt );

	m_greyc_iterationsText->SetValidator( vt );
	m_greyc_amplitudeText->SetValidator( vt );
	m_greyc_gaussprecText->SetValidator( vt );
	m_greyc_alphaText->SetValidator( vt );
	m_greyc_sigmaText->SetValidator( vt );
	m_greyc_sharpnessText->SetValidator( vt );
	m_greyc_anisoText->SetValidator( vt );
	m_greyc_spatialText->SetValidator( vt );
	m_greyc_angularText->SetValidator( vt );
}

LuxGui::~LuxGui() {
	delete m_engineThread;
	delete m_updateThread;
	delete m_flmloadThread;
	delete m_flmsaveThread;
	delete m_renderTimer;
	delete m_statsTimer;
	delete m_loadTimer;
	delete m_saveTimer;
	delete m_netTimer;

	for( std::vector<LuxLightGroupPanel*>::iterator it = m_LightGroupPanels.begin(); it != m_LightGroupPanels.end(); it++) {
		LuxLightGroupPanel *currPanel = *it;
		m_LightGroupsSizer->Detach(currPanel);
		delete currPanel;
	}
	m_LightGroupPanels.clear();
}

void LuxGui::ChangeRenderState(LuxGuiRenderState state) {
	switch(state) {
		case WAITING:
			// Waiting for input file. Most controls disabled.
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_view->Enable(ID_RENDER_COPY, false);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, false);
			m_viewerToolBar->Disable();
			m_renderOutput->SetMode(LOGO_VIEW);
			break;
		case RENDERING:
			// Rendering is in progress.
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_PAUSEITEM, true);
			m_render->Enable(ID_STOPITEM, true);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, true);
			m_renderToolBar->EnableTool(ID_STOPTOOL, true);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			m_viewerToolBar->Enable();
			m_renderOutput->SetMode(RENDER_VIEW);
			break;
		case STOPPING:
			// Rendering is being stopped.
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			break;
		case STOPPED:
			// Rendering is stopped.
			m_render->Enable(ID_RESUMEITEM, true);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, true);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			break;
		case PAUSED:
			// Rendering is paused.
			m_render->Enable(ID_RESUMEITEM, true);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, true);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, true);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, true);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			break;
		case FINISHED:
			// Rendering is finished.
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			break;
		case TONEMAPPING:
			// Tonemapping an FLM file
			m_render->Enable(ID_RESUMEITEM, false);
			m_render->Enable(ID_PAUSEITEM, false);
			m_render->Enable(ID_STOPITEM, false);
			m_view->Enable(ID_RENDER_COPY, true);
			m_renderToolBar->EnableTool(ID_RESUMETOOL, false);
			m_renderToolBar->EnableTool(ID_PAUSETOOL, false);
			m_renderToolBar->EnableTool(ID_STOPTOOL, false);
			m_renderToolBar->EnableTool(ID_RENDER_COPY, true);
			m_viewerToolBar->Enable();
			m_renderOutput->SetMode(RENDER_VIEW);
			break;
		case PARSING:
		default:
			// Dade - to avoid a gcc warning
			break;
	}
	m_guiRenderState = state;
}

void LuxGui::LoadImages() {
	wxImage::AddHandler(new wxPNGHandler());

	// App icon - only set on non-windows platforms
#ifndef __WXMSW__
	wxIcon appIcon;
	appIcon.CopyFromBitmap(wxMEMORY_BITMAP(luxicon_png));
	SetIcon(appIcon);
#else
	SetIcon( wxIcon("WXDEFAULT_FRAME", wxBITMAP_TYPE_ICO_RESOURCE ));
#endif

	// wxMac has problems changing an existing tool's icon, so we remove and add then again...
	// Resume toolbar tool
	wxToolBarToolBase *rendertool = m_renderToolBar->RemoveTool(ID_RESUMETOOL);
	rendertool->SetNormalBitmap(wxMEMORY_BITMAP(resume_png));
	m_renderToolBar->InsertTool(0, rendertool);
	m_renderToolBar->Realize();

	// Pause toolbar tool
	wxToolBarToolBase *pausetool = m_renderToolBar->RemoveTool(ID_PAUSETOOL);
	pausetool->SetNormalBitmap(wxMEMORY_BITMAP(pause_png));
	m_renderToolBar->InsertTool(1, pausetool);
	m_renderToolBar->Realize();

	// Stop toolbar tool
	wxToolBarToolBase *stoptool = m_renderToolBar->RemoveTool(ID_STOPTOOL);
	stoptool->SetNormalBitmap(wxMEMORY_BITMAP(stop_png));
	m_renderToolBar->InsertTool(2, stoptool);
	m_renderToolBar->Realize();

	// Add Thread toolbar tool
	wxToolBarToolBase *addtheadtool = m_renderToolBar->RemoveTool(ID_ADD_THREAD);
	addtheadtool->SetNormalBitmap(wxMEMORY_BITMAP(plus_png));
	m_renderToolBar->InsertTool(5, addtheadtool);
	m_renderToolBar->Realize();

	// Add Thread toolbar tool
	wxToolBarToolBase *remtheadtool = m_renderToolBar->RemoveTool(ID_REMOVE_THREAD);
	remtheadtool->SetNormalBitmap(wxMEMORY_BITMAP(minus_png));
	m_renderToolBar->InsertTool(6, remtheadtool);
	m_renderToolBar->Realize();

	// Copy toolbar tool
	wxToolBarToolBase *copytool = m_renderToolBar->RemoveTool(ID_RENDER_COPY);
	copytool->SetNormalBitmap(wxMEMORY_BITMAP(edit_copy_png));
	m_renderToolBar->InsertTool(8, copytool);
	m_renderToolBar->Realize();

	///////////////////////////////////////////////////////////////////////////////

	// NOTE - Ratow - Network toolbar is now a real toolbar component
	wxToolBarToolBase *addServertool = m_networkToolBar->RemoveTool(ID_ADD_SERVER);
	addServertool->SetNormalBitmap(wxMEMORY_BITMAP(plus_png));
	m_networkToolBar->InsertTool(2, addServertool);
	wxToolBarToolBase *removeServertool = m_networkToolBar->RemoveTool(ID_REMOVE_SERVER);
	removeServertool->SetNormalBitmap(wxMEMORY_BITMAP(minus_png));
	m_networkToolBar->InsertTool(3, removeServertool);
	m_networkToolBar->Realize();

	///////////////////////////////////////////////////////////////////////////////

	// Pan toolbar tool
	wxToolBarToolBase *pantool = m_viewerToolBar->RemoveTool(ID_PANTOOL);
	pantool->SetNormalBitmap(wxMEMORY_BITMAP(pan_png));
	m_viewerToolBar->InsertTool(0, pantool);
	m_viewerToolBar->Realize();

	// Zoom toolbar tool
	wxToolBarToolBase *zoomtool = m_viewerToolBar->RemoveTool(ID_ZOOMTOOL);
	zoomtool->SetNormalBitmap(wxMEMORY_BITMAP(zoom_png));
	m_viewerToolBar->InsertTool(1, zoomtool);
	m_viewerToolBar->Realize();

	// Refine toolbar tool
	wxToolBarToolBase *refinetool = m_viewerToolBar->RemoveTool(ID_REFINETOOL);
	refinetool->SetNormalBitmap(wxMEMORY_BITMAP(radiofocus_png));
	m_viewerToolBar->InsertTool(2, refinetool);
	m_viewerToolBar->Realize();

	wxMenuItem *renderitem = m_render->Remove(ID_RESUMEITEM);
	renderitem->SetBitmap(wxMEMORY_BITMAP(resume_png));
	m_render->Insert(0,renderitem);
	// Pause menu item
	wxMenuItem *pauseitem = m_render->Remove(ID_PAUSEITEM);
	pauseitem->SetBitmap(wxMEMORY_BITMAP(pause_png));
	m_render->Insert(1,pauseitem);
	// stop menu item
	wxMenuItem *stopitem = m_render->Remove(ID_STOPITEM);
	stopitem->SetBitmap(wxMEMORY_BITMAP(stop_png));
	m_render->Insert(2,stopitem);

	m_auinotebook->SetPageBitmap(0, wxMEMORY_BITMAP(render_png));
	m_auinotebook->SetPageBitmap(1, wxMEMORY_BITMAP(info_png));
	m_auinotebook->SetPageBitmap(2, wxMEMORY_BITMAP(network_png));
	m_auinotebook->SetPageBitmap(3, wxMEMORY_BITMAP(output_png));

	m_outputNotebook->SetPageBitmap(0, wxMEMORY_BITMAP(n_lightgroup_png));
	m_outputNotebook->SetPageBitmap(1, wxMEMORY_BITMAP(n_tonemap_png));
	// Note - radiance - disabled system tab as not used
	//m_outputNotebook->SetPageBitmap(2, wxMEMORY_BITMAP(n_system_png));

	m_tonemapBitmap->SetBitmap(wxMEMORY_BITMAP(tab_tonemap_png));
	m_histogramBitmap->SetBitmap(wxMEMORY_BITMAP(tab_histogram_png));
	m_bloomBitmap->SetBitmap(wxMEMORY_BITMAP(tab_lenseffects_png));
	m_colorspaceBitmap->SetBitmap(wxMEMORY_BITMAP(tab_colorspace_png));
	m_gammaBitmap->SetBitmap(wxMEMORY_BITMAP(tab_gamma_png));
	m_NoiseReductionBitmap->SetBitmap(wxMEMORY_BITMAP(tab_noisereduction_png));

	m_splashbmp = wxMEMORY_BITMAP(splash_png);

	// window collapse/enable icons/buttons
	m_Tab_ToneMapIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
	m_Tab_ToneMapToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));

	m_Tab_LensEffectsIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
	m_Tab_LensEffectsToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));

	m_Tab_ColorSpaceIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
	m_Tab_ColorSpaceToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));

	m_Tab_GammaIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
	m_Tab_GammaToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));

	m_Tab_HistogramIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
	m_Tab_HistogramToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));

	m_Tab_NoiseReductionIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
	m_Tab_NoiseReductionToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
}

void UpdateParam(luxComponent comp, luxComponentParameters param, double value, int index = 0) {
	if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
	// Update OpenGL viewer
	// m_renderOutput->SetComponentParameter(comp, param, value);
	// Update lux's film
	luxSetParameterValue(comp, param, value, index);
	}
}

void UpdateParam(luxComponent comp, luxComponentParameters param, const char* value, int index = 0) {
	if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
	// Update OpenGL viewer
	// m_renderOutput->SetComponentParameter(comp, param, value);
	// Update lux's film
	luxSetStringParameterValue(comp, param, value, index);
	}
}

double RetrieveParam(bool useDefault, luxComponent comp, luxComponentParameters param, int index = 0) {
	if(useDefault)
		return luxGetDefaultParameterValue(comp, param, index);	
	else
		return luxGetParameterValue(comp, param, index);
}

void LuxGui::OnMenu(wxCommandEvent& event) {
	switch (event.GetId()) {
		case ID_RESUMEITEM:
		case ID_RESUMETOOL:
			if(m_guiRenderState != RENDERING && m_guiRenderState != TONEMAPPING) {
				UpdateNetworkTree();

				// Start display update timer
				m_renderOutput->Reload();
				m_HistogramWindow->Update();

				// Restart timers in case they have been stopped
				if (!m_renderTimer->IsRunning())
					m_renderTimer->Start(1000*luxStatistics("displayInterval"), wxTIMER_CONTINUOUS);
				if (!m_statsTimer->IsRunning())
					m_statsTimer->Start(1000, wxTIMER_CONTINUOUS);
				if (!m_netTimer->IsRunning())
					m_netTimer->Start(1000, wxTIMER_CONTINUOUS);

				if(m_guiRenderState == PAUSED || m_guiRenderState == STOPPED) // Only re-start if we were previously stopped
					luxStart();
				if(m_guiRenderState == STOPPED)
					luxSetHaltSamplePerPixel(-1, false, false);
				ChangeRenderState(RENDERING);
			}
			break;
		case ID_PAUSEITEM:
		case ID_PAUSETOOL:
			if(m_guiRenderState != PAUSED && m_guiRenderState != TONEMAPPING) {
				// We have to check if network rendering is enabled. In this case,
				// we don't stop timer in order to save the image to the disk, etc.
				if (luxGetServerCount() < 1) {
					// Stop display update timer
					m_renderTimer->Stop();
					m_statsTimer->Stop();
				}

				if(m_guiRenderState == RENDERING)
					luxPause();
				ChangeRenderState(PAUSED);
			}
			break;
		case ID_STOPITEM:
		case ID_STOPTOOL:
			if ((m_guiRenderState == RENDERING || m_guiRenderState == PAUSED) && m_guiRenderState != TONEMAPPING) {
				// Dade - we can set the enough sample per pixel condition
				m_renderTimer->Stop();
				// Leave stats timer running so we know when threads stopped
				luxSetHaltSamplePerPixel(1, true, true);
				m_statusBar->SetStatusText(wxT("Waiting for render threads to stop."), 0);
				ChangeRenderState(STOPPING);
			}
			break;
		case ID_FULL_SCREEN: // CF
			if ( !IsFullScreen() )
			{
				m_renderToolBar->Show( false );
				m_viewerToolBar->Show( false );
				m_outputNotebook->Show( false );
				ShowFullScreen( true );
			}
			else
			{
				ShowFullScreen( false );

				if (m_view->IsChecked( ID_TOOL_BAR ) )
				{
					m_renderToolBar->Show( true );
					m_viewerToolBar->Show( true );
				}
				if (m_view->IsChecked( ID_SIDE_PANE ) )
				{
					m_outputNotebook->Show( true );
				}
			}
			Layout();
			m_renderPage->Layout();
			break;
		case ID_TOOL_BAR: // CF
			if ( m_renderToolBar->IsShown() )
			{
				m_renderToolBar->Show( false );
				m_viewerToolBar->Show( false );
			}
			else
			{
				m_renderToolBar->Show( true );
				m_viewerToolBar->Show( true );
			}
			m_renderPage->Layout();
			break;
		case ID_STATUS_BAR: // CF
			if ( m_statusBar->IsShown() )
			{
				m_statusBar->Show( false );
			}
			else
			{
				m_statusBar->Show( true );
			}
			Layout();
			break;
		case ID_SIDE_PANE: // CF
			if ( m_outputNotebook->IsShown() )
			{
				m_outputNotebook->Show( false );
				m_view->Check( ID_SIDE_PANE, false );
			}
			else
			{
				m_outputNotebook->Show( true );
				m_view->Check( ID_SIDE_PANE, true );
			}
			m_renderPage->Layout();
			break;
		case ID_VIEWER_RULERS_DISABLED:
		case ID_VIEWER_RULERS_PIXELS:
		case ID_VIEWER_RULERS_NORMALIZED:
			m_renderOutput->SetRulersEnabled( !m_view->IsChecked(ID_VIEWER_RULERS_DISABLED), m_view->IsChecked(ID_VIEWER_RULERS_NORMALIZED) );
			break;
		case ID_RENDER_COPY: // CF
			if ( m_guiRenderState != WAITING &&
				 wxTheClipboard->Open() )
			{
				m_statusBar->SetStatusText(wxT("Copying..."), 0);

				wxTheClipboard->SetData( new wxBitmapDataObject( *(new wxBitmap( *(new wxImage( luxStatistics("filmXres"), luxStatistics("filmYres"), luxFramebuffer()))))));
				wxTheClipboard->Close();

				m_statusBar->SetStatusText(wxT(""), 0);
			}
			break;
		case ID_CLEAR_LOG: // CF
			m_logTextCtrl->Clear();
			break;
		case ID_ADD_THREAD: // CF
			if ( m_numThreads < 16 ) SetRenderThreads( m_numThreads + 1 );
			break;
		case ID_REMOVE_THREAD: // CF
			if ( m_numThreads > 1 ) SetRenderThreads( m_numThreads - 1 );
			break;
		case ID_ADD_SERVER: // CF
			AddServer();
			break;
		case ID_REMOVE_SERVER: // CF
			RemoveServer();
			break;
		case ID_TM_RESET:
			ResetToneMapping();
			break;
		case wxID_ABOUT:
			new wxSplashScreen(m_splashbmp, wxSPLASH_CENTRE_ON_PARENT, 0, this, -1);
			break;
		case wxID_EXIT:
			Close(false);
			break;
		case ID_PAN_MODE: // CF
			m_viewerToolBar->ToggleTool( ID_PANTOOL, true );
			m_renderOutput->SetMode(PANZOOM);
			break;
		case ID_PANTOOL:
			m_renderOutput->SetMode(PANZOOM);
			break;
		case ID_ZOOM_MODE: // CF
			m_viewerToolBar->ToggleTool( ID_ZOOMTOOL, true );
			m_renderOutput->SetMode(SELECTION);
			break;
		case ID_ZOOMTOOL:
		case ID_REFINETOOL:
			m_renderOutput->SetMode(SELECTION);
			break;
		case ID_TM_APPLY:
			{
				// Apply tonemapping
				ApplyTonemapping();
			}
			break;
		case ID_AUTO_TONEMAP:
			{
				// Set auto tonemapping
				if(m_auto_tonemapCheckBox->IsChecked())
					m_auto_tonemap = true;
				else
					m_auto_tonemap = false;
			}
			break;
		case ID_TM_KERNELCHOICE:
			{
				// Change tonemapping kernel
				int choice = event.GetInt();
				SetTonemapKernel(choice);
			}
			break;
		case ID_TORGB_COLORSPACECHOICE:
			{
				// Change colorspace preset
				int choice = event.GetInt();
				SetColorSpacePreset(choice);
			}
			break;
		case ID_TORGB_WHITEPOINTCHOICE:
			{
				// Change whitepoint preset
				int choice = event.GetInt();
				SetWhitepointPreset(choice);
			}
			break;
		case ID_COMPUTEBLOOMLAYER:
			{
				// Signal film to update bloom layer at next tonemap
				UpdateParam(LUX_FILM, LUX_FILM_UPDATEBLOOMLAYER, 1.0f);
				m_deletebloomlayer->Enable(true);
				m_bloomweightSlider->Enable(true);
				m_bloomweightText->Enable(true);
				ApplyTonemapping(true);
			}
			break;
		case ID_DELETEBLOOMLAYER:
			{
				// Signal film to delete bloom layer
				UpdateParam(LUX_FILM, LUX_FILM_DELETEBLOOMLAYER, 1.0f);
				m_deletebloomlayer->Enable(false);
				m_bloomweightSlider->Enable(false);
				m_bloomweightText->Enable(false);
				ApplyTonemapping(true);
			}
			break;
		// Vignetting Enable/Disable checkbox
		case ID_VIGNETTING_ENABLED:
			{
				if(m_vignettingenabledCheckBox->IsChecked())
					m_Vignetting_Enabled = true;
				else
					m_Vignetting_Enabled = false;
				UpdateParam(LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		// Chromatic Aberration Enable/Disable checkbox
		case ID_ABERRATION_ENABLED:
			{
				if(m_aberrationEnabled->IsChecked())
					m_Aberration_enabled = true;
				else
					m_Aberration_enabled = false;
				UpdateParam(LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		// Compute glare layer button
		case ID_COMPUTEGLARELAYER:
			{
				// Signal film to update glare layer at next tonemap
				UpdateParam(LUX_FILM, LUX_FILM_UPDATEGLARELAYER, 1.0);
				m_deleteglarelayer->Enable(true);
				m_glareamountSlider->Enable(true);
				m_glareamountText->Enable(true);
				ApplyTonemapping(true);
			}
			break;
		// Delete glare layer button
		case ID_DELETEGLARELAYER:
			{
				// Signal film to update glare layer at next tonemap
				UpdateParam(LUX_FILM, LUX_FILM_DELETEGLARELAYER, 1.0f);
				m_deleteglarelayer->Enable(false);
				m_glareamountSlider->Enable(false);
				m_glareamountText->Enable(false);
				ApplyTonemapping(true);
			}
			break;

		// GREYC Enable/Disable checkbox
		case ID_GREYC_ENABLED:
			{
				if(m_greyc_EnabledCheckBox->IsChecked())
					m_GREYC_enabled = true;
				else
					m_GREYC_enabled = false;
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		// GREYC Enable/Disable fast approximation checkbox
		case ID_GREYC_FASTAPPROX:
			{
				if(m_greyc_fastapproxCheckBox->IsChecked())
					m_GREYC_fast_approx = true;
				else
					m_GREYC_fast_approx = false;
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX, m_GREYC_fast_approx);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		// GREYC interpolation mode
		case ID_GREYC_INTERPOLATIONCHOICE:
			{
				// Change interpolation mode
				m_GREYC_interp = event.GetInt();
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP, m_GREYC_interp);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		// Chiu Enable/Disable checkbox
		case ID_CHIU_ENABLED:
			{
				if(m_chiu_enableCheckBox->IsChecked())
					m_Chiu_enabled = true;
				else
					m_Chiu_enabled = false;
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		// Chiu Include Center checkbox
		case ID_CHIU_INCLUDECENTER:
			{
				if(m_chiu_includecenterCheckBox->IsChecked())
					m_Chiu_includecenter = true;
				else
					m_Chiu_includecenter = false;
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER, m_Chiu_includecenter);
				if(m_auto_tonemap && m_Chiu_enabled) ApplyTonemapping();
			}
			break;
		// Histogram options
		case ID_HISTOGRAM_CHANNEL:
			{
				switch( event.GetInt() ){
					case 0: m_HistogramWindow->SetOption(LUX_HISTOGRAM_RGB_ADD); break;
					case 1: m_HistogramWindow->SetOption(LUX_HISTOGRAM_RGB); break;
					case 2: m_HistogramWindow->SetOption(LUX_HISTOGRAM_RED); break;
					case 3: m_HistogramWindow->SetOption(LUX_HISTOGRAM_GREEN); break;
					case 4: m_HistogramWindow->SetOption(LUX_HISTOGRAM_BLUE); break;
					case 5: m_HistogramWindow->SetOption(LUX_HISTOGRAM_VALUE); break;
				}
				m_HistogramWindow->Update();
			}
			break;
		case ID_HISTOGRAM_LOG:
			{
				if(event.IsChecked())
					m_HistogramWindow->SetOption(LUX_HISTOGRAM_LOG);
				else
					m_HistogramWindow->ClearOption(LUX_HISTOGRAM_LOG);
				m_HistogramWindow->Update();
			}
			break;
		default:
			break;
	}
	m_HistogramWindow->SetEnabled(m_HistogramWindow->IsShownOnScreen());
}

void LuxGui::OnMouse(wxMouseEvent &event) {
	switch (event.GetId()) {
		// TABS Hide/Show(collapse)
		case ID_TAB_HISTOGRAM:
			{
				if( m_Tab_Control_HistogramPanel->IsShown() ){
					m_Tab_Control_HistogramPanel->Hide();
					m_Tab_HistogramIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
					m_HistogramWindow->SetEnabled(false);
				}else{
					ApplyTonemapping();
					m_Tab_Control_HistogramPanel->Show(true);
					m_Tab_HistogramIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
					m_HistogramWindow->SetEnabled(true);
				}
				m_HistogramPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_TONEMAP:
			{
				if( m_Tab_Control_ToneMapPanel->IsShown() ){
					m_Tab_Control_ToneMapPanel->Hide();
					m_Tab_ToneMapIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_ToneMapPanel->Show(true);
					m_Tab_ToneMapIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_TonemapOptionsPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_LENSEFFECTS:
			{
				if( m_Tab_Control_LensEffectsPanel->IsShown() ){
					m_Tab_Control_LensEffectsPanel->Hide();
					m_Tab_LensEffectsIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_LensEffectsPanel->Show(true);
					m_Tab_LensEffectsIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_BloomOptionsPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_LENSEFFECTS_TOGGLE:
			{
				if( m_Lenseffects_enabled ){
					m_Lenseffects_enabled = false;
					m_Tab_LensEffectsToggleIcon->SetBitmap(wxMEMORY_BITMAP(powerofficon_png));
				}else{
					m_Lenseffects_enabled = true;
					m_Tab_LensEffectsToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
				}
				m_LensEffectsAuiNotebook->Enable(m_Lenseffects_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled && m_Lenseffects_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled && m_Lenseffects_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Lenseffects_enabled ? m_Glare_amount : 0.0);
				UpdateParam(LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_Lenseffects_enabled ? m_bloomweight : 0.0);
				if (!m_Lenseffects_enabled)
					// prevent bloom update
					UpdateParam(LUX_FILM, LUX_FILM_UPDATEBLOOMLAYER, 0.0);
				
				if(m_auto_tonemap) ApplyTonemapping();				
				break;
			}
		case ID_TAB_COLORSPACE:
			{
				if( m_Tab_Control_ColorSpacePanel->IsShown() ){
					m_Tab_Control_ColorSpacePanel->Hide();
					m_Tab_ColorSpaceIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_ColorSpacePanel->Show(true);
					m_Tab_ColorSpaceIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_ColorSpaceOptionsPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_GAMMA:
			{
				if( m_Tab_Control_GammaPanel->IsShown() ){
					m_Tab_Control_GammaPanel->Hide();
					m_Tab_GammaIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_GammaPanel->Show(true);
					m_Tab_GammaIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_GammaOptionsPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_GAMMA_TOGGLE:
			{
				if( m_Gamma_enabled ){
					m_Gamma_enabled = false;
					m_Tab_GammaToggleIcon->SetBitmap(wxMEMORY_BITMAP(powerofficon_png));
				}else{
					m_Gamma_enabled = true;
					m_Tab_GammaToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
				}
				m_TORGB_gammaSlider->Enable(m_Gamma_enabled);
				m_TORGB_gammaText->Enable(m_Gamma_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_GAMMA, (m_Gamma_enabled ? m_TORGB_gamma : 1.0));
				if(m_auto_tonemap) ApplyTonemapping();				
				break;
			}
		case ID_TAB_NOISEREDUCTION:
			{
				if( m_Tab_Control_NoiseReductionPanel->IsShown() ){
					m_Tab_Control_NoiseReductionPanel->Hide();
					m_Tab_NoiseReductionIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_NoiseReductionPanel->Show(true);
					m_Tab_NoiseReductionIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_NoiseOptionsPanel->GetSizer()->Layout();
				m_Tonemap->GetSizer()->Layout();
				m_Tonemap->GetSizer()->FitInside(m_Tonemap);
				m_Tonemap->Refresh();
			}
			break;
		case ID_TAB_NOISEREDUCTION_TOGGLE:
			{
				if( m_Noisereduction_enabled ){
					m_Noisereduction_enabled = false;
					m_Tab_NoiseReductionToggleIcon->SetBitmap(wxMEMORY_BITMAP(powerofficon_png));
				}else{
					m_Noisereduction_enabled = true;
					m_Tab_NoiseReductionToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
				}
				m_NoiseReductionAuiNotebook->Enable(m_Noisereduction_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled && m_Noisereduction_enabled);
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled && m_Noisereduction_enabled);
				if(m_auto_tonemap) ApplyTonemapping();				
				break;
			}
		default:
			break;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void LuxGui::ApplyTonemapping(bool withlayercomputation) {
	if(m_updateThread == NULL && ( luxStatistics("sceneIsReady") || luxStatistics("filmIsReady") ) &&
    (m_guiWindowState == SHOWN || m_guiRenderState == FINISHED)) {
		if(!withlayercomputation) {
			luxError(LUX_NOERROR, LUX_INFO, "GUI: Updating framebuffer...");
			m_statusBar->SetStatusText(wxT("Tonemapping..."), 0);
		} else {
			luxError(LUX_NOERROR, LUX_INFO, "GUI: Updating framebuffer/Computing Lens Effect Layer(s)...");
			m_statusBar->SetStatusText(wxT("Computing Lens Effect Layer(s) & Tonemapping..."), 0);
		}
		m_updateThread = new boost::thread(boost::bind(&LuxGui::UpdateThread, this));
	}
}

void LuxGui::OnText(wxCommandEvent& event) {
	if ( event.GetEventType() != wxEVT_COMMAND_TEXT_ENTER ) return;

	switch (event.GetId()) {
		// Reinhard tonemapper options
		case ID_TM_REINHARD_PRESCALE_TEXT:
			if( m_TM_Reinhard_prescaleText->IsModified() )
			{
				wxString st = m_TM_Reinhard_prescaleText->GetValue();
				st.ToDouble( &m_TM_reinhard_prescale );

				if ( m_TM_reinhard_prescale > TM_REINHARD_PRESCALE_RANGE ) m_TM_reinhard_prescale = TM_REINHARD_PRESCALE_RANGE;
				else if ( m_TM_reinhard_prescale < 0.f ) m_TM_reinhard_prescale = 0.f;

				st = wxString::Format( _("%.02f"), m_TM_reinhard_prescale );
				m_TM_Reinhard_prescaleText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE ) * (m_TM_reinhard_prescale));
				m_TM_Reinhard_prescaleSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE, m_TM_reinhard_prescale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_REINHARD_POSTSCALE_TEXT:
			if ( m_TM_Reinhard_postscaleText->IsModified() )
			{
				wxString st = m_TM_Reinhard_postscaleText->GetValue();
				st.ToDouble( &m_TM_reinhard_postscale );

				if ( m_TM_reinhard_postscale > TM_REINHARD_POSTSCALE_RANGE ) m_TM_reinhard_postscale = TM_REINHARD_POSTSCALE_RANGE;
				else if ( m_TM_reinhard_postscale < 0.f ) m_TM_reinhard_postscale = 0.f;

				st = wxString::Format( _("%.02f"), m_TM_reinhard_postscale );
				m_TM_Reinhard_postscaleText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE ) * (m_TM_reinhard_postscale));
				m_TM_Reinhard_postscaleSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE, m_TM_reinhard_postscale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_REINHARD_BURN_TEXT:
			if ( m_TM_Reinhard_burnText->IsModified() )
			{
				wxString st = m_TM_Reinhard_burnText->GetValue();
				st.ToDouble( &m_TM_reinhard_burn );

				if ( m_TM_reinhard_burn > TM_REINHARD_BURN_RANGE ) m_TM_reinhard_burn = TM_REINHARD_BURN_RANGE;
				else if ( m_TM_reinhard_burn < 0 ) m_TM_reinhard_burn = 0;

				st = wxString::Format( _("%.02f"), m_TM_reinhard_burn );
				m_TM_Reinhard_burnText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE ) * (m_TM_reinhard_burn));
				m_TM_Reinhard_burnSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_BURN, m_TM_reinhard_burn);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Linear tonemapper options
		case ID_TM_LINEAR_SENSITIVITY_TEXT:
			if ( m_TM_Linear_sensitivityText->IsModified() )
			{
				wxString st = m_TM_Linear_sensitivityText->GetValue();
				st.ToDouble( &m_TM_linear_sensitivity );

				if ( m_TM_linear_sensitivity > TM_LINEAR_SENSITIVITY_RANGE ) m_TM_linear_sensitivity = TM_LINEAR_SENSITIVITY_RANGE;
				else if ( m_TM_linear_sensitivity < 0 ) m_TM_linear_sensitivity = 0;

				st = wxString::Format( _("%.02f"), m_TM_linear_sensitivity );
				m_TM_Linear_sensitivityText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE ) * (m_TM_linear_sensitivity));
				m_TM_Linear_sensitivitySlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_EXPOSURE_TEXT:
			if ( m_TM_Linear_exposureText->IsModified() )
			{
				wxString st = m_TM_Linear_exposureText->GetValue();
				st.ToDouble( &m_TM_linear_exposure );

				if ( m_TM_linear_exposure > powf(10.f, TM_LINEAR_EXPOSURE_LOG_MAX) ) 
					m_TM_linear_exposure = powf(10.f, TM_LINEAR_EXPOSURE_LOG_MAX);
				else if ( m_TM_linear_exposure < powf(10.f, TM_LINEAR_EXPOSURE_LOG_MIN) ) 
					m_TM_linear_exposure = powf(10.f, TM_LINEAR_EXPOSURE_LOG_MIN);

				st = wxString::Format( _("%.02f"), m_TM_linear_exposure );
				m_TM_Linear_exposureText->SetValue( st );
				m_TM_Linear_exposureSlider->SetValue( ValueToLogSliderVal(m_TM_linear_exposure, 
					TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX) );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_FSTOP_TEXT:
			if ( m_TM_Linear_fstopText->IsModified() )
			{
				wxString st = m_TM_Linear_fstopText->GetValue();
				st.ToDouble( &m_TM_linear_fstop );

				if ( m_TM_linear_fstop > TM_LINEAR_FSTOP_RANGE ) m_TM_linear_fstop = TM_LINEAR_FSTOP_RANGE;
				else if ( m_TM_linear_fstop < 0 ) m_TM_linear_fstop = 0;

				st = wxString::Format( _("%.02f"), m_TM_linear_fstop );
				m_TM_Linear_fstopText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE ) * (m_TM_linear_fstop));
				m_TM_Linear_fstopSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_GAMMA_TEXT:
			if ( m_TM_Linear_gammaText->IsModified() )
			{
				wxString st = m_TM_Linear_gammaText->GetValue();
				st.ToDouble( &m_TM_linear_gamma );

				if ( m_TM_linear_gamma > TM_LINEAR_GAMMA_RANGE ) m_TM_linear_gamma = TM_LINEAR_GAMMA_RANGE;
				else if ( m_TM_linear_gamma < 0 ) m_TM_linear_gamma = 0;

				st = wxString::Format( _("%.02f"), m_TM_linear_gamma );
				m_TM_Linear_gammaText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE ) * (m_TM_linear_gamma));
				m_TM_Linear_gammaSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA, m_TM_linear_gamma);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Contrast tonemapper options
		case ID_TM_CONTRAST_YWA_TEXT:
			if ( m_TM_contrast_ywaText->IsModified() )
			{
				wxString st = m_TM_contrast_ywaText->GetValue();
				st.ToDouble( &m_TM_contrast_ywa );

				if ( m_TM_contrast_ywa > powf(10.f, TM_CONTRAST_YWA_LOG_MAX) ) 
					m_TM_contrast_ywa = powf(10.f, TM_CONTRAST_YWA_LOG_MAX);
				else if ( m_TM_contrast_ywa < powf(10.f, TM_CONTRAST_YWA_LOG_MIN) ) 
					m_TM_contrast_ywa = powf(10.f, TM_CONTRAST_YWA_LOG_MIN);

				st = wxString::Format( _("%.02f"), m_TM_contrast_ywa );
				m_TM_contrast_ywaText->SetValue( st );
				m_TM_contrast_ywaSlider->SetValue( ValueToLogSliderVal(m_TM_contrast_ywa, 
					TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX) );
				UpdateParam(LUX_FILM, LUX_FILM_TM_CONTRAST_YWA, m_TM_contrast_ywa);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;


		// Colorspace options
		case ID_TORGB_XWHITE_TEXT:
			if ( m_TORGB_xwhiteText->IsModified() )
			{
				wxString st = m_TORGB_xwhiteText->GetValue();
				st.ToDouble( &m_TORGB_xwhite );

				if ( m_TORGB_xwhite > TORGB_XWHITE_RANGE ) m_TORGB_xwhite = TORGB_XWHITE_RANGE;
				else if ( m_TORGB_xwhite < 0 ) m_TORGB_xwhite = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_xwhite );
				m_TORGB_xwhiteText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE ) * (m_TORGB_xwhite));
				m_TORGB_xwhiteSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YWHITE_TEXT:
			if ( m_TORGB_ywhiteText->IsModified() )
			{
				wxString st = m_TORGB_ywhiteText->GetValue();
				st.ToDouble( &m_TORGB_ywhite );

				if ( m_TORGB_ywhite > TORGB_YWHITE_RANGE ) m_TORGB_ywhite = TORGB_YWHITE_RANGE;
				else if ( m_TORGB_ywhite < 0 ) m_TORGB_ywhite = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_ywhite );
				m_TORGB_ywhiteText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE ) * (m_TORGB_ywhite));
				m_TORGB_ywhiteSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XRED_TEXT:
			if ( m_TORGB_xredText->IsModified() )
			{
				wxString st = m_TORGB_xredText->GetValue();
				st.ToDouble( &m_TORGB_xred );

				if ( m_TORGB_xred > TORGB_XRED_RANGE ) m_TORGB_xred = TORGB_XRED_RANGE;
				else if ( m_TORGB_xred < 0 ) m_TORGB_xred = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_xred );
				m_TORGB_xredText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_XRED_RANGE ) * (m_TORGB_xred));
				m_TORGB_xredSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YRED_TEXT:
			if ( m_TORGB_yredText->IsModified() )
			{
				wxString st = m_TORGB_yredText->GetValue();
				st.ToDouble( &m_TORGB_yred );

				if ( m_TORGB_yred > TORGB_YRED_RANGE ) m_TORGB_yred = TORGB_YRED_RANGE;
				else if ( m_TORGB_yred < 0 ) m_TORGB_yred = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_yred );
				m_TORGB_yredText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_YRED_RANGE ) * (m_TORGB_yred));
				m_TORGB_yredSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XGREEN_TEXT:
			if ( m_TORGB_xgreenText->IsModified() )
			{
				wxString st = m_TORGB_xgreenText->GetValue();
				st.ToDouble( &m_TORGB_xgreen );

				if ( m_TORGB_xgreen > TORGB_XGREEN_RANGE ) m_TORGB_xgreen = TORGB_XGREEN_RANGE;
				else if ( m_TORGB_xgreen < 0 ) m_TORGB_xgreen = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_xgreen );
				m_TORGB_xgreenText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE ) * (m_TORGB_xgreen));
				m_TORGB_xgreenSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YGREEN_TEXT:
			if ( m_TORGB_ygreenText->IsModified() )
			{
				wxString st = m_TORGB_ygreenText->GetValue();
				st.ToDouble( &m_TORGB_ygreen );

				if ( m_TORGB_ygreen > TORGB_YGREEN_RANGE ) m_TORGB_ygreen = TORGB_YGREEN_RANGE;
				else if ( m_TORGB_ygreen < 0 ) m_TORGB_ygreen = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_ygreen );
				m_TORGB_ygreenText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE ) * (m_TORGB_ygreen));
				m_TORGB_ygreenSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XBLUE_TEXT:
			if ( m_TORGB_xblueText->IsModified() )
			{
				wxString st = m_TORGB_xblueText->GetValue();
				st.ToDouble( &m_TORGB_xblue );

				if ( m_TORGB_xblue > TORGB_XBLUE_RANGE ) m_TORGB_xblue = TORGB_XBLUE_RANGE;
				else if ( m_TORGB_xblue < 0 ) m_TORGB_xblue = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_xblue );
				m_TORGB_xblueText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE ) * (m_TORGB_xblue));
				m_TORGB_xblueSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YBLUE_TEXT:
			if ( m_TORGB_yblueText->IsModified() )
			{
				wxString st = m_TORGB_yblueText->GetValue();
				st.ToDouble( &m_TORGB_yblue );

				if ( m_TORGB_yblue > TORGB_YBLUE_RANGE ) m_TORGB_yblue = TORGB_YBLUE_RANGE;
				else if ( m_TORGB_yblue < 0 ) m_TORGB_yblue = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_yblue );
				m_TORGB_yblueText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE ) * (m_TORGB_yblue));
				m_TORGB_yblueSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Gamma
		case ID_TORGB_GAMMA_TEXT:
			if ( m_TORGB_gammaText->IsModified() )
			{
				wxString st = m_TORGB_gammaText->GetValue();
				st.ToDouble( &m_TORGB_gamma );

				if ( m_TORGB_gamma > TORGB_GAMMA_RANGE ) m_TORGB_gamma = TORGB_GAMMA_RANGE;
				else if ( m_TORGB_gamma < 0 ) m_TORGB_gamma = 0;

				st = wxString::Format( _("%.02f"), m_TORGB_gamma );
				m_TORGB_gammaText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE ) * (m_TORGB_gamma));
				m_TORGB_gammaSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Bloom
		case ID_BLOOMRADIUS_TEXT:
			if ( m_bloomradiusText->IsModified() )
			{
				wxString st = m_bloomradiusText->GetValue();
				st.ToDouble( &m_bloomradius );

				if ( m_bloomradius > BLOOMRADIUS_RANGE ) m_bloomradius = BLOOMRADIUS_RANGE;
				else if ( m_bloomradius < 0 ) m_bloomradius = 0;

				st = wxString::Format( _("%.02f"), m_bloomradius );
				m_bloomradiusText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE ) * (m_bloomradius));
				m_bloomradiusSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_BLOOMRADIUS, m_bloomradius);
			}
			break;
		case ID_BLOOMWEIGHT_TEXT:
			if ( m_bloomweightText->IsModified() )
			{
				wxString st = m_bloomweightText->GetValue();
				st.ToDouble( &m_bloomweight );

				if ( m_bloomweight > BLOOMWEIGHT_RANGE ) m_bloomweight = BLOOMWEIGHT_RANGE;
				else if ( m_bloomweight < 0 ) m_bloomweight = 0;

				st = wxString::Format( _("%.02f"), m_bloomweight );
				m_bloomweightText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE ) * (m_bloomweight));
				m_bloomweightSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_bloomweight);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Vignetting
		case ID_VIGNETTINGAMOUNT_TEXT:
			if ( m_vignettingamountText->IsModified() )
			{
				wxString st = m_vignettingamountText->GetValue();
				st.ToDouble( &m_Vignetting_Scale );

				if ( m_Vignetting_Scale > VIGNETTING_SCALE_RANGE ) m_Vignetting_Scale = VIGNETTING_SCALE_RANGE;
				else if ( VIGNETTING_SCALE_RANGE < -VIGNETTING_SCALE_RANGE ) m_Vignetting_Scale = -VIGNETTING_SCALE_RANGE;

				st = wxString::Format( _("%.02f"), m_Vignetting_Scale );
				m_vignettingamountText->SetValue( st );
				int val;
				if ( m_Vignetting_Scale >= 0.f )	
					val = (int) (FLOAT_SLIDER_RES/2) + (( (FLOAT_SLIDER_RES/2) / VIGNETTING_SCALE_RANGE ) * (m_Vignetting_Scale));
				else {
					val = (int)(( FLOAT_SLIDER_RES/2 * VIGNETTING_SCALE_RANGE ) * (1.f - fabsf(m_Vignetting_Scale)));
				}
				m_vignettingamountSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_VIGNETTING_SCALE, m_Vignetting_Scale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Aberration
		case ID_ABERRATIONAMOUNT_TEXT:
			if ( m_aberrationamountText->IsModified() )
			{
				wxString st = m_aberrationamountText->GetValue();
				st.ToDouble( &m_Aberration_amount );

				if ( m_Aberration_amount > ABERRATION_AMOUNT_RANGE ) 
					m_Aberration_amount = ABERRATION_AMOUNT_RANGE;
				else if ( m_Aberration_amount < 0 ) 
					m_Aberration_amount = 0;

				st = wxString::Format( _("%.02f"), m_Aberration_amount );
				m_aberrationamountText->SetValue( st );
				int val = (int) (( FLOAT_SLIDER_RES / ABERRATION_AMOUNT_RANGE ) * (m_Aberration_amount));
				m_aberrationamountSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_ABERRATION_AMOUNT, ABERRATION_AMOUNT_FACTOR * m_Aberration_amount);
				if(m_auto_tonemap && m_Aberration_enabled) ApplyTonemapping();
			}
			break;

		// Glare
		case ID_GLAREAMOUNT_TEXT:
			if ( m_glareamountText->IsModified() )
			{
				wxString st = m_glareamountText->GetValue();
				st.ToDouble( &m_Glare_amount );

				if ( m_Glare_amount > GLARE_AMOUNT_RANGE ) 
					m_Glare_amount = GLARE_AMOUNT_RANGE;
				else if ( m_Glare_amount < 0 ) 
					m_Glare_amount = 0;

				st = wxString::Format( _("%.02f"), m_Glare_amount );
				m_glareamountText->SetValue( st );
				int val = (int) (( FLOAT_SLIDER_RES / GLARE_AMOUNT_RANGE ) * (m_Glare_amount));
				m_glareamountSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Glare_amount);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_GLARERADIUS_TEXT:
			if ( m_glareradiusText->IsModified() )
			{
				wxString st = m_glareradiusText->GetValue();
				st.ToDouble( &m_Glare_radius );

				if ( m_Glare_radius > GLARE_RADIUS_RANGE ) 
					m_Glare_radius = GLARE_RADIUS_RANGE;
				else if ( m_Glare_radius < 0 ) 
					m_Glare_radius = 0;

				st = wxString::Format( _("%.02f"), m_Glare_radius );
				m_glareradiusText->SetValue( st );
				int val = (int) (( FLOAT_SLIDER_RES / GLARE_RADIUS_RANGE ) * (m_Glare_radius));
				m_glareradiusSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_GLARE_RADIUS, m_Glare_radius);
			}
			break;

		// GREYCStoration
		case ID_GREYC_ITERATIONS_TEXT:
			if ( m_greyc_iterationsText->IsModified() )
			{
				wxString st = m_greyc_iterationsText->GetValue();
				st.ToDouble( &m_GREYC_nb_iter );

				if ( m_GREYC_nb_iter > GREYC_NB_ITER_RANGE ) m_GREYC_nb_iter = GREYC_NB_ITER_RANGE;
				else if ( m_GREYC_nb_iter < 1 ) m_GREYC_nb_iter = 1;

				st = wxString::Format( _("%.02f"), m_GREYC_nb_iter );
				m_greyc_iterationsText->SetValue( st );
				int val = (int) m_GREYC_nb_iter;
				m_greyc_iterationsSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER, m_GREYC_nb_iter);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_AMPLITUDE_TEXT:
			if ( m_greyc_amplitudeText->IsModified() )
			{
				wxString st = m_greyc_amplitudeText->GetValue();
				st.ToDouble( &m_GREYC_amplitude );

				if ( m_GREYC_amplitude > GREYC_AMPLITUDE_RANGE ) m_GREYC_amplitude = GREYC_AMPLITUDE_RANGE;
				else if ( m_GREYC_amplitude < 0 ) m_GREYC_amplitude = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_amplitude );
				m_greyc_amplitudeText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE ) * (m_GREYC_amplitude));
				m_greyc_amplitudeSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE, m_GREYC_amplitude);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_GAUSSPREC_TEXT:
			if ( m_greyc_gaussprecText->IsModified() )
			{
				wxString st = m_greyc_gaussprecText->GetValue();
				st.ToDouble( &m_GREYC_gauss_prec );

				if ( m_GREYC_gauss_prec > GREYC_GAUSSPREC_RANGE ) m_GREYC_gauss_prec = GREYC_GAUSSPREC_RANGE;
				else if ( m_GREYC_gauss_prec < 0 ) m_GREYC_gauss_prec = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_gauss_prec );
				m_greyc_gaussprecText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE ) * (m_GREYC_gauss_prec));
				m_greyc_gausprecSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC, m_GREYC_gauss_prec);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_ALPHA_TEXT:
			if ( m_greyc_alphaText->IsModified() )
			{
				wxString st = m_greyc_alphaText->GetValue();
				st.ToDouble( &m_GREYC_alpha );

				if ( m_GREYC_alpha > GREYC_ALPHA_RANGE ) m_GREYC_alpha = GREYC_ALPHA_RANGE;
				else if ( m_GREYC_alpha < 0 ) m_GREYC_alpha = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_alpha );
				m_greyc_alphaText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE ) * (m_GREYC_alpha));
				m_greyc_alphaSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA, m_GREYC_alpha);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_SIGMA_TEXT:
			if ( m_greyc_sigmaText->IsModified() )
			{
				wxString st = m_greyc_sigmaText->GetValue();
				st.ToDouble( &m_GREYC_sigma );

				if ( m_GREYC_sigma > GREYC_SIGMA_RANGE ) m_GREYC_sigma = GREYC_SIGMA_RANGE;
				else if ( m_GREYC_sigma < 0 ) m_GREYC_sigma = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_sigma );
				m_greyc_sigmaText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE ) * (m_GREYC_sigma));
				m_greyc_sigmaSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA, m_GREYC_sigma);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_SHARPNESS_TEXT:
			if ( m_greyc_sharpnessText->IsModified() )
			{
				wxString st = m_greyc_sharpnessText->GetValue();
				st.ToDouble( &m_GREYC_sharpness );

				if ( m_GREYC_sharpness > GREYC_SHARPNESS_RANGE ) m_GREYC_sharpness = GREYC_SHARPNESS_RANGE;
				else if ( m_GREYC_sharpness < 0 ) m_GREYC_sharpness = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_sharpness );
				m_greyc_sharpnessText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE ) * (m_GREYC_sharpness));
				m_greyc_sharpnessSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS, m_GREYC_sharpness);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_ANISO_TEXT:
			if ( m_greyc_anisoText->IsModified() )
			{
				wxString st = m_greyc_anisoText->GetValue();
				st.ToDouble( &m_GREYC_anisotropy );

				if ( m_GREYC_anisotropy > GREYC_ANISOTROPY_RANGE ) m_GREYC_anisotropy = GREYC_ANISOTROPY_RANGE;
				else if ( m_GREYC_anisotropy < 0 ) m_GREYC_anisotropy = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_anisotropy );
				m_greyc_anisoText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE ) * (m_GREYC_anisotropy));
				m_greyc_anisoSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY, m_GREYC_anisotropy);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_SPATIAL_TEXT:
			if ( m_greyc_spatialText->IsModified() )
			{
				wxString st = m_greyc_spatialText->GetValue();
				st.ToDouble( &m_GREYC_dl );

				if ( m_GREYC_dl > GREYC_DL_RANGE ) m_GREYC_dl = GREYC_DL_RANGE;
				else if ( m_GREYC_dl < 0 ) m_GREYC_dl = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_dl );
				m_greyc_spatialText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_DL_RANGE ) * (m_GREYC_dl));
				m_greyc_spatialSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_DL, m_GREYC_dl);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_GREYC_ANGULAR_TEXT:
			if ( m_greyc_angularText->IsModified() )
			{
				wxString st = m_greyc_angularText->GetValue();
				st.ToDouble( &m_GREYC_da );

				if ( m_GREYC_da > GREYC_DA_RANGE ) m_GREYC_da = GREYC_DA_RANGE;
				else if ( m_GREYC_da < 0 ) m_GREYC_da = 0;

				st = wxString::Format( _("%.02f"), m_GREYC_da );
				m_greyc_angularText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / GREYC_DA_RANGE ) * (m_GREYC_da));
				m_greyc_angularSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_DA, m_GREYC_da);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
			}
			break;
		case ID_CHIU_RADIUS_TEXT:
			if ( m_chiu_radiusText->IsModified() )
			{
				wxString st = m_chiu_radiusText->GetValue();
				st.ToDouble( &m_Chiu_radius );

				if ( m_Chiu_radius > CHIU_RADIUS_MAX ) m_Chiu_radius = CHIU_RADIUS_MAX;
				else if ( m_Chiu_radius < CHIU_RADIUS_MIN ) m_Chiu_radius = CHIU_RADIUS_MIN;

				st = wxString::Format( _("%.02f"), m_Chiu_radius );
				m_chiu_radiusText->SetValue( st );
				int val = (int)(( FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX - CHIU_RADIUS_MIN) ) * (m_Chiu_radius - CHIU_RADIUS_MIN) );
				m_chiu_radiusSlider->SetValue( val );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS, m_Chiu_radius);
				if(m_auto_tonemap && m_Chiu_enabled) ApplyTonemapping();
			}
			break;		
		case ID_SERVER_TEXT:
			AddServer();
			break;
		default:
			break;
	}
}

void LuxGui::OnCheckBox(wxCommandEvent& event)
{
// Radiance - event does'nt work ? use OnMenu redirection for now.
}

void LuxGui::OnColourChanged(wxColourPickerEvent &event)
{
}

void LuxGui::OnScroll( wxScrollEvent& event ){
	switch (event.GetId()) {
		// Reinhard tonemapper options
		case ID_TM_REINHARD_PRESCALE:
			{
				m_TM_reinhard_prescale = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_reinhard_prescale );
				m_TM_Reinhard_prescaleText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE, m_TM_reinhard_prescale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_REINHARD_POSTSCALE:
			{
				m_TM_reinhard_postscale = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_reinhard_postscale );
				m_TM_Reinhard_postscaleText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE, m_TM_reinhard_postscale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_REINHARD_BURN:
			{
				m_TM_reinhard_burn = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_reinhard_burn );
				m_TM_Reinhard_burnText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_REINHARD_BURN, m_TM_reinhard_burn);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Linear tonemapper options
		case ID_TM_LINEAR_SENSITIVITY:
			{
				m_TM_linear_sensitivity = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_linear_sensitivity );
				m_TM_Linear_sensitivityText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_EXPOSURE:
			{
				m_TM_linear_exposure = LogSliderValToValue(event.GetPosition(), TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX);
				wxString st = wxString::Format( _("%.02f"), m_TM_linear_exposure );
				m_TM_Linear_exposureText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_FSTOP:
			{
				m_TM_linear_fstop = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_linear_fstop );
				m_TM_Linear_fstopText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TM_LINEAR_GAMMA:
			{
				m_TM_linear_gamma = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TM_linear_gamma );
				m_TM_Linear_gammaText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA, m_TM_linear_gamma);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Contrast tonemapper options
		case ID_TM_CONTRAST_YWA:
			{
				m_TM_contrast_ywa = LogSliderValToValue(event.GetPosition(), TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX);
				wxString st = wxString::Format( _("%.02f"), m_TM_contrast_ywa );
				m_TM_contrast_ywaText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TM_CONTRAST_YWA, m_TM_contrast_ywa);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Colorspace options
		case ID_TORGB_XWHITE:
			{
				m_TORGB_xwhite = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_xwhite );
				m_TORGB_xwhiteText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YWHITE:
			{
				m_TORGB_ywhite = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_ywhite );
				m_TORGB_ywhiteText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XRED:
			{
				m_TORGB_xred = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_XRED_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_xred );
				m_TORGB_xredText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YRED:
			{
				m_TORGB_yred = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_YRED_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_yred );
				m_TORGB_yredText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XGREEN:
			{
				m_TORGB_xgreen = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_xgreen );
				m_TORGB_xgreenText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YGREEN:
			{
				m_TORGB_ygreen = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_ygreen );
				m_TORGB_ygreenText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_XBLUE:
			{
				m_TORGB_xblue = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_xblue );
				m_TORGB_xblueText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_YBLUE:
			{
				m_TORGB_yblue = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_yblue );
				m_TORGB_yblueText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_TORGB_GAMMA:
			{
				m_TORGB_gamma = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_TORGB_gamma );
				m_TORGB_gammaText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Bloom
		case ID_BLOOMRADIUS:
			{
				m_bloomradius = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_bloomradius );
				m_bloomradiusText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_BLOOMRADIUS, m_bloomradius);
			}
			break;
		case ID_BLOOMWEIGHT:
			{
				m_bloomweight = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_bloomweight );
				m_bloomweightText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_bloomweight);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Vignetting
		case ID_VIGNETTINGAMOUNT:
			{
				double pos = (double)event.GetPosition() / FLOAT_SLIDER_RES;
				pos -= 0.5f;
				pos *= VIGNETTING_SCALE_RANGE * 2.f;
				m_Vignetting_Scale = pos;
				wxString st = wxString::Format( _("%.02f"), m_Vignetting_Scale );
				m_vignettingamountText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_VIGNETTING_SCALE, m_Vignetting_Scale);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// Chromatic Aberration
		case ID_ABERRATIONAMOUNT:
			{
				double pos = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / ABERRATION_AMOUNT_RANGE );
				m_Aberration_amount = pos;
				wxString st = wxString::Format( _("%.02f"), m_Aberration_amount );
				m_aberrationamountText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_ABERRATION_AMOUNT, ABERRATION_AMOUNT_FACTOR * m_Aberration_amount);
				if(m_auto_tonemap && m_Aberration_enabled) ApplyTonemapping();
			}
			break;

		// Glare
		case ID_GLAREAMOUNT:
			{
				double pos = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GLARE_AMOUNT_RANGE );
				m_Glare_amount = pos;
				wxString st = wxString::Format( _("%.02f"), m_Glare_amount );
				m_glareamountText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Glare_amount);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;
		case ID_GLARERADIUS:
			{
				double pos = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GLARE_RADIUS_RANGE );
				m_Glare_radius = pos;
				wxString st = wxString::Format( _("%.02f"), m_Glare_radius );
				m_glareradiusText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_GLARE_RADIUS, m_Glare_radius);
				if(m_auto_tonemap) ApplyTonemapping();
			}
			break;

		// GREYCStoration
		case ID_GREYC_ITERATIONS:
			{
				m_GREYC_nb_iter = (double)event.GetPosition();
				wxString st = wxString::Format( _("%.02f"), m_GREYC_nb_iter );
				m_greyc_iterationsText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER, m_GREYC_nb_iter);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_AMPLITUDE:
			{
				m_GREYC_amplitude = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_amplitude );
				m_greyc_amplitudeText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE, m_GREYC_amplitude);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_GAUSSPREC:
			{
				m_GREYC_gauss_prec = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_gauss_prec );
				m_greyc_gaussprecText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC, m_GREYC_gauss_prec);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_ALPHA:
			{
				m_GREYC_alpha = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_alpha );
				m_greyc_alphaText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA, m_GREYC_alpha);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_SIGMA:
			{
				m_GREYC_sigma = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_sigma );
				m_greyc_sigmaText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA, m_GREYC_sigma);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_SHARPNESS:
			{
				m_GREYC_sharpness = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_sharpness );
				m_greyc_sharpnessText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS, m_GREYC_sharpness);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_ANISO:
			{
				m_GREYC_anisotropy = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_anisotropy );
				m_greyc_anisoText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY, m_GREYC_anisotropy);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_SPATIAL:
			{
				m_GREYC_dl = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_DL_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_dl );
				m_greyc_spatialText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_DL, m_GREYC_dl);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;
		case ID_GREYC_ANGULAR:
			{
				m_GREYC_da = (double)event.GetPosition() / ( FLOAT_SLIDER_RES / GREYC_DA_RANGE );
				wxString st = wxString::Format( _("%.02f"), m_GREYC_da );
				m_greyc_angularText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_DA, m_GREYC_da);
				if(m_auto_tonemap && m_GREYC_enabled) ApplyTonemapping();
				
			}
			break;

		case ID_CHIU_RADIUS:
			{
				m_Chiu_radius = (double)event.GetPosition() / (FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX - CHIU_RADIUS_MIN)) + CHIU_RADIUS_MIN;
				wxString st = wxString::Format( _("%.02f"), m_Chiu_radius );
				m_chiu_radiusText->SetValue( st );
				UpdateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS, m_Chiu_radius);
				if(m_auto_tonemap && m_Chiu_enabled) ApplyTonemapping();
				
			}
			break;
		default:
			break;
	}
}

void LuxGui::OnFocus( wxFocusEvent& event ){
	//set text control's contents to current value when losing focus
	if( event.GetEventType() != wxEVT_KILL_FOCUS ) return;

	switch (event.GetId()) {
		// Reinhard tonemapper options
		case ID_TM_REINHARD_PRESCALE_TEXT:
			m_TM_Reinhard_prescaleText->SetValue( wxString::Format( _("%.02f"), m_TM_reinhard_prescale ) );
			break;
		case ID_TM_REINHARD_POSTSCALE_TEXT:
			m_TM_Reinhard_postscaleText->SetValue( wxString::Format( _("%.02f"), m_TM_reinhard_postscale ) );
			break;
		case ID_TM_REINHARD_BURN_TEXT:
			m_TM_Reinhard_burnText->SetValue( wxString::Format( _("%.02f"), m_TM_reinhard_burn ) );
			break;
		// Linear tonemapper options
		case ID_TM_LINEAR_SENSITIVITY_TEXT:
			m_TM_Linear_sensitivityText->SetValue( wxString::Format( _("%.02f"), m_TM_linear_sensitivity ) );
			break;
		case ID_TM_LINEAR_EXPOSURE_TEXT:
			m_TM_Linear_exposureText->SetValue( wxString::Format( _("%.02f"), m_TM_linear_exposure ) );
			break;
		case ID_TM_LINEAR_FSTOP_TEXT:
			m_TM_Linear_fstopText->SetValue( wxString::Format( _("%.02f"), m_TM_linear_fstop ) );
			break;
		case ID_TM_LINEAR_GAMMA_TEXT:
			m_TM_Linear_gammaText->SetValue( wxString::Format( _("%.02f"), m_TM_linear_gamma ) );
			break;
		// Contrast tonemapper options
		case ID_TM_CONTRAST_YWA_TEXT:
			m_TM_contrast_ywaText->SetValue( wxString::Format( _("%.02f"), m_TM_contrast_ywa ) );
			break;
		// Colorspace options
		case ID_TORGB_XWHITE_TEXT:
			m_TORGB_xwhiteText->SetValue( wxString::Format( _("%.02f"), m_TORGB_xwhite ) );
			break;
		case ID_TORGB_YWHITE_TEXT:
			m_TORGB_ywhiteText->SetValue( wxString::Format( _("%.02f"), m_TORGB_ywhite ) );
			break;
		case ID_TORGB_XRED_TEXT:
			m_TORGB_xredText->SetValue( wxString::Format( _("%.02f"), m_TORGB_xred ) );
			break;
		case ID_TORGB_YRED_TEXT:
			m_TORGB_yredText->SetValue( wxString::Format( _("%.02f"), m_TORGB_yred ) );
			break;
		case ID_TORGB_XGREEN_TEXT:
			m_TORGB_xgreenText->SetValue( wxString::Format( _("%.02f"), m_TORGB_xgreen ) );
			break;
		case ID_TORGB_YGREEN_TEXT:
			m_TORGB_ygreenText->SetValue( wxString::Format( _("%.02f"), m_TORGB_ygreen ) );
			break;
		case ID_TORGB_XBLUE_TEXT:
			m_TORGB_xblueText->SetValue( wxString::Format( _("%.02f"), m_TORGB_xblue ) );
			break;
		case ID_TORGB_YBLUE_TEXT:
			m_TORGB_yblueText->SetValue( wxString::Format( _("%.02f"), m_TORGB_yblue ) );
			break;
		case ID_TORGB_GAMMA_TEXT:
			m_TORGB_gammaText->SetValue( wxString::Format( _("%.02f"), m_TORGB_gamma ) );
			break;
		// Bloom options
		case ID_BLOOMRADIUS_TEXT:
			m_bloomradiusText->SetValue( wxString::Format( _("%.02f"), m_bloomradius ) );
			break;
		case ID_BLOOMWEIGHT_TEXT:
			m_bloomweightText->SetValue( wxString::Format( _("%.02f"), m_bloomweight ) );
			break;
		// Vignetting options
		case ID_VIGNETTINGAMOUNT_TEXT:
			m_vignettingamountText->SetValue( wxString::Format( _("%.02f"), m_Vignetting_Scale ) );
			break;
		// Aberration options
		case ID_ABERRATIONAMOUNT_TEXT:
			m_aberrationamountText->SetValue( wxString::Format( _("%.02f"), m_Aberration_amount ) );
			break;
		// Glare options
		case ID_GLAREAMOUNT_TEXT:
			m_glareamountText->SetValue( wxString::Format( _("%.02f"), m_Glare_amount ) );
			break;
		case ID_GLARERADIUS_TEXT:
			m_glareradiusText->SetValue( wxString::Format( _("%.02f"), m_Glare_radius ) );
			break;
		// GREYC options
		case ID_GREYC_ITERATIONS_TEXT:
			m_greyc_iterationsText->SetValue( wxString::Format( _("%.02f"), m_GREYC_nb_iter ) );
			break;
		case ID_GREYC_AMPLITUDE_TEXT:
			m_greyc_amplitudeText->SetValue( wxString::Format( _("%.02f"), m_GREYC_amplitude ) );
			break;
		case ID_GREYC_GAUSSPREC_TEXT:
			m_greyc_gaussprecText->SetValue( wxString::Format( _("%.02f"), m_GREYC_gauss_prec ) );
			break;
		case ID_GREYC_ALPHA_TEXT:
			m_greyc_alphaText->SetValue( wxString::Format( _("%.02f"), m_GREYC_alpha ) );
			break;
		case ID_GREYC_SIGMA_TEXT:
			m_greyc_sigmaText->SetValue( wxString::Format( _("%.02f"), m_GREYC_sigma ) );
			break;
		case ID_GREYC_SHARPNESS_TEXT:
			m_greyc_sharpnessText->SetValue( wxString::Format( _("%.02f"), m_GREYC_sharpness ) );
			break;
		case ID_GREYC_ANISO_TEXT:
			m_greyc_anisoText->SetValue( wxString::Format( _("%.02f"), m_GREYC_anisotropy ) );
			break;
		case ID_GREYC_SPATIAL_TEXT:
			m_greyc_spatialText->SetValue( wxString::Format( _("%.02f"), m_GREYC_dl ) );
			break;
		case ID_GREYC_ANGULAR_TEXT:
			m_greyc_angularText->SetValue( wxString::Format( _("%.02f"), m_GREYC_da ) );
			break;
		case ID_CHIU_RADIUS_TEXT:
			m_chiu_radiusText->SetValue( wxString::Format( _("%0.2f"), m_Chiu_radius ) );
			break;
		default:
			break;
	}

}

void LuxGui::SetTonemapKernel(int choice) {
	m_TM_kernelChoice->SetSelection(choice);
	UpdateParam(LUX_FILM, LUX_FILM_TM_TONEMAPKERNEL, choice);
	m_TM_kernel = choice;
	switch (choice) {
		case 0: {
				// Reinhard
				m_TonemapReinhardOptionsPanel->Show();
				m_TonemapLinearOptionsPanel->Hide();
				m_TonemapContrastOptionsPanel->Hide(); }
			break;
		case 1: {
				// Linear
				m_TonemapReinhardOptionsPanel->Hide();
				m_TonemapLinearOptionsPanel->Show();
				m_TonemapContrastOptionsPanel->Hide(); }
			break;
		case 2: {
				// Contrast
				m_TonemapReinhardOptionsPanel->Hide();
				m_TonemapLinearOptionsPanel->Hide();
				m_TonemapContrastOptionsPanel->Show(); }
			break;
		case 3: {
				// MaxWhite
				m_TonemapReinhardOptionsPanel->Hide();
				m_TonemapLinearOptionsPanel->Hide();
				m_TonemapContrastOptionsPanel->Hide(); }
			break;
		default:
			break;
	}
	m_Tonemap->GetSizer()->Layout();
	m_Tonemap->GetSizer()->FitInside(m_Tonemap);
	Refresh();
	if(m_auto_tonemap) ApplyTonemapping();
}

void LuxGui::SetColorSpacePreset(int choice) {
	m_TORGB_colorspaceChoice->SetSelection(choice);
	m_TORGB_whitepointChoice->SetSelection(0);
	switch (choice) {
		case 0: {
				// sRGB - HDTV (ITU-R BT.709-5)
				m_TORGB_xwhite = 0.314275f; m_TORGB_ywhite = 0.329411f;
				m_TORGB_xred = 0.63f; m_TORGB_yred = 0.34f;
				m_TORGB_xgreen = 0.31f; m_TORGB_ygreen = 0.595f;
				m_TORGB_xblue = 0.155f; m_TORGB_yblue = 0.07f; }
			break;
		case 1: {
				// ROMM RGB
				m_TORGB_xwhite = 0.346f; m_TORGB_ywhite = 0.359f;
				m_TORGB_xred = 0.7347f; m_TORGB_yred = 0.2653f;
				m_TORGB_xgreen = 0.1596f; m_TORGB_ygreen = 0.8404f;
				m_TORGB_xblue = 0.0366f; m_TORGB_yblue = 0.0001f; }
			break;
		case 2: {
				// Adobe RGB 98
				m_TORGB_xwhite = 0.313f; m_TORGB_ywhite = 0.329f;
				m_TORGB_xred = 0.64f; m_TORGB_yred = 0.34f;
				m_TORGB_xgreen = 0.21f; m_TORGB_ygreen = 0.71f;
				m_TORGB_xblue = 0.15f; m_TORGB_yblue = 0.06f; }
			break;
		case 3: {
				// Apple RGB
				m_TORGB_xwhite = 0.313f; m_TORGB_ywhite = 0.329f;
				m_TORGB_xred = 0.625f; m_TORGB_yred = 0.34f;
				m_TORGB_xgreen = 0.28f; m_TORGB_ygreen = 0.595f;
				m_TORGB_xblue = 0.155f; m_TORGB_yblue = 0.07f; }
			break;
		case 4: {
				// NTSC (FCC 1953, ITU-R BT.470-2 System M)
				m_TORGB_xwhite = 0.310f; m_TORGB_ywhite = 0.316f;
				m_TORGB_xred = 0.67f; m_TORGB_yred = 0.33f;
				m_TORGB_xgreen = 0.21f; m_TORGB_ygreen = 0.71f;
				m_TORGB_xblue = 0.14f; m_TORGB_yblue = 0.08f; }
			break;
		case 5: {
				// NTSC (1979) (SMPTE C, SMPTE-RP 145)
				m_TORGB_xwhite = 0.313f; m_TORGB_ywhite = 0.329f;
				m_TORGB_xred = 0.63f; m_TORGB_yred = 0.34f;
				m_TORGB_xgreen = 0.31f; m_TORGB_ygreen = 0.595f;
				m_TORGB_xblue = 0.155f; m_TORGB_yblue = 0.07f; }
			break;
		case 6: {
				// PAL/SECAM (EBU 3213, ITU-R BT.470-6)
				m_TORGB_xwhite = 0.313f; m_TORGB_ywhite = 0.329f;
				m_TORGB_xred = 0.64f; m_TORGB_yred = 0.33f;
				m_TORGB_xgreen = 0.29f; m_TORGB_ygreen = 0.60f;
				m_TORGB_xblue = 0.15f; m_TORGB_yblue = 0.06f; }
			break;
		case 7: {
				// CIE (1931) E
				m_TORGB_xwhite = 0.333f; m_TORGB_ywhite = 0.333f;
				m_TORGB_xred = 0.7347f; m_TORGB_yred = 0.2653f;
				m_TORGB_xgreen = 0.2738f; m_TORGB_ygreen = 0.7174f;
				m_TORGB_xblue = 0.1666f; m_TORGB_yblue = 0.0089f; }
			break;
		default:
			break;
	}

	// Update values in film trough API
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);

	UpdateTonemapWidgetValues();
	Refresh();
	if(m_auto_tonemap) ApplyTonemapping();
}

void LuxGui::SetWhitepointPreset(int choice) {
	m_TORGB_whitepointChoice->SetSelection(choice);

	// first choice is "none"
	if (choice < 1)
		return;

	// default to E
	float x = 1;
	float y = 1;
	float z = 1;
	switch (choice - 1) {
		case 0: 
			{
				// A
				x = 1.09850;
				y = 1.00000;
				z = 0.35585;
			}
			break;
		case 1: 
			{
				// B
				x = 0.99072;
				y = 1.00000;
				z = 0.85223;
			}
			break;
		case 2: 
			{
				// C
				x = 0.98074;
				y = 1.00000;
				z = 1.18232;
			}
			break;
		case 3: 
			{
				// D50
				x = 0.96422;
				y = 1.00000;
				z = 0.82521;
			}
			break;
		case 4: 
			{
				// D55
				x = 0.95682;
				y = 1.00000;
				z = 0.92149;
			}
			break;
		case 5: 
			{
				// D65
				x = 0.95047;
				y = 1.00000;
				z = 1.08883;
			}
			break;
		case 6: 
			{
				// D75
				x = 0.94972;
				y = 1.00000;
				z = 1.22638;
			}
			break;
		case 7: 
			{
				// E
				x = 1.00000;
				y = 1.00000;
				z = 1.00000;
			}
			break;
		case 8: 
			{
				// F2
				x = 0.99186;
				y = 1.00000;
				z = 0.67393;
			}
			break;
		case 9: 
			{
				// F7
				x = 0.95041;
				y = 1.00000;
				z = 1.08747;
			}
			break;
		case 10: 
			{
				// F11
				x = 1.00962;
				y = 1.00000;
				z = 0.64350;
			}
			break;
		default:
			break;
	}

	m_TORGB_xwhite = x / (x+y+z);
	m_TORGB_ywhite = y / (x+y+z);

	// Update values in film trough API
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	UpdateParam(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);

	UpdateTonemapWidgetValues();
	Refresh();
	if(m_auto_tonemap) ApplyTonemapping();
}

void LuxGui::ResetToneMapping(){
	if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
		ResetToneMappingFromFilm( true );
		return;
	}

	m_TM_kernel = 0; // *

	m_TM_reinhard_prescale = 1.0;
	m_TM_reinhard_postscale = 1.0;
	m_TM_reinhard_burn = 6.0;

	m_TM_linear_exposure = 1.0f;
	m_TM_linear_sensitivity = 50.0f;
	m_TM_linear_fstop = 2.8;
	m_TM_linear_gamma = 1.0;

	m_TM_contrast_ywa = 0.1;

	m_bloomradius = 0.07f;
	m_bloomweight = 0.25f;

	m_Vignetting_Enabled = false;
	m_Vignetting_Scale = 0.4;
	
	m_Aberration_enabled = false;
	m_Aberration_amount = 0.5;

	m_Glare_amount = 0.03f;
	m_Glare_radius = 0.03f;
	m_Glare_blades = 3;

	m_TORGB_xwhite = 0.314275f;
	m_TORGB_ywhite = 0.329411f;
	m_TORGB_xred = 0.63f;
	m_TORGB_yred = 0.34f;
	m_TORGB_xgreen = 0.31f;
	m_TORGB_ygreen = 0.595f;
	m_TORGB_xblue = 0.155f;
	m_TORGB_yblue = 0.07f;
	m_TORGB_gamma = 2.2f;

	m_GREYC_enabled = false;
	m_GREYC_fast_approx = true;
	m_GREYC_amplitude = 40.0;
	m_GREYC_sharpness = 0.8;
	m_GREYC_anisotropy = 0.2;
	m_GREYC_alpha = 0.8;
	m_GREYC_sigma = 1.1;
	m_GREYC_gauss_prec = 2.0;
	m_GREYC_dl = 0.8;
	m_GREYC_da = 30.0;
	m_GREYC_nb_iter = 1;
	m_GREYC_interp = 0;

	m_Chiu_enabled = false;
	m_Chiu_includecenter = false;
	m_Chiu_radius = 3;

	UpdateTonemapWidgetValues();
	m_outputNotebook->Enable( false );
	Refresh();
}

void LuxGui::UpdatedTonemapParam() {
	if(m_auto_tonemap) ApplyTonemapping();
}

void LuxGui::UpdateTonemapWidgetValues() {
	// Tonemap kernel selection
	SetTonemapKernel(m_TM_kernel);

	// Reinhard widgets
	m_TM_Reinhard_prescaleSlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE) * m_TM_reinhard_prescale) );
	wxString st = wxString::Format( _("%.02f"), m_TM_reinhard_prescale );
	m_TM_Reinhard_prescaleText->SetValue(st);

	m_TM_Reinhard_postscaleSlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE) * (m_TM_reinhard_postscale)));
	st = wxString::Format( _("%.02f"), m_TM_reinhard_postscale );
	m_TM_Reinhard_postscaleText->SetValue(st);

	m_TM_Reinhard_burnSlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE) * m_TM_reinhard_burn));
	st = wxString::Format( _("%.02f"), m_TM_reinhard_burn );
	m_TM_Reinhard_burnText->SetValue(st);

	// Linear widgets
	m_TM_Linear_exposureSlider->SetValue( ValueToLogSliderVal(m_TM_linear_exposure, 
		TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX) );
	st = wxString::Format( _("%.02f"), m_TM_linear_exposure );
	m_TM_Linear_exposureText->SetValue(st);

	m_TM_Linear_sensitivitySlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE) * m_TM_linear_sensitivity) );
	st = wxString::Format( _("%.02f"), m_TM_linear_sensitivity );
	m_TM_Linear_sensitivityText->SetValue(st);

	m_TM_Linear_fstopSlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE) * (m_TM_linear_fstop)));
	st = wxString::Format( _("%.02f"), m_TM_linear_fstop );
	m_TM_Linear_fstopText->SetValue(st);

	m_TM_Linear_gammaSlider->SetValue( (int)((FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE) * m_TM_linear_gamma));
	st = wxString::Format( _("%.02f"), m_TM_linear_gamma );
	m_TM_Linear_gammaText->SetValue(st);

	// Contrast widgets
	m_TM_contrast_ywaSlider->SetValue( ValueToLogSliderVal(m_TM_contrast_ywa, 
		TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX) );
	st = wxString::Format( _("%.02f"), m_TM_contrast_ywa );
	m_TM_contrast_ywaText->SetValue(st);

	// Colorspace widgets
	m_TORGB_xwhiteSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE) * m_TORGB_xwhite));
	st = wxString::Format( _("%.02f"), m_TORGB_xwhite );
	m_TORGB_xwhiteText->SetValue(st);
	m_TORGB_ywhiteSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE) * m_TORGB_ywhite));
	st = wxString::Format( _("%.02f"), m_TORGB_ywhite );
	m_TORGB_ywhiteText->SetValue(st);

	m_TORGB_xredSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_XRED_RANGE) * m_TORGB_xred));
	st = wxString::Format( _("%.02f"), m_TORGB_xred );
	m_TORGB_xredText->SetValue(st);
	m_TORGB_yredSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_YRED_RANGE) * m_TORGB_yred));
	st = wxString::Format( _("%.02f"), m_TORGB_yred );
	m_TORGB_yredText->SetValue(st);

	m_TORGB_xgreenSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE) * m_TORGB_xgreen));
	st = wxString::Format( _("%.02f"), m_TORGB_xgreen );
	m_TORGB_xgreenText->SetValue(st);
	m_TORGB_ygreenSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE) * m_TORGB_ygreen));
	st = wxString::Format( _("%.02f"), m_TORGB_ygreen );
	m_TORGB_ygreenText->SetValue(st);

	m_TORGB_xblueSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE) * m_TORGB_xblue));
	st = wxString::Format( _("%.02f"), m_TORGB_xblue );
	m_TORGB_xblueText->SetValue(st);
	m_TORGB_yblueSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE) * m_TORGB_yblue));
	st = wxString::Format( _("%.02f"), m_TORGB_yblue );
	m_TORGB_yblueText->SetValue(st);

	m_TORGB_gammaSlider->SetValue( (int)((FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE) * m_TORGB_gamma));
	st = wxString::Format( _("%.02f"), m_TORGB_gamma );
	m_TORGB_gammaText->SetValue(st);
	
	// Bloom widgets
	m_bloomradiusSlider->SetValue( (int)((FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE) * m_bloomradius));
	st = wxString::Format( _("%.02f"), m_bloomradius );
	m_bloomradiusText->SetValue(st);

	m_bloomweightSlider->SetValue( (int)((FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE) * m_bloomweight));
	st = wxString::Format( _("%.02f"), m_bloomweight );
	m_bloomweightText->SetValue(st);

	// GREYC widgets
	m_greyc_iterationsSlider->SetValue( (int) m_GREYC_nb_iter );
	st = wxString::Format( _("%.02f"), m_GREYC_nb_iter );
	m_greyc_iterationsText->SetValue(st);

	m_greyc_amplitudeSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE) * m_GREYC_amplitude));
	st = wxString::Format( _("%.02f"), m_GREYC_amplitude );
	m_greyc_amplitudeText->SetValue(st);

	m_greyc_gausprecSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE) * m_GREYC_gauss_prec));
	st = wxString::Format( _("%.02f"), m_GREYC_gauss_prec );
	m_greyc_gaussprecText->SetValue(st);

	m_greyc_alphaSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE) * m_GREYC_alpha));
	st = wxString::Format( _("%.02f"), m_GREYC_alpha );
	m_greyc_alphaText->SetValue(st);

	m_greyc_sigmaSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE) * m_GREYC_sigma));
	st = wxString::Format( _("%.02f"), m_GREYC_sigma );
	m_greyc_sigmaText->SetValue(st);

	m_greyc_sharpnessSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE) * m_GREYC_sharpness));
	st = wxString::Format( _("%.02f"), m_GREYC_sharpness );
	m_greyc_sharpnessText->SetValue(st);

	m_greyc_anisoSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE) * m_GREYC_anisotropy));
	st = wxString::Format( _("%.02f"), m_GREYC_anisotropy );
	m_greyc_anisoText->SetValue(st);

	m_greyc_spatialSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_DL_RANGE) * m_GREYC_dl));
	st = wxString::Format( _("%.02f"), m_GREYC_dl );
	m_greyc_spatialText->SetValue(st);

	m_greyc_angularSlider->SetValue( (int)((FLOAT_SLIDER_RES / GREYC_DA_RANGE) * m_GREYC_da));
	st = wxString::Format( _("%.02f"), m_GREYC_da );
	m_greyc_angularText->SetValue(st);

	m_GREYCinterpolationChoice->SetSelection( m_GREYC_interp );

	m_greyc_EnabledCheckBox->SetValue( m_GREYC_enabled );
	m_greyc_fastapproxCheckBox->SetValue( m_GREYC_fast_approx );

	m_chiu_enableCheckBox->SetValue( m_Chiu_enabled );
	m_chiu_includecenterCheckBox->SetValue( m_Chiu_includecenter );

	m_chiu_radiusSlider->SetValue( (int)((FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX-CHIU_RADIUS_MIN)) * (m_Chiu_radius-CHIU_RADIUS_MIN)) );
	st = wxString::Format( _("%.02f"), m_Chiu_radius );
	m_chiu_radiusText->SetValue(st);

	// Histogram
	m_HistogramWindow->SetEnabled(m_Tab_Control_HistogramPanel->IsShown());

	Refresh();
}

void LuxGui::ResetToneMappingFromFilm( bool useDefaults ){

	double t;

	m_TM_kernel = (int) RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_TONEMAPKERNEL);

	m_TM_reinhard_prescale = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE);
	m_TM_reinhard_postscale = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE);
	m_TM_reinhard_burn = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_BURN);

	m_TM_linear_exposure = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE);
	m_TM_linear_sensitivity = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY);
	m_TM_linear_fstop = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP);
	m_TM_linear_gamma = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA);

	m_TM_contrast_ywa = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_CONTRAST_YWA);

	m_TORGB_xwhite = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_WHITE);
	m_TORGB_ywhite = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_WHITE);
	m_TORGB_xred = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_RED);
	m_TORGB_yred = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_RED);
	m_TORGB_xgreen = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_GREEN);
	m_TORGB_ygreen = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_GREEN);
	m_TORGB_xblue = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_BLUE);
	m_TORGB_yblue = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_BLUE);
	m_TORGB_gamma =  RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_GAMMA);

	m_bloomradius = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_BLOOMRADIUS);
	m_bloomweight = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_BLOOMWEIGHT);

	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_VIGNETTING_ENABLED);
	m_Vignetting_Enabled = t != 0.0;
	m_Vignetting_Scale = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_VIGNETTING_SCALE);

	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_ABERRATION_ENABLED);
	m_Aberration_enabled = t != 0.0;
	m_Aberration_amount = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_ABERRATION_AMOUNT);


	m_Glare_amount = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_AMOUNT);
	m_Glare_radius = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_RADIUS);
	m_Glare_blades = (int)RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_BLADES);

	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED);
	if(t != 0.0) m_GREYC_enabled = true;
	else m_GREYC_enabled = false;
	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX);
	if(t != 0.0) m_GREYC_fast_approx = true;
	else m_GREYC_fast_approx = false;

	m_GREYC_amplitude = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE);
	m_GREYC_sharpness = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS);
	m_GREYC_anisotropy = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY);
	m_GREYC_alpha = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA);
	m_GREYC_sigma = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA);
	m_GREYC_gauss_prec = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC);
	m_GREYC_dl = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_DL);
	m_GREYC_da = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_DA);
	m_GREYC_nb_iter = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER);
	m_GREYC_interp = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP);

	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED);
	m_Chiu_enabled = t != 0.0;
	t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER);
	m_Chiu_includecenter = t != 0.0;
	m_Chiu_radius = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE, m_TM_reinhard_prescale);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE, m_TM_reinhard_postscale);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_BURN, m_TM_reinhard_burn);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA, m_TM_linear_gamma);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_CONTRAST_YWA, m_TM_contrast_ywa);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);

	luxSetParameterValue(LUX_FILM, LUX_FILM_BLOOMRADIUS, m_bloomradius);
	luxSetParameterValue(LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_bloomweight);

	luxSetParameterValue(LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_VIGNETTING_SCALE, m_Vignetting_Scale);

	luxSetParameterValue(LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_ABERRATION_AMOUNT, m_Aberration_amount);

	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Glare_amount);
	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_RADIUS, m_Glare_radius);
	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_BLADES, m_Glare_blades);

	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX, m_GREYC_fast_approx);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE, m_GREYC_amplitude);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS, m_GREYC_sharpness);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY, m_GREYC_anisotropy);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA, m_GREYC_alpha);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA, m_GREYC_sigma);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC, m_GREYC_gauss_prec);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_DL, m_GREYC_dl);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_DA, m_GREYC_da);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER, m_GREYC_nb_iter);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP, m_GREYC_interp);

	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER, m_Chiu_includecenter);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS, m_Chiu_radius);
	
	UpdateTonemapWidgetValues();
	if(m_auto_tonemap) ApplyTonemapping();
}

void LuxGui::UpdateLightGroupWidgetValues() {
	for( std::vector<LuxLightGroupPanel*>::iterator it = m_LightGroupPanels.begin(); it != m_LightGroupPanels.end(); it++) {
		(*it)->UpdateWidgetValues();
	}
	Refresh();
}
void LuxGui::ResetLightGroups( void ) {
	if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
		ResetLightGroupsFromFilm( true );
		return;
	}

	// Remove the lightgroups
	for( std::vector<LuxLightGroupPanel*>::iterator it = m_LightGroupPanels.begin(); it != m_LightGroupPanels.end(); it++) {
		LuxLightGroupPanel *currPanel = *it;
		m_LightGroupsSizer->Detach(currPanel);
		delete currPanel;
	}
	m_LightGroupPanels.clear();

	// Update
	m_LightGroups->Layout();
}
void LuxGui::ResetLightGroupsFromFilm( bool useDefaults ) {
	// Remove the old lightgroups
	for( std::vector<LuxLightGroupPanel*>::iterator it = m_LightGroupPanels.begin(); it != m_LightGroupPanels.end(); it++) {
		LuxLightGroupPanel *currPanel = *it;
		m_LightGroupsSizer->Detach(currPanel);
		delete currPanel;
	}
	m_LightGroupPanels.clear();

	// Add the new lightgroups
	int numLightGroups = (int)luxGetParameterValue(LUX_FILM, LUX_FILM_LG_COUNT);
	for(int i = 0; i < numLightGroups; i++) {
		LuxLightGroupPanel *currPanel = new LuxLightGroupPanel(
			this, m_LightGroups
		);
		currPanel->SetIndex(i);
		currPanel->ResetValuesFromFilm( useDefaults );
		m_LightGroupsSizer->Add(currPanel, 0, wxEXPAND | wxALL, 1);
		m_LightGroupPanels.push_back(currPanel);
	}

	// Update
	UpdateLightGroupWidgetValues();
	m_LightGroups->Layout();
}

int LuxGui::ValueToLogSliderVal(float value, const float logLowerBound, const float logUpperBound) {

	if (value <= 0)
		return 0;

	float logvalue = Clamp<float>(log10f(value), logLowerBound, logUpperBound);

	const int val = static_cast<int>((logvalue - logLowerBound) / 
		(logUpperBound - logLowerBound) * FLOAT_SLIDER_RES);
	return val;
}

float LuxGui::LogSliderValToValue(int sliderval, const float logLowerBound, const float logUpperBound) {

	float logvalue = (float)sliderval * (logUpperBound - logLowerBound) / 
		FLOAT_SLIDER_RES + logLowerBound;

	return powf(10.f, logvalue);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// LuxLightGroupPanel

LuxGui::LuxLightGroupPanel::LuxLightGroupPanel(
	LuxGui *gui,
	wxWindow* parent, 
	wxWindowID id, 
	const wxPoint& pos, 
	const wxSize& size, 
	long style )
	: LightGroupPanel(parent, id, pos, size, style), m_Gui(gui)
{
	m_lightgroupBitmap->SetBitmap(wxMEMORY_BITMAP(tab_lightgroup_png));
	m_Tab_LightGroupIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
	m_Tab_LightGroupToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
	m_BarBlackBodyStaticBitmap->SetBitmap(wxMEMORY_BITMAP(bar_blackbody_png));
	m_Index = -1;
	ResetValues();
}

int LuxGui::LuxLightGroupPanel::GetIndex() const {
	return m_Index;
}

void LuxGui::LuxLightGroupPanel::SetIndex( int index ) {
	m_Index = index;
	if(index > 2){
		m_Tab_Control_LightGroupPanel->Hide();
		m_Tab_LightGroupIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
	}
}

void LuxGui::LuxLightGroupPanel::UpdateWidgetValues() {
	if(m_LG_enable) m_Tab_LightGroupToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
	else m_Tab_LightGroupToggleIcon->SetBitmap(wxMEMORY_BITMAP(powerofficon_png));
	SetWidgetsEnabled(m_LG_enable);
	wxString st;
	m_LG_scaleSlider->SetValue(ScaleToSliderVal(m_LG_scale));
	st = wxString::Format(_("%.04f"), m_LG_scale);
	m_LG_scaleText->SetValue(st);
	float val = ((m_LG_temperature - LG_TEMPERATURE_MIN) 
			/ (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) * FLOAT_SLIDER_RES;
	m_LG_temperatureSlider->SetValue(val);
	st = wxString::Format(_("%.02f"), m_LG_temperature);
	m_LG_temperatureText->SetValue(st);
	wxColour colour(Clamp(int(m_LG_scaleRed * 255.0), 0, 255),
					Clamp(int(m_LG_scaleGreen * 255.0), 0, 255),
					Clamp(int(m_LG_scaleBlue * 255.0), 0, 255));
	m_LG_rgbPicker->SetColour(colour);
}
void LuxGui::LuxLightGroupPanel::ResetValues() {
	m_LG_enable = true;
	m_LG_scale = 1.f;
	m_LG_temperature_enabled = false;
	m_LG_temperature = 6500.f;
	m_LG_rgb_enabled = false;
	m_LG_scaleRed = 1.f;
	m_LG_scaleGreen = 1.f;
	m_LG_scaleBlue = 1.f;
	m_LG_scaleX = 1.f;
	m_LG_scaleY = 1.f;
}
void LuxGui::LuxLightGroupPanel::ResetValuesFromFilm( bool useDefaults ) {
	char tmpStr[256];
	luxGetStringParameterValue(LUX_FILM, LUX_FILM_LG_NAME, &tmpStr[0], 256, m_Index);
	m_LG_name->SetLabel(wxString::FromAscii(tmpStr));
	m_LG_enable = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_ENABLE, m_Index) != 0.f;
	m_LG_scale = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE, m_Index);
	double t = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_TEMPERATURE, m_Index);
	m_LG_temperature_enabled = t != 0.0;
	m_LG_temperature = m_LG_temperature_enabled ? t : m_LG_temperature;
	double r = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_RED, m_Index);
	double g = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_GREEN, m_Index);
	double b = RetrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_BLUE, m_Index);
	m_LG_rgb_enabled = (r != 1.0) && (g != 1.0) && (b != 1.0);
	if (m_LG_rgb_enabled) {
		m_LG_scaleRed = r;
		m_LG_scaleGreen = g;
		m_LG_scaleBlue = b;
	}

	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_ENABLE, m_LG_enable, m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE, m_LG_scale, m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);
}


void LuxGui::LuxLightGroupPanel::OnMouse(wxMouseEvent &event) {
	switch (event.GetId()) {
		// TABS Hide/Show(collapse)
		case ID_TAB_LG:
			{
				if( m_Tab_Control_LightGroupPanel->IsShown() ){
					m_Tab_Control_LightGroupPanel->Hide();
					m_Tab_LightGroupIcon->SetBitmap(wxMEMORY_BITMAP(arrowleft_png));
				}else{
					m_Tab_Control_LightGroupPanel->Show(true);
					m_Tab_LightGroupIcon->SetBitmap(wxMEMORY_BITMAP(arrowdownactive_png));
				}
				m_LG_MainSizer->Layout();
				m_Gui->m_LightGroupsSizer->Layout();
				m_Gui->m_LightGroups->GetSizer()->FitInside(m_Gui->m_LightGroups);
				m_Gui->m_LightGroups->Layout();
				Refresh();
				m_Gui->Refresh();
			}
			break;
		case ID_TAB_LG_TOGGLE:
			{
				if( m_LG_enable ){
					m_LG_enable = false;
					m_Tab_LightGroupToggleIcon->SetBitmap(wxMEMORY_BITMAP(powerofficon_png));
				}else{
					m_LG_enable = true;
					m_Tab_LightGroupToggleIcon->SetBitmap(wxMEMORY_BITMAP(powericon_png));
				}
				luxSetParameterValue(LUX_FILM, LUX_FILM_LG_ENABLE, m_LG_enable, m_Index);
				SetWidgetsEnabled(m_LG_enable);
				m_Gui->UpdatedTonemapParam();
				Refresh();
			}
			break;
		default:
			break;
	}
}

void LuxGui::LuxLightGroupPanel::OnText(wxCommandEvent& event) {
	if ( event.GetEventType() != wxEVT_COMMAND_TEXT_ENTER ) return;

	switch(event.GetId()) {
		case ID_LG_SCALE_TEXT:
			if (m_LG_scaleText->IsModified()) {
				wxString st = m_LG_scaleText->GetValue();
				st.ToDouble(&m_LG_scale);
				if (m_LG_scale > powf(10.f, LG_SCALE_LOG_MAX))
					m_LG_scale = powf(10.f, LG_SCALE_LOG_MAX);
				else if (m_LG_scale < 0.f)
					m_LG_scale = 0.f;
				st = wxString::Format(_("%.04f"), m_LG_scale);
				m_LG_scaleText->SetValue(st);
				m_LG_scaleSlider->SetValue(ScaleToSliderVal(m_LG_scale));
				UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE, m_LG_scale, m_Index);
				m_Gui->UpdatedTonemapParam();
			}
			break;
		case ID_LG_TEMPERATURE_TEXT:
			if (m_LG_temperatureText->IsModified()) {
				wxString st = m_LG_temperatureText->GetValue();
				st.ToDouble(&m_LG_temperature);
				if (m_LG_temperature > LG_TEMPERATURE_MAX)
					m_LG_temperature = LG_TEMPERATURE_MAX;
				else if (m_LG_temperature < LG_TEMPERATURE_MIN)
					m_LG_temperature = LG_TEMPERATURE_MIN;
				st = wxString::Format(_("%.02f"), m_LG_temperature);
				m_LG_temperatureText->SetValue(st);
				float val = ((m_LG_temperature - LG_TEMPERATURE_MIN) 
						/ (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) * FLOAT_SLIDER_RES;
				m_LG_temperatureSlider->SetValue(val);
				UpdateParam(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);
				if (m_LG_temperature_enabled)
					m_Gui->UpdatedTonemapParam();
			}
			break;
		default:
			break;
	}
}

void LuxGui::LuxLightGroupPanel::OnCheckBox(wxCommandEvent& event)
{
	if (event.GetEventType() != wxEVT_COMMAND_CHECKBOX_CLICKED)
		return;

	switch (event.GetId()) {
		case ID_LG_RGB_ENABLED:
			{
				if(m_LG_rgbEnabled->IsChecked())
					m_LG_rgb_enabled = true;
				else
					m_LG_rgb_enabled = false;

				UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
				UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
				UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);

				m_LG_rgbPicker->Enable(m_LG_rgb_enabled);

				m_Gui->UpdatedTonemapParam();
			}
			break;
		case ID_LG_TEMPERATURE_ENABLED:
			{
				if(m_LG_temperatureEnabled->IsChecked())
					m_LG_temperature_enabled = true;
				else
					m_LG_temperature_enabled = false;

				m_LG_temperatureSlider->Enable(m_LG_temperature_enabled);
				m_LG_temperatureText->Enable(m_LG_temperature_enabled);

				UpdateParam(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);
				m_Gui->UpdatedTonemapParam();
			}
			break;
/*
		case ID_LG_ENABLE:
			m_LG_enable = m_LG_enableCheckbox->GetValue();
			SetWidgetsEnabled(m_LG_enable);
			UpdateParam(LUX_FILM, LUX_FILM_LG_ENABLE, m_LG_enable, m_Index);
			m_Gui->UpdatedTonemapParam();
			break; */
		default:
			break;
	}
}

void LuxGui::LuxLightGroupPanel::OnColourChanged(wxColourPickerEvent &event)
{
	switch (event.GetId()) {
		case ID_LG_RGBCOLOR:
			m_LG_scaleRed = event.GetColour().Red() / 255.0;
			m_LG_scaleGreen = event.GetColour().Green() / 255.0;
			m_LG_scaleBlue = event.GetColour().Blue() / 255.0;
			UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
			UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
			UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);
			if (m_LG_rgb_enabled)
				m_Gui->UpdatedTonemapParam();
			break;
		default:
			break;
	}
}

void LuxGui::LuxLightGroupPanel::OnScroll(wxScrollEvent& event) {
	switch(event.GetId()) {
		case ID_LG_SCALE:
		{
			m_LG_scale = SliderValToScale(event.GetPosition());
			wxString st = wxString::Format(_("%.04f"), m_LG_scale);
			m_LG_scaleText->SetValue(st);
			UpdateParam(LUX_FILM, LUX_FILM_LG_SCALE, m_LG_scale, m_Index);
			m_Gui->UpdatedTonemapParam();
			break;
		}
		case ID_LG_TEMPERATURE:
		{
			m_LG_temperature = ((((float) event.GetPosition()) / FLOAT_SLIDER_RES) 
					* (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) + LG_TEMPERATURE_MIN;
			wxString st = wxString::Format(_("%.02f"), m_LG_temperature);
			m_LG_temperatureText->SetValue(st);
			UpdateParam(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);
			if (m_LG_temperature_enabled)
				m_Gui->UpdatedTonemapParam();
			break;
		}
		default:
			break;
	}
}

void LuxGui::LuxLightGroupPanel::SetWidgetsEnabled(bool enabled) {
	m_LG_scaleSlider->Enable(enabled);
	m_LG_scaleText->Enable(enabled);
	m_LG_rgbEnabled->Enable(enabled);
	m_LG_rgbPicker->Enable(enabled && m_LG_rgb_enabled);
	m_LG_temperatureEnabled->Enable(enabled);
	m_LG_temperatureSlider->Enable(enabled && m_LG_temperature_enabled);
	m_LG_temperatureText->Enable(enabled && m_LG_temperature_enabled);
}

int LuxGui::LuxLightGroupPanel::ScaleToSliderVal(float scale) {

	if (scale <= 0)
		return 0;

	float logscale = Clamp<float>(log10f(scale), LG_SCALE_LOG_MIN, LG_SCALE_LOG_MAX);

	const int val = static_cast<int>((logscale - LG_SCALE_LOG_MIN) / 
		(LG_SCALE_LOG_MAX - LG_SCALE_LOG_MIN) * FLOAT_SLIDER_RES);
	return val;
}

float LuxGui::LuxLightGroupPanel::SliderValToScale(int sliderval) {

	float logscale = (float)sliderval * (LG_SCALE_LOG_MAX - LG_SCALE_LOG_MIN) / 
		FLOAT_SLIDER_RES + LG_SCALE_LOG_MIN;

	return powf(10.f, logscale);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// ImageWindow

LuxGui::ImageWindow::ImageWindow(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name)
	: wxWindow(parent, id, pos, size, style, name)
{
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(LuxGui::ImageWindow::OnPaint));
	this->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(LuxGui::ImageWindow::OnEraseBackground));
	m_bitmap = NULL;
}

LuxGui::ImageWindow::~ImageWindow(){
	this->Disconnect(wxEVT_PAINT, wxPaintEventHandler(LuxGui::ImageWindow::OnPaint));
	this->Disconnect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(LuxGui::ImageWindow::OnEraseBackground));
	if(m_bitmap!=NULL) delete m_bitmap;
}
void LuxGui::ImageWindow::SetImage(const wxImage& img){
	if(m_bitmap!=NULL) delete m_bitmap;
	m_bitmap = new wxBitmap(img);
	Refresh();
}

void LuxGui::ImageWindow::OnPaint(wxPaintEvent& event){
	wxPaintDC dc(this);
	if(m_bitmap!=NULL) dc.DrawBitmap(*m_bitmap, 0, 0, false);
}

void LuxGui::ImageWindow::OnEraseBackground(wxEraseEvent& event){
	//empty handler to reduce redraw flicker
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// LuxHistogramWindow

LuxGui::LuxHistogramWindow::LuxHistogramWindow(wxWindow *parent, wxWindowID id, const wxPoint& pos)
	: ImageWindow(parent, id, pos, wxSize(200,100))
{
	this->Connect(wxEVT_SIZE, wxSizeEventHandler(LuxGui::LuxHistogramWindow::OnSize));
	wxImage tmp_img(200, 100, true);
	SetImage(tmp_img); //empty image
	m_Options=LUX_HISTOGRAM_RGB_ADD; //default mode
	m_IsEnabled=false;
}

LuxGui::LuxHistogramWindow::~LuxHistogramWindow(){
	this->Disconnect(wxEVT_SIZE, wxSizeEventHandler(LuxGui::LuxHistogramWindow::OnSize));
}

void LuxGui::LuxHistogramWindow::Update(){
	if(!IsShownOnScreen() || !m_IsEnabled) return;
	wxSize size=GetSize();
	wxImage img(size.GetWidth(), size.GetHeight(), true);
	if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) luxGetHistogramImage(img.GetData(), size.GetWidth(), size.GetHeight(), m_Options);
	SetImage(img);
}

void LuxGui::LuxHistogramWindow::SetOption(int option){
	switch(option){
		case LUX_HISTOGRAM_RGB:
		case LUX_HISTOGRAM_RGB_ADD:
		case LUX_HISTOGRAM_RED:
		case LUX_HISTOGRAM_GREEN:
		case LUX_HISTOGRAM_BLUE:
		case LUX_HISTOGRAM_VALUE:
		{
			m_Options &= ~(LUX_HISTOGRAM_RGB|LUX_HISTOGRAM_RGB_ADD|LUX_HISTOGRAM_RED|LUX_HISTOGRAM_GREEN|LUX_HISTOGRAM_BLUE|LUX_HISTOGRAM_VALUE);
			m_Options |= option;
		} break;
		case LUX_HISTOGRAM_LOG:
		{
			m_Options |=  LUX_HISTOGRAM_LOG;
		} break;
		default: break;
	}
}

void LuxGui::LuxHistogramWindow::SetEnabled(bool enabled){
	m_IsEnabled=enabled;
	UpdateParam(LUX_FILM, LUX_FILM_HISTOGRAM_ENABLED, enabled);
}

void LuxGui::LuxHistogramWindow::ClearOption(int option){
	m_Options &= ~option;
}

void LuxGui::LuxHistogramWindow::OnSize(wxSizeEvent& event){
	wxSize size=this->GetContainingSizer()->GetSize();
	SetSize(size);
	Update();
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Network panel...

void LuxGui::UpdateNetworkTree( void )
{
	m_serverUpdateSpin->SetValue( luxGetNetworkServerUpdateInterval() );
	wxTreeItemId idSelected = m_networkTreeCtrl->GetSelection();
	luxTreeData *pSelectedData = (luxTreeData *)m_networkTreeCtrl->GetItemData(idSelected);
	wxString sidSelected;
	if (pSelectedData)
		sidSelected = pSelectedData->m_SlaveID;

	m_networkTreeCtrl->DeleteAllItems();

	wxTreeItemId idRootNode = m_networkTreeCtrl->AddRoot( _("Master") );

	int nServers = luxGetServerCount();
	RenderingServerInfo *pInfoList = new RenderingServerInfo[nServers];
	nServers = luxGetRenderingServersStatus( pInfoList, nServers );

	wxString sTemp;
	wxTreeItemId idTempNode;

	double sampDiv = luxStatistics("filmXres") * luxStatistics("filmYres");

	for( int n1 = 0; n1 < nServers; n1++ )
	{
		luxTreeData *pTempData = new luxTreeData;

		pTempData->m_SlaveFile 	= m_CurrentFile;

		pTempData->m_SlaveName 	= wxString::FromUTF8(pInfoList[n1].name);
		pTempData->m_SlavePort 	= wxString::FromUTF8(pInfoList[n1].port);
		pTempData->m_SlaveID 	= wxString::FromUTF8(pInfoList[n1].sid);

		pTempData->m_secsSinceLastContact 		= pInfoList[n1].secsSinceLastContact;
		pTempData->m_numberOfSamplesReceived 	= pInfoList[n1].numberOfSamplesReceived;

		sTemp = wxString::Format( _("Slave: %s - Port: %s - ID: %s - Samples Per Pixel: %lf"),
									pTempData->m_SlaveName.c_str(),
									pTempData->m_SlavePort.c_str(),
									pTempData->m_SlaveID.c_str(),
									( pInfoList[n1].numberOfSamplesReceived / sampDiv ) );

		idTempNode = m_networkTreeCtrl->AppendItem( idRootNode, sTemp, -1, -1, pTempData );
		m_networkTreeCtrl->AppendItem( idTempNode, m_CurrentFile );
		if (pTempData->m_SlaveID == sidSelected)
			m_networkTreeCtrl->SelectItem(idTempNode);
 	}

	m_networkTreeCtrl->ExpandAll();
	delete[] pInfoList;
}

void LuxGui::OnTreeSelChanged( wxTreeEvent& event )
{
	wxTreeItemId idTreeNode = event.GetItem();

	luxTreeData *pNodeData = (luxTreeData *)m_networkTreeCtrl->GetItemData( idTreeNode );

	if ( pNodeData != NULL )
	{
		m_serverTextCtrl->SetValue( wxString::Format(_("%s:%s"), pNodeData->m_SlaveName.c_str(), pNodeData->m_SlavePort.c_str()));
	}
}

void LuxGui::AddServer( void )
{
	wxString sServer( m_serverTextCtrl->GetValue() );

	if ( !sServer.empty() )
	{
		luxAddServer( sServer.char_str() );

		UpdateNetworkTree();
	}
}

void LuxGui::RemoveServer( void )
{
	wxString sServer( m_serverTextCtrl->GetValue() );

	if ( !sServer.empty() )
	{
		luxRemoveServer( sServer.char_str() );

		UpdateNetworkTree();
	}
}


void LuxGui::OnSpin( wxSpinEvent& event )
{
    switch (event.GetId()) {
		case ID_SERVER_UPDATE_INT:
			{
				luxSetNetworkServerUpdateInterval( event.GetPosition() );
			}
		case ID_GLAREBLADES:
			{
				m_Glare_blades = m_glarebladesSpin->GetValue();

				if ( m_Glare_blades > GLARE_BLADES_MAX ) 
					m_Glare_blades = GLARE_BLADES_MAX;
				else if ( m_Glare_blades < GLARE_BLADES_MIN ) 
					m_Glare_blades = GLARE_BLADES_MIN;

				UpdateParam(LUX_FILM, LUX_FILM_GLARE_BLADES, m_Glare_blades);
			}
			break;			
		default:
			break;
	}
}

void LuxGui::OnSpinText(wxCommandEvent& event) {
    switch (event.GetId()) {
		case ID_GLAREBLADES:
			{
				m_Glare_blades = m_glarebladesSpin->GetValue();

				if ( m_Glare_blades > GLARE_BLADES_MAX ) 
					m_Glare_blades = GLARE_BLADES_MAX;
				else if ( m_Glare_blades < GLARE_BLADES_MIN ) 
					m_Glare_blades = GLARE_BLADES_MIN;

				UpdateParam(LUX_FILM, LUX_FILM_GLARE_BLADES, m_Glare_blades);
			}
			break;			
		default:
			break;
	}
}


// CF
//////////////////////////////////////////////////////////////////////////////////////////////////

bool LuxGui::CanStopRendering() {
	if(m_guiRenderState == RENDERING) {
		// Give warning that current rendering is not stopped
		if(wxMessageBox(wxT("Do you want to stop the current render and start a new one?"), wxT("Current file is still rendering"), wxYES_NO | wxICON_QUESTION, this) == wxNO)
			return false;
	}
	return true;
}


void LuxGui::StopRendering() {
	m_statusBar->SetStatusText(wxT(""), 0);
	// Clean up if this is not the first rendering
	if(m_guiRenderState != WAITING) {
		if(m_flmloadThread)
			m_flmloadThread->join();
		delete m_flmloadThread;
		m_flmloadThread = NULL;
		if(m_flmsaveThread)
			m_flmsaveThread->join();
		delete m_flmsaveThread;
		m_flmsaveThread = NULL;
		if(m_guiRenderState != FINISHED && m_guiRenderState != TONEMAPPING) {
			if(m_updateThread)
				m_updateThread->join();
			delete m_updateThread;
			m_updateThread = NULL;
			luxExit();
			if(m_engineThread)
				m_engineThread->join();
			delete m_engineThread;
			m_engineThread = NULL;
		}
		luxError(LUX_NOERROR, LUX_INFO, "Freeing resources.");
		luxCleanup();
		ChangeRenderState(WAITING);
		m_renderOutput->Reset();
	}
}

void LuxGui::OnOpen(wxCommandEvent& event) {
	if(!CanStopRendering())
		return;

	wxFileDialog filedlg(this,
	                     _("Choose a scene file to open"),
						 wxEmptyString,
						 wxEmptyString,
						 _("LuxRender scene files (*.lxs)|*.lxs|All files (*.*)|*.*"),
						 wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if(filedlg.ShowModal() == wxID_OK) {
		StopRendering();
		RenderScenefile(filedlg.GetPath());
	}
}

void LuxGui::OnResumeFLM(wxCommandEvent &event) {
	if(!CanStopRendering())
		return;

	wxFileDialog filedlg1(this,
	                     _("Choose a scene file to open"),
						 wxEmptyString,
						 wxEmptyString,
						 _("LuxRender scene files (*.lxs)|*.lxs|All files (*.*)|*.*"),
						 wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if(filedlg1.ShowModal() != wxID_OK)
		return;

	wxFileDialog filedlg2(this,
	                     _("Choose an FLM file to open"),
						 wxEmptyString,
						 wxEmptyString,
						 _("LuxRender FLM files (*.flm)|*.flm|All files (*.*)|*.*"),
						 wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if(filedlg2.ShowModal() != wxID_OK)
		return;

	StopRendering();
	RenderScenefile(filedlg1.GetPath(), filedlg2.GetPath());
}

void LuxGui::OnLoadFLM(wxCommandEvent &event) {
	if(!CanStopRendering())
		return;

	wxFileDialog filedlg(this,
	                     _("Choose an FLM file to open"),
						 wxEmptyString,
						 wxEmptyString,
						 _("LuxRender FLM files (*.flm)|*.flm|All files (*.*)|*.*"),
						 wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if(filedlg.ShowModal() == wxID_OK) {
		StopRendering();

		wxString filename = filedlg.GetPath();
		wxFileName fn(filename);
		SetTitle(wxT("LuxRender - ")+fn.GetName());

		// Start load thread
		m_progDialog = new wxProgressDialog(wxT("Loading FLM..."), wxT(""), 100, NULL, wxSTAY_ON_TOP);
		m_progDialog->Pulse();
		m_loadTimer->Start(1000, wxTIMER_CONTINUOUS);

		delete m_flmloadThread;
		m_flmloadThread = new boost::thread(boost::bind(&LuxGui::FlmLoadThread, this, filename));
	}
}
void LuxGui::OnSaveFLM(wxCommandEvent &event) {
	if( !luxStatistics("sceneIsReady") && !luxStatistics("filmIsReady") )
		return;
	if( m_guiRenderState == WAITING )
		return;

	wxFileDialog filedlg(this,
	                     _("Choose an FLM file to save to"),
						 wxEmptyString,
						 wxEmptyString,
						 _("LuxRender FLM files (*.flm)|*.flm|All files (*.*)|*.*"),
						 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

	if(filedlg.ShowModal() == wxID_OK) {
		wxString filename = filedlg.GetPath();
		// Start save thread
		m_progDialog = new wxProgressDialog(wxT("Saving FLM..."), wxT(""), 100, NULL, wxSTAY_ON_TOP);
		m_progDialog->Pulse();
		m_saveTimer->Start(1000, wxTIMER_CONTINUOUS);

		delete m_flmsaveThread;
		m_flmsaveThread = new boost::thread(boost::bind(&LuxGui::FlmSaveThread, this, filename));
	}
}

void LuxGui::OnExit(wxCloseEvent& event) {
	m_renderTimer->Stop();
	m_statsTimer->Stop();
	m_loadTimer->Stop();
	m_saveTimer->Stop();
	m_netTimer->Stop();

	//if we have a scene file
	if(m_guiRenderState != WAITING) {
		if(m_flmloadThread)
			m_flmloadThread->join();
		delete m_flmloadThread;
		m_flmloadThread = NULL;
		if(m_flmsaveThread)
			m_flmsaveThread->join();
		delete m_flmsaveThread;
		m_flmsaveThread = NULL;
		if(m_guiRenderState == PARSING && m_progDialog) {
			// destroy progress dialog if quitting during parsing
			m_progDialog->Destroy();
			delete m_progDialog;
			m_progDialog = NULL;
		}
		if(m_guiRenderState != FINISHED && m_guiRenderState != TONEMAPPING) {
			if(m_updateThread)
				m_updateThread->join();
			delete m_updateThread;
			m_updateThread = NULL;
			luxExit();
			if(m_engineThread)
				m_engineThread->join();
			delete m_engineThread;
			m_engineThread = NULL;
		}
		luxError(LUX_NOERROR, LUX_INFO, "Freeing resources.");
		luxCleanup();
	}

	Destroy();
}

void LuxGui::OnError(wxLuxErrorEvent &event) {
	static const wxColour debugColour = wxColour(0, 0, 0);
	static const wxColour infoColour = wxColour(0, 255, 0);
	static const wxColour warningColour = wxColour(255, 128, 0);
	static const wxColour errorColour = wxColour(255, 0, 0);
	static const wxColour severeColour = wxColour(255, 0, 0);

	std::stringstream ss("");
	ss << '[' << boost::posix_time::second_clock::local_time() << ' ';
	int severity = event.GetError()->GetSeverity();
	switch (severity) {
		case LUX_DEBUG:
			ss << "Debug: ";
			m_logTextCtrl->SetDefaultStyle(debugColour);
			break;
		case LUX_INFO:
			ss << "Info: ";
			m_logTextCtrl->SetDefaultStyle(infoColour);
			break;
		case LUX_WARNING:
			ss << "Warning: ";
			m_logTextCtrl->SetDefaultStyle(warningColour);
			break;
		case LUX_ERROR:
			ss << "Error: ";
			m_logTextCtrl->SetDefaultStyle(errorColour);
			break;
		case LUX_SEVERE:
			ss << "Severe error: ";
			m_logTextCtrl->SetDefaultStyle(severeColour);
			break;
		default:
			m_logTextCtrl->SetDefaultStyle(infoColour);
	}
	ss << event.GetError()->GetCode() << "] ";

	m_logTextCtrl->AppendText(wxString::FromAscii(ss.str().c_str()));

	m_logTextCtrl->SetDefaultStyle(*wxBLACK);
	ss.str("");
	ss << event.GetError()->GetMessage() << std::endl;
	m_logTextCtrl->AppendText(wxString::FromAscii(ss.str().c_str()));
	m_logTextCtrl->ShowPosition(m_logTextCtrl->GetLastPosition());

	// Feature request #584: Show a dialog in case of warning/error
	if (m_showWarningDialog && severity > LUX_INFO) {
		m_showWarningDialog = false;
		if (severity < LUX_SEVERE) {
			wxMessageBox(wxT("There was an abnormal condition reported. Please, check the Log tab for more information."),
				wxT("LuxRender Notification"),
				wxOK | (severity == LUX_WARNING ? wxICON_EXCLAMATION : wxICON_ERROR), this);

		} else {
			wxMessageBox(wxT("There was severe error reported. Please, check the Log tab for more information."),
				wxT("LuxRender Error"),
				wxOK | wxICON_STOP, this);
		}
	}
}

void LuxGui::OnTimer(wxTimerEvent& event) {
	switch (event.GetId()) {
		case ID_RENDERUPDATE:
			if(m_updateThread == NULL && (luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) &&
			    (m_guiWindowState == SHOWN || m_guiRenderState == FINISHED)) {
				luxError(LUX_NOERROR, LUX_INFO, "GUI: Updating framebuffer...");
				m_statusBar->SetStatusText(wxT("Tonemapping..."), 0);
				m_updateThread = new boost::thread(boost::bind(&LuxGui::UpdateThread, this));
			}
			break;
		case ID_STATSUPDATE:
			if(luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
				UpdateStatistics();
				if(m_guiRenderState == STOPPING && m_samplesSec == 0.0) {
					// Render threads stopped, do one last render update
					luxError(LUX_NOERROR, LUX_INFO, "GUI: Updating framebuffer...");
					m_statusBar->SetStatusText(wxT("Tonemapping..."), 0);
					delete m_updateThread;
					m_updateThread = new boost::thread(boost::bind(&LuxGui::UpdateThread, this));
					m_statsTimer->Stop();
					luxPause();
					luxError(LUX_NOERROR, LUX_INFO, "Rendering stopped by user.");
					ChangeRenderState(STOPPED);
				}
			}
			break;
		case ID_LOADUPDATE:
			if (!m_progDialog)
				break;
			m_progDialog->Pulse();
			if(luxStatistics("sceneIsReady") || m_guiRenderState == FINISHED) {
				m_progDialog->Destroy();
				delete m_progDialog;
				m_progDialog = NULL;
				m_loadTimer->Stop();

				if(luxStatistics("sceneIsReady")) {
					// Scene file loaded
					// Add other render threads if necessary
					int curThreads = 1;
					while(curThreads < m_numThreads) {
						luxAddThread();
						curThreads++;
					}

					// Start updating the display by faking a resume menu item click.
					wxCommandEvent startEvent(wxEVT_COMMAND_MENU_SELECTED, ID_RESUMEITEM);
					GetEventHandler()->AddPendingEvent(startEvent);

					// Enable tonemapping options and reset from values trough API
					m_outputNotebook->Enable( true );
					ResetToneMappingFromFilm( false );
					ResetLightGroupsFromFilm( false );
					Refresh();
				}
			} else if( luxStatistics("filmIsReady") ) {
				m_progDialog->Destroy();
				delete m_progDialog;
				m_progDialog = NULL;
				m_loadTimer->Stop();

				if(m_flmloadThread) {
					m_flmloadThread->join();
					delete m_flmloadThread;
					m_flmloadThread = NULL;
				}

				ChangeRenderState(TONEMAPPING);

				// Enable tonemapping options and reset from values trough API
				m_outputNotebook->Enable( true );
				ResetToneMappingFromFilm( false );
				ResetLightGroupsFromFilm( false );
				// Update framebuffer
				luxUpdateFramebuffer();
				m_renderOutput->Reload();
				// Update stats
				UpdateStatistics();
				// Refresh the GUI
				Refresh();
			}
			break;
		case ID_SAVEUPDATE:
			if (!m_progDialog)
				break;
			m_progDialog->Pulse();
			break;
		case ID_NETUPDATE:
			UpdateNetworkTree();
			break;
	}
}

void LuxGui::OnCommand(wxCommandEvent &event) {
	if(event.GetEventType() == wxEVT_LUX_TONEMAPPED) {
		// Make sure the update thread has ended so we can start another one later.
		if (m_updateThread)
			m_updateThread->join();
		delete m_updateThread;
		m_updateThread = NULL;
		m_statusBar->SetStatusText(wxT(""), 0);
		m_renderOutput->Reload();
		m_HistogramWindow->Update();

	} else if(event.GetEventType() == wxEVT_LUX_PARSEERROR) {
		m_progDialog->Destroy();
		delete m_progDialog;
		m_progDialog = NULL;
		m_loadTimer->Stop();

		ChangeRenderState(FINISHED);
	} else if(event.GetEventType() == wxEVT_LUX_FLMLOADERROR) {
		wxMessageBox(wxT("FLM load error.\nSee log for details."), wxT("Error"), wxOK | wxICON_ERROR, this);
		if(m_flmloadThread) {
			m_flmloadThread->join();
			delete m_flmloadThread;
			m_flmloadThread = NULL;
		}
		ChangeRenderState(WAITING);
	} else if(event.GetEventType() == wxEVT_LUX_FINISHED && m_guiRenderState == RENDERING) {
		// Ignoring finished events if another file is being opened (state != RENDERING)
		//wxMessageBox(wxT("Rendering is finished."), wxT("LuxRender"), wxOK | wxICON_INFORMATION, this);
		ChangeRenderState(FINISHED);
		// Stop timers and update output one last time.
		m_renderTimer->Stop();
#ifdef __WXOSX_COCOA__
		wxTimerEvent rendUpdEvent(*m_renderTimer);
		rendUpdEvent.SetId(ID_RENDERUPDATE);
#else
		wxTimerEvent rendUpdEvent(ID_RENDERUPDATE, GetId());
#endif
		GetEventHandler()->AddPendingEvent(rendUpdEvent);
		m_statsTimer->Stop();
#ifdef __WXOSX_COCOA__
		wxTimerEvent statUpdEvent(*m_renderTimer);
		rendUpdEvent.SetId(ID_RENDERUPDATE);
#else
		wxTimerEvent statUpdEvent(ID_STATSUPDATE, GetId());
#endif
		GetEventHandler()->AddPendingEvent(statUpdEvent);
	} else if( event.GetEventType() == wxEVT_LUX_SAVEDFLM ) {
		m_progDialog->Destroy();
		delete m_progDialog;
		m_progDialog = NULL;
		m_saveTimer->Stop();

		if (m_flmsaveThread)
			m_flmsaveThread->join();
		delete m_flmsaveThread;
		m_flmsaveThread = NULL;
	}
}

#if defined (__WXMSW__) ||  defined (__WXGTK__)
void LuxGui::OnIconize( wxIconizeEvent& event )
{
	if(!event.Iconized())
		m_guiWindowState = SHOWN;
	else
		m_guiWindowState = HIDDEN;
}
#endif

void LuxGui::RenderScenefile(wxString sceneFilename, wxString flmFilename) {
	// Get the absolute path of the flm file
	boost::filesystem::path fullPath(boost::filesystem::initial_path());
	fullPath = boost::filesystem::system_complete(boost::filesystem::path(flmFilename.fn_str(), boost::filesystem::native));

	// Set the FLM filename
	luxOverrideResumeFLM(fullPath.string().c_str());

	// Render the scene
	RenderScenefile(sceneFilename);
}

void LuxGui::RenderScenefile(wxString filename) {
	wxFileName fn(filename);
	SetTitle(wxT("LuxRender - ")+fn.GetName());

	// CF
	m_CurrentFile = filename;

	ChangeRenderState(PARSING);

	// NOTE - lordcrc - create progress dialog before starting engine thread
	//                  so we don't try to destroy it before it's properly created
	m_progDialog = new wxProgressDialog(wxT("Loading scene..."), wxT(""), 100, NULL, wxSTAY_ON_TOP);
	m_progDialog->Pulse();
	m_loadTimer->Start(1000, wxTIMER_CONTINUOUS);

	// Dade - reset flags for warning/error dialogs
	m_showWarningDialog = true;

	// Start main render thread
	delete m_engineThread;
	m_engineThread = new boost::thread(boost::bind(&LuxGui::EngineThread, this, filename));
}

void LuxGui::OnSelection(wxViewerEvent& event) {
	if(m_viewerToolBar->GetToolState(ID_ZOOMTOOL) == true) {
		// Zoom in and de-select anything
		m_renderOutput->SetZoom(event.GetSelection().get());
		m_renderOutput->SetSelection(NULL);
	} else if(m_viewerToolBar->GetToolState(ID_REFINETOOL) == true) {
		// Highlight current selection
		m_renderOutput->SetHighlight(event.GetSelection().get());
		m_renderOutput->SetSelection(NULL);
		// TODO: Pass selection to the core for actual refinement
	}
}

void LuxGui::EngineThread(wxString filename) {
	boost::filesystem::path fullPath(boost::filesystem::initial_path());
	fullPath = boost::filesystem::system_complete(boost::filesystem::path(filename.fn_str(), boost::filesystem::native));

	if (chdir(fullPath.branch_path().string().c_str()) == -1) {
		// Dade - something has gone wrong
		std::stringstream ss("");
		ss << "Unable to change directory to: '" <<
				fullPath.branch_path().string().c_str() << "'";
		luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
	}

	// NOTE - lordcrc - initialize rand()
	srand(time(NULL));

	// if stdin is input, don't use full path
	if (filename == wxString::FromAscii("-"))
		luxParse(filename.fn_str());
	else
		luxParse(fullPath.leaf().c_str());

	if (luxStatistics("terminated"))
		return;

	if(!luxStatistics("sceneIsReady")) {
		wxCommandEvent errorEvent(wxEVT_LUX_PARSEERROR, GetId());
		GetEventHandler()->AddPendingEvent(errorEvent);

		luxWait();
	} else {
		luxWait();

		luxError(LUX_NOERROR, LUX_INFO, "Rendering done.");
		wxCommandEvent endEvent(wxEVT_LUX_FINISHED, GetId());
		GetEventHandler()->AddPendingEvent(endEvent);
	}
}

void LuxGui::UpdateThread() {
	luxUpdateFramebuffer();

	wxCommandEvent endEvent(wxEVT_LUX_TONEMAPPED, GetId());
	GetEventHandler()->AddPendingEvent(endEvent);
}

void LuxGui::FlmLoadThread(wxString filename) {
	luxLoadFLM(filename.fn_str());
	if(!luxStatistics("filmIsReady")) {
		wxCommandEvent errorEvent(wxEVT_LUX_FLMLOADERROR, GetId());
		GetEventHandler()->AddPendingEvent(errorEvent);
	}
}

void LuxGui::FlmSaveThread(wxString filename) {
	luxSaveFLM(filename.fn_str());
	wxCommandEvent savedEvent(wxEVT_LUX_SAVEDFLM, GetId());
	GetEventHandler()->AddPendingEvent(savedEvent);
}

void LuxGui::SetRenderThreads(int num) {
	if(luxStatistics("sceneIsReady")) {
		if(num > m_numThreads) {
			for(; num > m_numThreads; m_numThreads++)
				luxAddThread();
		} else {
			for(; num < m_numThreads; m_numThreads--)
				luxRemoveThread();
		}
	} else {
		m_numThreads = num;
	}

	m_ThreadText->SetLabel( wxString::Format( _("Threads: %02d  "), m_numThreads ) );
}

void LuxGui::UpdateStatistics() {
	m_samplesSec = luxStatistics("samplesSec");
	int samplesSec = Floor2Int(m_samplesSec);
	int samplesTotSec = Floor2Int(luxStatistics("samplesTotSec"));
	int secElapsed = Floor2Int(luxStatistics("secElapsed"));
	double samplesPx = luxStatistics("samplesPx");
	int efficiency = Floor2Int(luxStatistics("efficiency"));
	int EV = luxStatistics("filmEV");

	int secs = (secElapsed) % 60;
	int mins = (secElapsed / 60) % 60;
	int hours = (secElapsed / 3600);

	wxString stats;
	stats.Printf(wxT("%02d:%02d:%02d - %d S/s - %d TotS/s - %.2f S/px - %i%% eff - EV = %d"),
	             hours, mins, secs, samplesSec, samplesTotSec, samplesPx, efficiency, EV);
	m_statusBar->SetStatusText(stats, 1);
}

/*** LuxOutputWin ***/

BEGIN_EVENT_TABLE(LuxOutputWin, wxWindow)
#ifndef __WXOSX_COCOA__
    EVT_PAINT (LuxOutputWin::OnPaint)
#endif
END_EVENT_TABLE()

LuxOutputWin::LuxOutputWin(wxWindow *parent)
      : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, -1)), wxViewerBase() {
}

wxWindow* LuxOutputWin::GetWindow() {
	return this;
}

void LuxOutputWin::Reload() {
	Refresh();
}

void LuxOutputWin::OnDraw(wxDC &dc) {
	if (luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
		int w = luxStatistics("filmXres"), h = luxStatistics("filmYres");
		SetVirtualSize(w, h);
		SetScrollRate(1,1);
		unsigned char* fb = luxFramebuffer();
		dc.DrawBitmap(wxBitmap(wxImage(w, h, fb, true)), 0, 0, false);
	}
}

/*** LuxGuiErrorHandler wrapper ***/

void LuxGuiErrorHandler(int code, int severity, const char *msg) {
	// Dade - console print enabled by command line option
	if (copyLog2Console)
		luxErrorPrint(code, severity, msg);

	boost::shared_ptr<LuxError> error(new LuxError(code, severity, msg));
	wxLuxErrorEvent errorEvent(error, wxEVT_LUX_ERROR);
	wxTheApp->GetTopWindow()->GetEventHandler()->AddPendingEvent(errorEvent);
}