/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

#include "StdAfx.h"

#include "oEvent.h"
#include "TabAuto.h"
#include "meos_util.h"
#include "gdiconstants.h"
#include "MeosSQL.h"
#include <cstdint>
#include <mutex>
#include <thread>
#include <chrono>

MySQLReconnect::MySQLReconnect(const wstring &errorIn) : AutoMachine("MySQL-service", Machines::mMySQLReconnect), error(errorIn) {
  timeError = getLocalTime();
  // hThread (std::thread) is default-constructed (not joinable)
}

MySQLReconnect::MySQLReconnect(const MySQLReconnect &other)
  : AutoMachine(other), error(other.error), timeError(other.timeError),
    timeReconnect(other.timeReconnect), toRemove(other.toRemove) {
  // hThread is not copied; default-constructed (not joinable)
}

MySQLReconnect::~MySQLReconnect() {
  if (hThread.joinable())
    hThread.join();
}


static std::mutex CS_MySQL;
static volatile uint32_t mysqlConnecting=0;
static volatile uint32_t mysqlStatus=0;

void initMySQLCriticalSection(bool init) {
  // std::mutex is RAII; no explicit initialization or destruction needed
  (void)init;
}

bool isThreadReconnecting()
{
  std::lock_guard<std::mutex> lock(CS_MySQL);
  return mysqlConnecting != 0;
}

static void reconnectThread(oEvent *oe) {
  {
    std::lock_guard<std::mutex> lock(CS_MySQL);
    mysqlConnecting=1;
    mysqlStatus=0;
  }
  bool res = oe->reConnectRaw();

  {
    std::lock_guard<std::mutex> lock(CS_MySQL);
    if (res)
      mysqlStatus=1;
    else
      mysqlStatus=-1;

    mysqlConnecting=0;
  }
}

void MySQLReconnect::settings(gdioutput &gdi, oEvent &oe, State state) {
}

void MySQLReconnect::process(gdioutput &gdi, oEvent *oe, AutoSyncType ast)
{
  if (isThreadReconnecting())
    return;

  if (mysqlStatus==1) {
    if (hThread.joinable())
      hThread.join();
    mysqlStatus=0;
    string err;
    if (!oe->reConnect(err)) {
      gdi.addInfoBox("", L"warning:dbproblem#" + gdi.widen(err), L"Databasvarning", BoxStyle::HeaderWarning, 9000);
      interval = 10;
    }
    else {
      gdi.addInfoBox("", L"Återansluten mot databasen, tävlingen synkroniserad.", L"", BoxStyle::Header, 10000);
      timeReconnect = getLocalTime();
      gdi.setDBErrorState(false);
      gdi.setWindowTitle(oe->getTitleName());
      interval=0;
      toRemove = true;
    }
  }
  else if (mysqlStatus==-1) {
    if (hThread.joinable())
      hThread.join();
    mysqlStatus=0;
    interval = 10;//Wait ten seconds for next attempt

    gdi.setDBErrorState(true);
    string err;
    if (oe->hasDBConnection()) {
      oe->sqlConnection->getErrorMessage(err);
    }
    return;
  }
  else {
    mysqlConnecting = 1;
    hThread = std::thread(reconnectThread, oe);
    interval = 1;
  }
}

int AutomaticCB(gdioutput *gdi, GuiEventType type, BaseInfo* data);

void MySQLReconnect::status(gdioutput &gdi) {
  AutoMachine::status(gdi);
  if (interval>0){
    gdi.addStringUT(1, timeError + L": " + lang.tl("DATABASE ERROR")).setColor(colorDarkRed);
    gdi.fillRight();
    gdi.addString("", 0, "Nästa försök:");
    gdi.addTimer(gdi.getCY(),  gdi.getCX()+10, timerCanBeNegative, int((meos_steady_clock_ms()-timeout)/1000));
  }
  else {
    gdi.addStringUT(0, timeError + L": " + lang.tl("DATABASE ERROR")).setColor(colorDarkGrey);
    gdi.fillRight();
    gdi.addStringUT(0, timeReconnect + L":");
    gdi.addString("", 1, "Återansluten mot databasen, tävlingen synkroniserad.").setColor(colorDarkGreen);
    gdi.dropLine();
    gdi.fillDown();
    gdi.popX();
  }
  gdi.popX();
  gdi.fillDown();
  gdi.dropLine();

  gdi.popX();
  gdi.dropLine(0.3);
  gdi.addButton("Stop", "Stoppa automaten", AutomaticCB).setExtra(getId());
}
