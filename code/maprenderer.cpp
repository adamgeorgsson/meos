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
#define _USE_MATH_DEFINES

#include "StdAfx.h"

#include <algorithm>
#include <string>

#include "gdioutput.h"
#include "meosexception.h"
#include "gdistructures.h"
#include "meos_util.h"
#include "localizer.h"
#include "gdifonts.h"
#include "oEvent.h"
#include "gdiconstants.h"
#include "image.h"
#include "maprenderer.h"
#include "xmlparser.h"
#include "csvparser.h"
#include "utm_interface.h"

using namespace std;
extern Image image;

void MapData::load(const xmlobject& data) {
  imageId = data.getObjectInt64u("mapid");
  top = data.getObjectDouble("top");
  bottom = data.getObjectDouble("bottom");
  left = data.getObjectDouble("left");
  right = data.getObjectDouble("right");

  latCenter = data.getObjectDouble("latc");
  lngCenter = data.getObjectDouble("lngc");

  string wdata;
  data.getObjectString("world", wdata);
  vector<string> wcoord;
  split(wdata, ",", wcoord);
  
  world.clear();
  for (auto& w : wcoord)
    world.push_back(atof(w.c_str()));
}

void MapData::save(xmlparser& data) const {
  data.startTag("Map");
  data.write64u("mapid", imageId);
  data.write("top", top);
  data.write("bottom", bottom);
  data.write("left", left);
  data.write("right", right);

  data.write("latc", latCenter);
  data.write("lngc", lngCenter);
  string wdata;
  for (double d : world) {
    if (!wdata.empty())
      wdata += ",";
    wdata += oDataContainer::formatDouble(d);
  }
  data.write("world", wdata);
  data.endTag();
}

void MapData::readWorld(const wstring& worldFile) {
  csvparser csv;
  list<vector<wstring>> out;
  csv.parse(worldFile, out);
  world.clear();
  if (out.size() >= 6) {
    for (auto& line : out) {
      if (line.size() == 1 && !line.begin()->empty()) {
        double val = _wtof(line[0].data());
        if (val != 0 || line.begin()->at(0) == '0')
          world.push_back(val);
      }
    }
  }
}

void MapData::setMapPos(double top, double left, double bottom, double right) {
  this->top = top;
  this->left = left;
  this->right = right;
  this->bottom = bottom;
}

void MapData::getDimensions(int& h, int& w) const {
  h = image.getHeight(imageId);
  w = image.getWidth(imageId);
}

void MapData::render(gdioutput& gdi, int xp, int yp) const {
  int h = image.getHeight(imageId);
  int w = image.getWidth(imageId);

  gdi.addImage("", xp, yp, 0, itow(imageId));
  gdi.refreshFast();
}

pair<int, int>  MapData::render(oEvent& oe, gdioutput& gdi, int xp, int yp, 
                                const vector<tuple<oControl*, wstring, RenderCType>>& ctrlList) const {
  shared_ptr<const MapData> md = shared_from_this();
  auto rdr = make_shared<MapDataRenderer>(gdi, xp, yp, md);

  oe.loadImage(imageId);
  int xmax = 0, xmin = numeric_limits<int>::max();
  int ymax = 0, ymin = numeric_limits<int>::max();
  bool any = false;
  for (auto &[ctrl, label, type] : ctrlList) {
    if (ctrl) {
      double xpos = ctrl->getDCI().getDouble("xpos");
      double ypos = ctrl->getDCI().getDouble("ypos");

      double lat = ctrl->getDCI().getDouble("latcrd");
      double lon = ctrl->getDCI().getDouble("longcrd");
      int xc, yc;
      if (mapCoordinate(lon, lat, xc, yc)) {
        any = true;
        rdr->addControl(type, xc, yc, label);
        xmin = min(xmin, xc);
        xmax = max(xmax, xc);

        ymin = min(ymin, yc);
        ymax = max(ymax, yc);
      }
    }
  }

  if (!any) {
    throw meosException("Kan inte lokalisera kontrollerna på kartan.");
  }

  int margin = metersToPixels(100);
  
  int wm = xmax - xmin + 2 * margin;
  int hm = ymax - ymin + 2 * margin;

  gdi.addImage("", yp, xp, 0, itow(imageId), gdi.scaleLength(wm), gdi.scaleLength(hm), xmin-margin, ymin-margin, wm, hm);
  RECT rc;
  rc.left = xp - 1;
  rc.top = yp - 1;
  rc.right = xp + gdi.scaleLength(wm) + 1;
  rc.bottom = yp + gdi.scaleLength(hm) + 1;
  gdi.addRectangle(rc, GDICOLOR::colorTransparent, true);

  rdr->setView(xmin - margin, xmax + margin, ymin - margin, ymax + margin);

  gdi.setMapRenderer(rdr);
  gdi.refreshFast();
  return make_pair(xp + gdi.scaleLength(wm), yp + gdi.scaleLength(hm));
}

void MapData::setImage(uint64_t imgId) {
  imageId = imgId;
}

pair<double, double> MapData::mapPoint(double x, double y) const {
  x = (x - left) / (right - left);
  y = (y - top) / (bottom - top);
  return make_pair(x, y);
}

pair<int, int> MapDataContainer::render(oEvent& oe, gdioutput& gdi, int xp, int yp, const vector<tuple<oControl*, wstring, RenderCType>> &ctrl) const {
  if (!maps.empty())
    return maps[0]->render(oe, gdi, xp, yp, ctrl);
  else
    return make_pair(-1, -1);
}

void MapDataContainer::add(shared_ptr<MapData>& newMap) {
  maps.push_back(newMap);
}

bool MapDataContainer::validCoordinates(const oControl& ctrl) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      if (map->validCoordinates(ctrl))
        return true;
  }
  return false;
}

bool MapDataContainer::getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      if (map->getCoordinatePosition(ctrl, dimx, dimy, xp, yp))
        return true;
  }
  return false;
}

void  MapDataContainer::getUsedImage(set<uint64_t>& img) const {
  for (auto& map : maps) {
    if (map->getImage() != 0)
      img.insert(map->getImage());
  }
}

void MapDataContainer::serialize(xmlparser& xml) const {
  if (!maps.empty()) {
    xml.startTag("Maps");
    for (auto& map : maps) {
      map->save(xml);
    }
    xml.endTag();
  }
}

string MapDataContainer::save() const {
  string out;
  xmlparser xml;
  xml.openMemoryOutput(true);
  
  serialize(xml);

  xml.getMemoryOutput(out);  
  return out;
}

void MapDataContainer::load(const string& raw) {
  xmlparser xml;
  xml.readMemory(raw, 0);
  deserialize(xml.getObject("Maps"));
}

bool MapDataContainer::deserialize(const xmlobject& xmaps) {
  maps.clear();
  if (xmaps) {
    xmlList mapLst;
    xmaps.getObjects("Map", mapLst);
    for (auto& m : mapLst) {
      auto md = make_shared<MapData>();
      try {
        md->load(m);
        maps.emplace_back(md);
      }
      catch (const std::exception& ) {
      }
    }
  }
  return !maps.empty();
}

void MapDataRenderer::renderDecoration(HDC hDC, gdioutput& gdi) const {
  //HPEN pen = CreatePen(PS_SOLID, scaleLength(2), RGB(0, 0, 0));
  SelectObject(hDC, hPen);
  SelectObject(hDC, GetStockObject(NULL_BRUSH));
  int h, w;
  data->getDimensions(h, w);
  int rad = gdi.scaleLength(data->metersToPixels(40));
  int lastCX = -1;
  int lastCY = -1;
  bool usedTop = false;

  TextInfo ti;
  ti.setColor(GDICOLOR(RGB(250, 50, 215)));
  ti.format = boldLarge;
  ti.font = L"Calibri";
  ti.text = L"222";
  gdi.calcStringSize(ti, hDC);
  
  gdi.formatString(ti, hDC);
  int rf = rad / 8;
  int texth = ti.getHeight();

  int lastcx = -1;
  int lastcy = -1;

  for (int j = 0; j < controls.size(); j++) {
    auto& c = controls[j];

    int cx = x + gdi.scaleLength(c.x - xmin) - gdi.getOffsetX();
    int cy = y + gdi.scaleLength(c.y - ymin) - gdi.getOffsetY();

    if (c.type == RenderCType::Finish) {
      Ellipse(hDC, cx - rad - rf, cy - rad - rf, cx + rad + rf, cy + rad + rf);
      Ellipse(hDC, cx - rad + rf, cy - rad + rf, cx + rad - rf, cy + rad - rf);
    }
    else if (c.type == RenderCType::Start) {
      double ang = 0;
      if (j + 1 < controls.size()) {
        double dx = controls[j + 1].x - c.x;
        double dy = controls[j + 1].y - c.y;
        if (dx != 0 || dy != 0)
          ang = atan2(dy, dx);
      }
      
      constexpr double d120 = 2.0 * M_PI / 3.0;
      int p1x = cx + int(cos(ang) * rad);
      int p1y = cy + int(sin(ang) * rad);

      int p2x = cx + int(cos(ang + d120) * rad);
      int p2y = cy + int(sin(ang + d120) * rad);

      int p3x = cx + int(cos(ang - d120) * rad);
      int p3y = cy + int(sin(ang - d120) * rad);

      MoveToEx(hDC, p1x, p1y, nullptr);
      LineTo(hDC, p2x, p2y);
      LineTo(hDC, p3x, p3y);
      LineTo(hDC, p1x, p1y);
    }
    else {
      Ellipse(hDC, cx - rad, cy - rad, cx + rad, cy + rad);
    }

    bool isCourse = true;
    if (c.type == RenderCType::CorrectControl || c.type == RenderCType::WrongControl) {
      isCourse = false;
      int cmin = 1000000, cmax = -10000000;
      for (auto& c2 : controls) {
        cmin = min(cmin, c2.y);
        cmax = min(cmax, c2.y);
      }

      if (c.y == cmin && !usedTop) {
        usedTop = true;
        TextOut(hDC, cx - rad, cy - rad - rf - texth, c.label.c_str(), c.label.size());
      }
      else {
        TextOut(hDC, cx - rad, cy + rad + rf, c.label.c_str(), c.label.size());
      }
    }
  
    if (lastcx != -1 && lastcy != -1) {
      double dx = cx - lastcx;
      double dy = cy - lastcy;
      double dist = sqrt(dx * dx + dy * dy);
      if (dist > 2*(rad + 2*rf) && isCourse) {
        int xoff = int(dx * (rad + 2*rf) / dist);
        int yoff = int(dy * (rad + 2*rf) / dist);

        MoveToEx(hDC, lastcx + xoff, lastcy + yoff, nullptr);
        LineTo(hDC, cx - xoff, cy - yoff);
      }
    }

    lastcx = cx;
    lastcy = cy;
  }

  SelectObject(hDC, GetStockObject(BLACK_PEN));
}

MapDataRenderer::MapDataRenderer(const gdioutput &gdi, int x, int y, const shared_ptr<const MapData>& src) :
 x(x), y(y), data(src) {
  int w = gdi.scaleLength(max(2, src->metersToPixels(3)));
  hPen = CreatePen(PS_SOLID, w, RGB(250, 50, 215));
}

MapDataRenderer::~MapDataRenderer() {
  DeleteObject(hPen);
}

void MapDataRenderer::setView(int xmin, int xmax, int ymin, int ymax) {
  this->xmax = xmax;
  this->xmin = xmin;

  this->ymax = ymax;
  this->ymin = ymin;
}

int MapData::metersToPixels(int meter) const {
  if (world.size() == 6) {
    int px = (2.0 * meter) / (std::abs(world[0]) + std::abs(world[3])); // XX
    return px;
  }
  else
    return meter;
}

bool MapData::mapCoordinate(double lng, double lat, int& x, int& y) const {
  x = -1, y = -1;

  double xx, yy;
  int zone = utm::LatLonToUTMXY(latCenter, lngCenter, 0, xx, yy);

  if (world.size() == 6) {
    const double &offX = world[4];
    const double &offY = world[5];

    int w, h;
    getDimensions(h, w);

    double oppX = offX + w * world[0];
    double oppY = offY + h * world[3];

    utm::LatLonToUTMXY(lat, lng, zone, xx, yy);
    
    double relX = (xx - offX) / (oppX - offX);   
    double relY = (yy - offY) / (oppY - offY);
   
/*    double E = world[3];
    double C = world[4];
    double F = world[5];
    double A = world[0];

    double xp = (xx * E - E * C) / A * E;
    double yp = (yy * A - F * A) / A * E;

    double relX = xp / w;
    double relY = yp / h;
  */  
    if (!(relX >= 0.0 && relX <= 1.0))
      return false;

    if (!(relY >= 0.0 && relY <= 1.0))
      return false;

    x = min(w, int(round(w * relX)));
    y = min(h, int(round(h * relY)));
    return true;
  }

  return false;
}

bool MapData::validCoordinates(const oControl& ctrl) const {
  int dimx, dimy, xp, yp;
  return getCoordinatePosition(ctrl, dimx, dimy, xp, yp);
}

bool MapData::getCoordinatePosition(const oControl& ctrl, int& dimx, int& dimy, int& xp, int& yp) const {
  double xpos = ctrl.getDCI().getDouble("xpos");
  double ypos = ctrl.getDCI().getDouble("ypos");
  double lat = ctrl.getDCI().getDouble("latcrd");
  double lon = ctrl.getDCI().getDouble("longcrd");

  getDimensions(dimy, dimx);

  if (mapCoordinate(lon, lat, xp, yp))
    return true;

  if (xpos == 0.0 && ypos == 0.0)
    return false; // Unsupported (default, unset) coordinate

  if (ypos <= top || ypos >= bottom)
    return false;

  if (xpos <= left || xpos >= right)
    return false;

  return true;
}
