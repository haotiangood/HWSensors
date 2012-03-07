/*
 *  IT87x.cpp
 *  HWSensors
 *
 *  Based on code from Open Hardware Monitor project by Michael Möller (C) 2011
 *
 *  Created by mozo on 08/10/10.
 *  Copyright 2010 mozodojo. All rights reserved.
 *
 */

/*
 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1
 
 The contents of this file are subject to the Mozilla Public License Version
 1.1 (the "License"); you may not use this file except in compliance with
 the License. You may obtain a copy of the License at
 
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS" basis,
 WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 for the specific language governing rights and limitations under the License.
 
 The Original Code is the Open Hardware Monitor code.
 
 The Initial Developer of the Original Code is 
 Michael Möller <m.moeller@gmx.ch>.
 Portions created by the Initial Developer are Copyright (C) 2011
 the Initial Developer. All Rights Reserved.
 
 Contributor(s):
 
 Alternatively, the contents of this file may be used under the terms of
 either the GNU General Public License Version 2 or later (the "GPL"), or
 the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 in which case the provisions of the GPL or the LGPL are applicable instead
 of those above. If you wish to allow use of your version of this file only
 under the terms of either the GPL or the LGPL, and not to allow others to
 use your version of this file under the terms of the MPL, indicate your
 decision by deleting the provisions above and replace them with the notice
 and other provisions required by the GPL or the LGPL. If you do not delete
 the provisions above, a recipient may use your version of this file under
 the terms of any one of the MPL, the GPL or the LGPL.
 
 */

#include "ITEIT87x.h"

#include <architecture/i386/pio.h>

#include "FakeSMCDefinitions.h"

#define Debug FALSE

#define LogPrefix "IT87x: "
#define DebugLog(string, args...)	do { if (Debug) { IOLog (LogPrefix "[Debug] " string "\n", ## args); } } while(0)
#define WarningLog(string, args...) do { IOLog (LogPrefix "[Warning] " string "\n", ## args); } while(0)
#define InfoLog(string, args...)	do { IOLog (LogPrefix string "\n", ## args); } while(0)

#define super SuperIOMonitor
OSDefineMetaClassAndStructors(IT87x, SuperIOMonitor)

UInt8 IT87x::readByte(UInt8 reg)
{
	outb(address + ITE_ADDRESS_REGISTER_OFFSET, reg);
	
	UInt8 value = inb(address + ITE_DATA_REGISTER_OFFSET);	
	__unused UInt8 check = inb(address + ITE_DATA_REGISTER_OFFSET);
	
	return value;
}

void IT87x::writeByte(UInt8 reg, UInt8 value)
{
	outb(address + ITE_ADDRESS_REGISTER_OFFSET, reg);
	outb(address + ITE_DATA_REGISTER_OFFSET, value);
}

UInt8 IT87x::temperatureSensorsLimit()
{
    return 3;
}

UInt8 IT87x::voltageSensorsLimit()
{
    return 9;
}

UInt8 IT87x::tachometerSensorsLimit()
{
    return 5;
}

SInt32 IT87x::readTemperature(UInt32 index)
{
	return readByte(ITE_TEMPERATURE_BASE_REG + index);
}

float IT87x::readVoltage(UInt32 index)
{
    return (float)(readByte(ITE_VOLTAGE_BASE_REG + index) * voltageGain * voltageSpecificGain[index]) / 1000.0f;
}

SInt32 IT87x::readTachometer(UInt32 index)
{
    long value;
    
    if (has16bitFanCounter)
    {
        value = readByte(ITE_FAN_TACHOMETER_REG[index]);
        
        value |= readByte(ITE_FAN_TACHOMETER_EXT_REG[index]) << 8;
        
        return value > 0x3f && value < 0xffff ? (float)(1350000 + value) / (float)(value * 2) : 0;
    }
    else
    {
        value = readByte(ITE_FAN_TACHOMETER_REG[index]);
        
        int divisor = 2;
        
        if (index < 2) 
            divisor = 1 << ((readByte(ITE_FAN_TACHOMETER_DIVISOR_REGISTER) >> (3 * index)) & 0x7);
        
        return value > 0 && value < 0xff ? 1350000.0f / (float)(value * divisor) : 0; 
    }
}

void IT87x::enter()
{
	outb(registerPort, 0x87);
	outb(registerPort, 0x01);
	outb(registerPort, 0x55);
	outb(registerPort, 0x55);
}

void IT87x::exit()
{
	outb(registerPort, SUPERIO_CONFIGURATION_CONTROL_REGISTER);
	outb(valuePort, 0x02);
}

bool IT87x::probePort()
{	
	UInt16 id = listenPortWord(SUPERIO_CHIP_ID_REGISTER);
	
	if (id == 0 || id == 0xffff)
		return false;
	
	switch (id)
	{
		case IT8512F:
		case IT8712F:
		case IT8716F:
		case IT8718F:
		case IT8720F: 
		case IT8721F: 
		case IT8726F:
		case IT8728F:
		case IT8752F:
        case IT8772E:
			model = id; 
			break; 
		default:
			WarningLog("found unsupported chip ID=0x%x", id);
			return false;
	}
    
    selectLogicalDevice(ITE_ENVIRONMENT_CONTROLLER_LDN);
        
    IOSleep(50);
        
    if (!getLogicalDeviceAddress()) {
        WarningLog("can't get monitoring logical device address");
        return false;
    }
        
    IOSleep(50);
		
	UInt8 vendor = readByte(ITE_VENDOR_ID_REGISTER);
	
	if (vendor != ITE_VENDOR_ID) {
        WarningLog("invalid vendor ID=0x%x", vendor);
		return false;
    }
	
	if ((readByte(ITE_CONFIGURATION_REGISTER) & 0x10) == 0) {
        WarningLog("invalid configuration register value");
		return false;
    }
	
    if (id == IT8721F || id == IT8728F || id == IT8772E)
        voltageGain = 12;
    else
        voltageGain = 16;
    
    UInt8 version = readByte(ITE_VERSION_REGISTER) & 0x0F;
    
    has16bitFanCounter = !(id == IT8712F && version < 8);
    
    for (int i = 0; i < 9; i++)
        voltageSpecificGain[i] = 1;
	
	return true;
}

UInt8 IT87x::getPortsCount()
{
    return 1;
}

const char *IT87x::getModelName()
{
	switch (model) 
	{
        case IT8512F: return "IT8512F";
        case IT8712F: return "IT8712F";
        case IT8716F: return "IT8716F";
        case IT8718F: return "IT8718F";
        case IT8720F: return "IT8720F";
        case IT8721F: return "IT8721F";
        case IT8726F: return "IT8726F";
		case IT8728F: return "IT8728F";
        case IT8752F: return "IT8752F";
        case IT8772E: return "IT8772E";
	}
	
	return "unknown";
}

const char *IT87x::getVendorName()
{
    return "ITE";
}