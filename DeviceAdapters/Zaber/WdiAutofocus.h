#pragma once
///////////////////////////////////////////////////////////////////////////////
// FILE:          WdiAutofocus.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Device adapter for WDI autofocus.
//
// AUTHOR:        Martin Zak (contact@zaber.com)

// COPYRIGHT:     Zaber Technologies, 2024

// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

#include "Zaber.h"

extern const char* g_WdiAutofocusName;
extern const char* g_WdiAutofocusDescription;

namespace WdiAutofocus_ {
	const double xLdaNativePerMm = 1000000.0;
}

class WdiAutofocus : public CAutoFocusBase<WdiAutofocus>, public ZaberBase
{
public:
	WdiAutofocus();
	~WdiAutofocus();

	// Device API
	// ----------
	int Initialize();
	int Shutdown();
	void GetName(char* name) const;
	bool Busy();


	// AutoFocus API
	// ----------
	virtual bool IsContinuousFocusLocked();
	virtual int FullFocus();
	virtual int IncrementalFocus();
	virtual int GetLastFocusScore(double& score);
	virtual int GetCurrentFocusScore(double& score);
	virtual int GetOffset(double& offset);
	virtual int SetOffset(double offset);;
	virtual int AutoSetParameters();
	virtual int SetContinuousFocusing(bool state);
	virtual int GetContinuousFocusing(bool& state);

	// ZaverBase class overrides
	// ----------------
	virtual void onNewConnection();


	// Properties
	// ----------------
	int WdiHostGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int WdiPortGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int PortGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int FocusAddressGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int FocusAxisGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int ObjectiveTurretAddressGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int LimitMinGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);
	int LimitMaxGetSet(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	int ReadWdiPosition(float& position);

	long wdiPort_;
	std::string wdiHost_;

	long focusAddress_;
	long focusAxis_;
	long objectiveTurretAddress_;

	double limitMin_;
	double limitMax_;

	zmlmi::WdiAutofocusProvider provider_;
	zmlmi::Autofocus autofocus_;
};
