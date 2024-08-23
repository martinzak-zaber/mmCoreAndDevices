///////////////////////////////////////////////////////////////////////////////
// FILE:          WdiAutofocus.cpp
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

#ifdef WIN32
#pragma warning(disable: 4355)
#endif

#include "WdiAutofocus.h"

const char* g_WdiAutofocusName = "WdiAutofocus";
const char* g_WdiAutofocusDescription = "Zaber WDI Autofocus device adapter";

using namespace std;

WdiAutofocus::WdiAutofocus() :
	ZaberBase(this),
	focusAddress_(1),
	focusAxis_(1),
	objectiveTurretAddress_(-1),
	wdiHost_("Undefined"),
	wdiPort_(27),
	limitMin_(0.0),
	limitMax_(25.0)
{
	this->LogMessage("WdiAutofocus::WdiAutofocus\n", true);

	InitializeDefaultErrorMessages();
	ZaberBase::setErrorMessages([&](auto code, auto message) { this->SetErrorText(code, message); });

	// Pre-initialization properties
	CreateProperty(MM::g_Keyword_Name, g_WdiAutofocusName, MM::String, true);

	CreateProperty(MM::g_Keyword_Description, g_WdiAutofocusDescription, MM::String, true);


	CPropertyAction* pAct = new CPropertyAction(this, &WdiAutofocus::PortGetSet);
	CreateProperty("Zaber Serial Port", port_.c_str(), MM::String, false, pAct, true);

	pAct = new CPropertyAction(this, &WdiAutofocus::WdiHostGetSet);
	CreateProperty("WDI Hostname/IP", wdiHost_.c_str(), MM::String, false, pAct, true);

	pAct = new CPropertyAction(this, &WdiAutofocus::WdiPortGetSet);
	CreateIntegerProperty("WDI Port", wdiPort_, false, pAct, true);

	pAct = new CPropertyAction(this, &WdiAutofocus::FocusAddressGetSet);
	CreateIntegerProperty("Focus Stage Device Number", focusAddress_, false, pAct, true);
	SetPropertyLimits("Focus Stage Device Number", 1, 99);

	pAct = new CPropertyAction(this, &WdiAutofocus::FocusAxisGetSet);
	CreateIntegerProperty("Focus Stage Axis Number", focusAxis_, false, pAct, true);
	SetPropertyLimits("Focus Stage Axis Number", 1, 99);

	pAct = new CPropertyAction(this, &WdiAutofocus::ObjectiveTurretAddressGetSet);
	CreateIntegerProperty("Objective Turret Device Number", objectiveTurretAddress_, false, pAct, true);
	SetPropertyLimits("Focus Stage Axis Number", -1, 99);
}


WdiAutofocus::~WdiAutofocus()
{
	this->LogMessage("WdiAutofocus::~WdiAutofocus\n", true);
	Shutdown();
}


///////////////////////////////////////////////////////////////////////////////
// Stage & Device API methods
///////////////////////////////////////////////////////////////////////////////

void WdiAutofocus::GetName(char* name) const
{
	CDeviceUtils::CopyLimitedString(name, g_WdiAutofocusDescription);
}


int WdiAutofocus::Initialize()
{
	if (initialized_)
	{
		return DEVICE_OK;
	}

	core_ = GetCoreCallback();

	this->LogMessage("WdiAutofocus::Initialize\n", true);

	auto ret = handleException([=]() {
		ensureConnected();
		autofocus_.getStatus();

		auto settings = autofocus_.getFocusAxis().getSettings();
		limitMin_ = settings.get("motion.tracking.limit.min") / WdiAutofocus_::xLdaNativePerMm;
		limitMax_ = settings.get("motion.tracking.limit.max") / WdiAutofocus_::xLdaNativePerMm;
	 });
	if (ret != DEVICE_OK)
	{
		this->LogMessage("Attempt to connect to autofocus failed.\n", true);
		return ret;
	}

	auto pAct = new CPropertyAction(this, &WdiAutofocus::LimitMinGetSet);
	CreateFloatProperty("Limit Min [mm]", limitMin_, false, pAct);
	pAct = new CPropertyAction(this, &WdiAutofocus::LimitMaxGetSet);
	CreateFloatProperty("Limit Max [mm]", limitMax_, false, pAct);

	ret = UpdateStatus();
	if (ret != DEVICE_OK)
	{
		return ret;
	}

	if (ret == DEVICE_OK)
	{
		initialized_ = true;
		return DEVICE_OK;
	}
	else
	{
		return ret;
	}
}


int WdiAutofocus::Shutdown()
{
	this->LogMessage("WdiAutofocus::Shutdown\n", true);

	if (initialized_)
	{
		initialized_ = false;
	}

	return DEVICE_OK;
}


bool WdiAutofocus::Busy()
{
	this->LogMessage("WdiAutofocus::Busy\n", true);

	bool busy = false;
	auto ret = handleException([&]() {
		ensureConnected();
		busy = autofocus_.getFocusAxis().isBusy();
	});
	return ret == DEVICE_OK && busy;
}



///////////////////////////////////////////////////////////////////////////////
// Action handlers
// Handle changes and updates to property values.
///////////////////////////////////////////////////////////////////////////////

int WdiAutofocus::WdiHostGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	ostringstream os;
	os << "WdiAutofocus::WdiHostGetSet(" << pProp << ", " << eAct << ")\n";
	this->LogMessage(os.str().c_str(), false);

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(wdiHost_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(wdiHost_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::WdiPortGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	ostringstream os;
	os << "WdiAutofocus::WdiPortGetSet(" << pProp << ", " << eAct << ")\n";
	this->LogMessage(os.str().c_str(), false);

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(wdiPort_);
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(wdiPort_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::PortGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	ostringstream os;
	os << "WdiAutofocus::PortGetSet(" << pProp << ", " << eAct << ")\n";
	this->LogMessage(os.str().c_str(), false);

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::FocusAddressGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	this->LogMessage("WdiAutofocus::FocusAddressGetSet\n", true);

	if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(focusAddress_);
	}
	else if (eAct == MM::BeforeGet)
	{
		pProp->Set(focusAddress_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::FocusAxisGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	this->LogMessage("WdiAutofocus::FocusAxisGetSet\n", true);

	if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(focusAxis_);
	}
	else if (eAct == MM::BeforeGet)
	{
		pProp->Set(focusAxis_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::ObjectiveTurretAddressGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	this->LogMessage("WdiAutofocus::ObjectiveTurretAddressGetSet\n", true);

	if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			resetConnection();
		}

		pProp->Get(objectiveTurretAddress_);
	}
	else if (eAct == MM::BeforeGet)
	{
		pProp->Set(objectiveTurretAddress_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::LimitMinGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	this->LogMessage("ObjectiveChanger::LimitMinGetSet\n", true);

	if (eAct == MM::AfterSet)
	{
		double newLimit;
		pProp->Get(newLimit);
		auto update = limitMin_ != newLimit;
		limitMin_ = newLimit;
		if (update) {
			return handleException([=]() {
				ensureConnected();
				autofocus_.setLimitMin(limitMin_ * WdiAutofocus_::xLdaNativePerMm);
			});
		}
	}
	else if (eAct == MM::BeforeGet)
	{
		pProp->Set(limitMin_);
	}

	return DEVICE_OK;
}

int WdiAutofocus::LimitMaxGetSet(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	this->LogMessage("ObjectiveChanger::LimitMaxGetSet\n", true);

	if (eAct == MM::AfterSet)
	{
		double newLimit;
		pProp->Get(newLimit);
		auto update = limitMax_ != newLimit;
		limitMax_ = newLimit;
		if (update) {
			return handleException([=]() {
				ensureConnected();
				autofocus_.setLimitMax(limitMax_ * WdiAutofocus_::xLdaNativePerMm);
			});
		}
	}
	else if (eAct == MM::BeforeGet)
	{
		pProp->Set(limitMax_);
	}

	return DEVICE_OK;
}

void WdiAutofocus::onNewConnection() {
	ZaberBase::onNewConnection();
	this->LogMessage("WdiAutofocus::onNewConnection\n", true);

	provider_ = zmlmi::WdiAutofocusProvider::openTcp(wdiHost_, wdiPort_);

	auto focusAxis = connection_->getDevice(focusAddress_).getAxis(focusAxis_);
	auto turretDevice = objectiveTurretAddress_ > 0 ? std::optional<zml::Device>(connection_->getDevice(objectiveTurretAddress_)) : std::nullopt;
	autofocus_ = zmlmi::Autofocus(provider_.getProviderId(), focusAxis, turretDevice);
}

int WdiAutofocus::FullFocus() {
	this->LogMessage("WdiAutofocus::FullFocus\n", true);

	return handleException([=]() {
		ensureConnected();

		autofocus_.focusOnce(true);
	});
}

int WdiAutofocus::IncrementalFocus() {
	this->LogMessage("WdiAutofocus::IncrementalFocus\n", true);

	return handleException([=]() {
		ensureConnected();

		autofocus_.focusOnce();
	});
}

int WdiAutofocus::GetLastFocusScore(double& score) {
	this->LogMessage("WdiAutofocus::GetLastFocusScore\n", true);
	return GetCurrentFocusScore(score);
}

int WdiAutofocus::GetCurrentFocusScore(double& score) {
	this->LogMessage("WdiAutofocus::GetCurrentFocusScore\n", true);
	float position;
	auto ret = ReadWdiPosition(position);
	score = abs(position);
	return ret;
}

int WdiAutofocus::GetOffset(double& offset) {
	this->LogMessage("WdiAutofocus::GetOffset\n", true);
	offset = 0.0;
	return DEVICE_OK;
}
int WdiAutofocus::SetOffset(double offset) {
	this->LogMessage("WdiAutofocus::SetOffset\n", true);
	return DEVICE_OK;
}

int WdiAutofocus::AutoSetParameters() {
	this->LogMessage("WdiAutofocus::AutoSetParameters\n", true);
	return DEVICE_OK;
}

int WdiAutofocus::SetContinuousFocusing(bool state) {
	this->LogMessage("WdiAutofocus::SetContinuousFocusing\n", true);

	return handleException([=]() {
		ensureConnected();

		if (state) {
			autofocus_.startFocusLoop();
		} else {
			autofocus_.stopFocusLoop();
		}
	});
}

int WdiAutofocus::GetContinuousFocusing(bool& state) {
	this->LogMessage("WdiAutofocus::GetContinuousFocusing\n", true);
	return handleException([&]() {
		ensureConnected();

		state = autofocus_.getFocusAxis().isBusy();
	});
}

bool WdiAutofocus::IsContinuousFocusLocked() {
	this->LogMessage("WdiAutofocus::IsContinuousFocusLocked\n", true);

	bool locked = false;
	auto ret = handleException([&]() {
		ensureConnected();
		auto status = autofocus_.getStatus();
		locked = status.getInFocus();
	});
	return ret == DEVICE_OK && locked;
}

int WdiAutofocus::ReadWdiPosition(float& position) {
	return handleException([&]() {
		ensureConnected();

		auto positionInt = provider_.genericRead(41, 4, 1, 0, "t");
		position = positionInt[0] / 1024.0f;
	});
}
