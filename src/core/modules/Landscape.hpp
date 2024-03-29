/*
 * Stellarium
 * Copyright (C) 2003 Fabien Chereau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#ifndef _LANDSCAPE_HPP_
#define _LANDSCAPE_HPP_

#include "VecMath.hpp"
#include "StelToneReproducer.hpp"
#include "StelProjector.hpp"

#include "StelFader.hpp"
#include "StelUtils.hpp"
#include "StelTextureTypes.hpp"
#include "StelLocation.hpp"

#include <QMap>
#include <QImage>

class QSettings;
class StelLocation;
class StelCore;
class StelPainter;

//! @class Landscape
//! Store and manages the displaying of the Landscape.
//! Don't use this class directly, use the LandscapeMgr.
//! A landscape's most important element is a photo panorama.
//! Optional components include:
//!  - A fog texture that is displayed with the Fog [F] command.
//!  - A location. It is possible to auto-move to the location when loading.
//!  - Atmospheric conditions: temperature/pressure/extinction coefficients.
//!  - Light pollution information (Bortle index)
//!  - A night texture that gets blended over the dimmed daylight panorama. (Spherical and Fisheye only)
//!  - A polygonal horizon line (required for PolygonalLandscape). If present, defines a measured horizon line, which can be plotted or queried for rise/set predictions.
//!  - You can set a minimum brightness level to prevent too dark landscape. There is
//!    a global activation setting (config.ini[landscape]flag_minimal_brightness),
//!    a global value (config.ini[landscape]minimal_brightness),
//!    and, if config.ini[landscape]flag_landscape_sets_minimal_brightness=true,
//!    optional individual values given in landscape.ini[landscape]minimal_brightness are used.
//!
//! We discern:
//!   @param LandscapeId: The directory name of the landscape.
//!   @param name: The landscape name as specified in the LandscapeIni (may contain spaces, UTF8, ...)
class Landscape
{
public:
	Landscape(float _radius = 2.f);
	virtual ~Landscape();
	//! Load landscape.
	//! @param landscapeIni A reference to an existing QSettings object which describes the landscape
	//! @param landscapeId The name of the directory for the landscape files (e.g. "ocean")
	virtual void load(const QSettings& landscapeIni, const QString& landscapeId) = 0;
	virtual void draw(StelCore* core) = 0;
	void update(double deltaTime)
	{
		landFader.update((int)(deltaTime*1000));
		fogFader.update((int)(deltaTime*1000));
	}

	//! Set the brightness of the landscape plus brightness of optional add-on night lightscape.
	//! This is called in each draw().
	void setBrightness(const float b, const float pollutionBrightness=0.0f) {landscapeBrightness = b; lightScapeBrightness=pollutionBrightness; }

	//! Set whether landscape is displayed (does not concern fog)
	void setFlagShow(const bool b) {landFader=b;}
	//! Get whether landscape is displayed (does not concern fog)
	bool getFlagShow() const {return (bool)landFader;}
	//! Set whether fog is displayed
	void setFlagShowFog(const bool b) {fogFader=b;}
	//! Get whether fog is displayed
	bool getFlagShowFog() const {return (bool)fogFader;}
	//! Get landscape name
	QString getName() const {return name;}
	//! Get landscape author name
	QString getAuthorName() const {return author;}
	//! Get landscape description
	QString getDescription() const {return description;}

	//! Return the associated location (may be empty!)
	const StelLocation& getLocation() const {return location;}
	//! Return if the location is valid (a valid location has a valid planetName!)
	bool hasLocation() const {return (location.planetName.length() > 0);}
  	//! Return default Bortle index (light pollution value) or -1 (unknown/no change)
	int getDefaultBortleIndex() const {return defaultBortleIndex;}
	//! Return default fog setting (0/1) or -1 (no change)
	int getDefaultFogSetting() const {return defaultFogSetting;}
	//! Return default atmosperic extinction [mag/airmass], or -1 (no change)
	float getDefaultAtmosphericExtinction() const {return defaultExtinctionCoefficient;}
	//! Return configured atmospheric temperature [degrees Celsius], for refraction computation, or -1000 for "unknown/no change".
	float getDefaultAtmosphericTemperature() const {return defaultTemperature;}
	//! Return configured atmospheric pressure [mbar], for refraction computation.
	//! returns -1 to signal "standard conditions" [compute from altitude], or -2 for "unknown/invalid/no change"
	float getDefaultAtmosphericPressure() const {return defaultPressure;}
	//! Return minimal brightness for landscape
	//! returns -1 to signal "standard conditions" (use default value from config.ini)
	float getLandscapeMinimalBrightness() const {return minBrightness;}

	//! Set an additional z-axis (azimuth) rotation after landscape has been loaded.
	//! This is intended for special uses such as when the landscape consists of
	//! a vehicle which might change orientation over time (e.g. a ship). It is called
	//! e.g. by the LandscapeMgr. Contrary to that, the purpose of the azimuth rotation
	//! (landscape/[decor_]angle_rotatez) in landscape.ini is to orient the pano.
	//! @param d the rotation angle in degrees.
	void setZRotation(float d) {angleRotateZOffset = d * M_PI/180.0f;}

	//! Get whether the landscape is currently fully visible (i.e. opaque).
	bool getIsFullyVisible() const {return landFader.getInterstate() >= 0.999f;}

	// GZ: NEW FUNCTION:
	//! can be used to find sunrise or visibility questions on the real-world landscape horizon.
	//! Default implementation indicates the horizon equals math horizon.
	virtual float getOpacity(Vec3d azalt) const {return (azalt[2]<0 ? 1.0f : 0.0f); }
	//! The list of azimuths and altitudes can come in various formats. We read the first two elements, which can be of formats:
	enum horizonListMode {
		azDeg_altDeg   = 0, //! azimuth[degrees] altitude[degrees]
		azDeg_zdDeg    = 1, //! azimuth[degrees] zenithDistance[degrees]
		azRad_altRad   = 2, //! azimuth[radians] altitude[radians]
		azRad_zdRad    = 3, //! azimuth[radians] zenithDistance[radians]
		azGrad_altGrad = 4, //! azimuth[new_degrees] altitude[new_degrees] (may be found on theodolites)
		azGrad_zdGrad  = 5  //! azimuth[new_degrees] zenithDistance[new_degrees] (may be found on theodolites)
	};
	
protected:
	//! Load attributes common to all landscapes
	//! @param landscapeIni A reference to an existing QSettings object which describes the landscape
	//! @param landscapeId The name of the directory for the landscape files (e.g. "ocean")
	void loadCommon(const QSettings& landscapeIni, const QString& landscapeId);
	//! Create a StelSphericalPolygon that describes a measured horizon line. If present, this can be used to draw a horizon line
	//! or simplify the functionality to discern if an object is below the horizon.
	//! @param _lineFileName A text file with lines that are either empty or comment lines starting with # or azimuth altitude [degrees]
	//! @param _polyAngleRotateZ possibility to set some final calibration offset like meridian convergence correction.
	void createPolygonalHorizon(const QString& lineFileName, const float polyAngleRotateZ=0.0f, const QString &listMode="azDeg_altDeg");

	//! search for a texture in landscape directory, else global textures directory
	//! @param basename The name of a texture file, e.g. "fog.png"
	//! @param landscapeId The landscape ID (directory name) to which the texture belongs
	//! @exception misc possibility of throwing "file not found" exceptions
	const QString getTexturePath(const QString& basename, const QString& landscapeId) const;
	float radius;
	QString name;          //! Read from landscape.ini:[landscape]name
	QString author;        //! Read from landscape.ini:[landscape]author
	QString description;   //! Read from landscape.ini:[landscape]description
	//float nightBrightness;
	float minBrightness;   //! Read from landscape.ini:[landscape]minimal_brightness. Allows minimum visibility that cannot be underpowered.
	float landscapeBrightness;  //! brightness [0..1] to draw the landscape. Computed by the LandscapeMgr.
	float lightScapeBrightness; //! can be used to draw nightscape texture (e.g. city light pollution), if available. Computed by the LandscapeMgr.
	bool validLandscape;   //! was a landscape loaded properly?
	LinearFader landFader; //! Used to slowly fade in/out landscape painting.
	LinearFader fogFader;  //! Used to slowly fade in/out fog painting.
	int rows; //! horizontal rows.  May be given in landscape.ini:[landscape]tesselate_rows. More indicates higher accuracy, but is slower.
	int cols; //! vertical columns. May be given in landscape.ini:[landscape]tesselate_cols. More indicates higher accuracy, but is slower.
	float angleRotateZ;    //! [radians] if pano does not have its left border in the east, rotate in azimuth. Configured in landscape.ini[landscape]angle_rotatez (or decor_angle_rotatez for old_style landscapes)
	float angleRotateZOffset; //! [radians] This is a rotation changeable at runtime via setZRotation (called by LandscapeMgr::setZRotation).
							  //! Not in landscape.ini: Used in special cases where the horizon may rotate, e.g. on a ship.

	StelLocation location; //! OPTIONAL. If present, can be used to set location.
	int defaultBortleIndex; //! May be given in landscape.ini:[location]light_pollution. Default: -1 (no change).
	int defaultFogSetting;  //! May be given in landscape.ini:[location]display_fog: -1(no change), 0(off), 1(on). Default: -1.
	float defaultExtinctionCoefficient; //! May be given in landscape.ini:[location]atmospheric_extinction_coefficient. Default -1 (no change).
	float defaultTemperature; //! [Celsius] May be given in landscape.ini:[location]atmospheric_temperature. default: -1000.0 (no change)
	float defaultPressure;    //! [mbar]    May be given in landscape.ini:[location]atmospheric_pressure. Default -1.0 (compute from [location]/altitude), use -2 to indicate "no change".

	// Optional elements which, if present, describe a horizon polygon. They can be used to render a line or a filled region, esp. in LandscapePolygonal
	SphericalRegionP horizonPolygon;   //! Optional element describing the horizon line.
									   //! Data shall be read from the file given as landscape.ini[landscape]polygonal_horizon_list
									   //! For LandscapePolygonal, this is the only horizon data item.
	Vec3f horizonPolygonLineColor ;    //! for all horizon types, the horizonPolygon line, if specified, will be drawn in this color
									   //! specified in landscape.ini[landscape]horizon_line_color. Negative red (default) indicated "don't draw".
};

//! @class LandscapeOldStyle
//! This was the original landscape, introduced for decorative purposes. It segments the horizon in several tiles
//! (usually 4 or 8), therefore allowing very high resolution horizons also on limited hardware,
//! and closes the ground with a separate bottom piece. (You may want to configure a map with pointers to surrounding mountains or a compass rose instead!)
//! You can use panoramas created in equirectangular or cylindrical coordinates, for the latter case set
//! [landscape]tan_mode=true.
//! Until V0.10.5 there was an undetected bug involving vertical positioning. For historical reasons (many landscapes
//! were already configured and published), it was decided to keep this bug as feature, but a fix for new landscapes is
//! available: [landscape]calibrated=true.
//! As of 0.10.6, the fix is only valid for equirectangular panoramas.
//! As of V0.13, [landscape]calibrated=true and [landscape]tan_mode=true go together for cylindrical panoramas.
//! It is more involved to configure, but may still be preferred if you require the resolution, e.g. for alignment studies
//! for archaeoastronomy. In this case, don't forget to set calibrated=true in landscape.ini.
class LandscapeOldStyle : public Landscape
{
public:
	LandscapeOldStyle(float radius = 2.f);
	virtual ~LandscapeOldStyle();
	virtual void load(const QSettings& landscapeIni, const QString& landscapeId);
	virtual void draw(StelCore* core);
	//void create(bool _fullpath, QMap<QString, QString> param); // still not implemented
	virtual float getOpacity(Vec3d azalt) const;
protected:
	typedef struct
	{
		StelTextureSP tex;
		float texCoords[4];
	} landscapeTexCoord;

private:
	void drawFog(StelCore* core, StelPainter&) const;
	void drawDecor(StelCore* core, StelPainter&) const;
	void drawGround(StelCore* core, StelPainter&) const;
	QVector<double> groundVertexArr;
	QVector<float> groundTexCoordArr;
	StelTextureSP* sideTexs;
	int nbSideTexs;
	int nbSide;
	landscapeTexCoord* sides;
	StelTextureSP fogTex;
	//landscapeTexCoord fogTexCoord; // GZ: UNUSED!
	StelTextureSP groundTex;
	QVector<QImage*> sidesImages; // GZ: Required for opacity lookup
	//landscapeTexCoord groundTexCoord; // GZ: UNUSED!
	int nbDecorRepeat;
	float fogAltAngle;
	float fogAngleShift;
	float decorAltAngle; // vertical extent of the side panels
	float decorAngleShift;
	float groundAngleShift; //! [radians]: altitude of the bottom plane. Usually negative and equal to decorAngleShift
	float groundAngleRotateZ; //! [radians]
	int drawGroundFirst;
	bool tanMode;		// Whether the angles should be converted using tan instead of sin, i.e., for a cylindrical pano
	bool calibrated;	// if true, the documented altitudes are indeed correct (the original code is buggy!)
	struct LOSSide
	{
		StelVertexArray arr;
		StelTextureSP tex;
	};

	QList<LOSSide> precomputedSides;
};

/////////////////////////////////////////////////////////
///
//! @class LandscapePolygonal
//! This uses the list of (usually measured) horizon altitudes to define the horizon.
//! Define it with the following names in landscape.ini:
//! @param landscape/ground_color use this color below horizon
//! @param landscape/polygonal_horizon_list filename containing azimuths/altitudes, compatible with Carte du Ciel.
//! @param landscape/polygonal_angle_rotatez offset for the polygonal measurement
//!        (different from landscape/angle_rotatez in photo panos, often photo and line are not aligned.)
class LandscapePolygonal : public Landscape
{
public:
	LandscapePolygonal(float radius = 1.f);
	virtual ~LandscapePolygonal();
	virtual void load(const QSettings& landscapeIni, const QString& landscapeId);
	virtual void draw(StelCore* core);
	virtual float getOpacity(Vec3d azalt) const;
private:
	// we have inherited: horizonFileName, horizonPolygon, horizonPolygonLineColor
	Vec3f groundColor; //! specified in landscape.ini[landscape]ground_color.
};

///////////////////////////////////////////////////////////////
///
//! @class LandscapeFisheye
//! This uses a single image in fisheye projection. The image is typically square, ...
//! @param texFov:  field of view (opening angle) of the square texture, radians.
//! If @param angleRotateZ==0, the top image border is due south.
class LandscapeFisheye : public Landscape
{
public:
	LandscapeFisheye(float radius = 1.f);
	virtual ~LandscapeFisheye();
	virtual void load(const QSettings& landscapeIni, const QString& landscapeId);
	virtual void draw(StelCore* core);
	//! Sample landscape texture for transparency/opacity. May be used for visibility, sunrise etc.
	//! @param azalt normalized direction in alt-az frame
	virtual float getOpacity(Vec3d azalt) const;
	//! create a fisheye landscape from basic parameters (no ini file needed).
	//! @param name Landscape name
	//! @param maptex the fisheye texture
	//! @param maptexIllum the fisheye texture that is overlaid in the night (streetlights, skyglow, ...)
	//! @param texturefov field of view for the photo, degrees
	//! @param angleRotateZ azimuth rotation angle, degrees
	void create(const QString name, const QString& maptex, float texturefov, float angleRotateZ);
	void create(const QString name, float texturefov, const QString& maptex, const QString &_maptexFog="", const QString& _maptexIllum="", const float angleRotateZ=0.0f);
private:

	StelTextureSP mapTex;      //!< The fisheye image, centered on the zenith.
	StelTextureSP mapTexFog;   //!< Optional panorama of identical size (create as layer over the mapTex image in your favorite image processor).
							   //!< can also be smaller, just the texture is again mapped onto the same geometry.
	StelTextureSP mapTexIllum; //!< Optional fisheye image of identical size (create as layer in your favorite image processor) or at least, proportions.
							   //!< To simulate light pollution (skyglow), street lights, light in windows, ... at night
	QImage *mapImage;          //!< The same image as mapTex, but stored in-mem for sampling.

	float texFov;
};

//////////////////////////////////////////////////////////////////////////
//! @class LandscapeSpherical
//! This uses a single panorama image in spherical (equirectangular) projection. A complete image is rectangular with the horizon forming a
//! horizontal line centered vertically, and vertical altitude angles linearly mapped in image height.
//! Since 0.13 and Qt5, large images of 8192x4096 pixels are available, but they still may not work on every hardware.
//! If @param angleRotateZ==0, the left/right image border is due east.
//! It is possible to remove empty top or bottom parts of the textures (main texture: only top part should meaningfully be cut away!)
//! The textures should still be power-of-two, so maybe 8192x1024 for the fog, or 8192x2048 for the light pollution.
//! (It's OK to stretch the textures. They just have to fit, geometrically!)
//! TODO: Allow a horizontal split for 2 or even 4 parts, i.e. super-large, super-accurate panos.
class LandscapeSpherical : public Landscape
{
public:
	LandscapeSpherical(float radius = 1.f);
	virtual ~LandscapeSpherical();
	virtual void load(const QSettings& landscapeIni, const QString& landscapeId);
	virtual void draw(StelCore* core);
	//! Sample landscape texture for transparency/opacity. May be used for visibility, sunrise etc.
	//! @param azalt normalized direction in alt-az frame
	//! @retval alpha (0=fully transparent, 1=fully opaque. Trees, leaves, glass etc may have intermediate values.)
	virtual float getOpacity(Vec3d azalt) const;
	//! create a spherical landscape from basic parameters (no ini file needed).
	//! @param name Landscape name
	//! @param maptex the equirectangular texture
	//! @param maptexIllum the equirectangular texture that is overlaid in the night (streetlights, skyglow, ...)
	//! @param angleRotateZ azimuth rotation angle, degrees [0]
	//! @param _mapTexTop altitude angle of top edge of texture, degrees [90]
	//! @param _mapTexBottom altitude angle of bottom edge of texture, degrees [-90]
	//! @param _fogTexTop altitude angle of top edge of fog texture, degrees [90]
	//! @param _fogTexBottom altitude angle of bottom edge of fog texture, degrees [-90]
	//! @param _illumTexTop altitude angle of top edge of light pollution texture, degrees [90]
	//! @param _illumTexBottom altitude angle of bottom edge of light pollution texture, degrees [-90]
	void create(const QString name, const QString& maptex, const QString &_maptexFog="", const QString& _maptexIllum="", const float _angleRotateZ=0.0f,
				const float _mapTexTop=90.0f, const float _mapTexBottom=-90.0f,
				const float _fogTexTop=90.0f, const float _fogTexBottom=-90.0f,
				const float _illumTexTop=90.0f, const float _illumTexBottom=-90.0f);
private:

	StelTextureSP mapTex;      //!< The equirectangular panorama texture
	StelTextureSP mapTexFog;   //!< Optional panorama of identical size (create as layer over the mapTex image in your favorite image processor).
							   //!< can also be smaller, just the texture is again mapped onto the same geometry.
	StelTextureSP mapTexIllum; //!< Optional panorama of identical size (create as layer over the mapTex image in your favorite image processor).
							   //!< To simulate light pollution (skyglow), street lights, light in windows, ... at night
	// These vars are here to conserve texture memory. They must be allowed to be different: a landscape may have its highest elevations at 15°, fog may reach from -25 to +15°,
	// light pollution may cover -5° (street lamps slightly below) plus parts of or even the whole sky. All have default values to simplify life.
	float mapTexTop;           //!< zenithal top angle of the landscape texture, radians
	float mapTexBottom;		   //!< zenithal bottom angle of the landscape texture, radians
	float fogTexTop;		   //!< zenithal top angle of the fog texture, radians
	float fogTexBottom;		   //!< zenithal bottom angle of the fog texture, radians
	float illumTexTop;		   //!< zenithal top angle of the illumination texture, radians
	float illumTexBottom;	   //!< zenithal bottom angle of the illumination texture, radians
	QImage *mapImage;          //!< The same image as mapTex, but stored in-mem for opacity sampling.
};

#endif // _LANDSCAPE_HPP_
