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

// This include must come first (before lux.h)
#include <boost/serialization/vector.hpp>

#include "renderserver.h"
#include "api.h"
#include "context.h"
#include "paramset.h"
#include "error.h"
#include "color.h"
#include "osfunc.h"
#include "version.h"

#include <boost/version.hpp>
#if (BOOST_VERSION < 103401)
#include <boost/filesystem/operations.hpp>
#else
#include <boost/filesystem.hpp>
#endif
#include <fstream>
#include <boost/asio.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/restrict.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread/thread.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace lux;
using namespace boost::iostreams;
using namespace boost::filesystem;
using namespace std;
using boost::asio::ip::tcp;

//------------------------------------------------------------------------------
// RenderServer
//------------------------------------------------------------------------------

RenderServer::RenderServer(int tCount, int port, bool wFlmFile) : errorMessages(), threadCount(tCount),
	tcpPort(port), writeFlmFile(wFlmFile), state(UNSTARTED), serverThread(NULL)
{
}

RenderServer::~RenderServer()
{
	if ((state == READY) && (state == BUSY))
		stop();
}

void RenderServer::start() {
	if (state != UNSTARTED) {
		LOG( LUX_ERROR,LUX_SYSTEM) << "Can not start a rendering server in state: " << state;
		return;
	}

	LOG( LUX_INFO,LUX_NOERROR) << "Launching server mode [" << threadCount << " threads]";
	LOG( LUX_DEBUG,LUX_NOERROR) << "Server version " << LUX_SERVER_VERSION_STRING;

	// Dade - start the tcp server threads
	serverThread = new NetworkRenderServerThread(this);

	serverThread->serverThread6 = new boost::thread(boost::bind(
		NetworkRenderServerThread::run, 6, serverThread));
	serverThread->serverThread4 = new boost::thread(boost::bind(
		NetworkRenderServerThread::run, 4, serverThread));

	state = READY;
}

void RenderServer::join()
{
	if ((state != READY) && (state != BUSY)) {
		LOG( LUX_ERROR,LUX_SYSTEM) << "Can not join a rendering server in state: " << state;
		return;
	}

	serverThread->join();
}

void RenderServer::stop()
{
	if ((state != READY) && (state != BUSY)) {
		LOG( LUX_ERROR,LUX_SYSTEM) << "Can not stop a rendering server in state: " << state;
		return;
	}

	serverThread->interrupt();
	serverThread->join();

	state = STOPPED;
}

void RenderServer::errorHandler(int code, int severity, const char *msg) {
	boost::mutex::scoped_lock(errorMessageLock);
	errorMessages.push_back(ErrorMessage(code, severity, msg));
}

//------------------------------------------------------------------------------
// NetworkRenderServerThread
//------------------------------------------------------------------------------

static void printInfoThread()
{
	while (true) {
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		xt.sec += 5;
		boost::thread::sleep(xt);

		int sampleSec = static_cast<int>(luxStatistics("samplesSec"));
		// Print only if we are rendering something
		if (sampleSec > 0)
			LOG( LUX_INFO,LUX_NOERROR) << luxPrintableStatistics(true);
	}
}

static void writeTransmitFilm(basic_ostream<char> &stream, const string &filename)
{
	string file = filename;
	string tempfile = file + ".temp";

	LOG( LUX_DEBUG,LUX_NOERROR) << "Writing film samples to file '" << tempfile << "'";

	ofstream out(tempfile.c_str(), ios::out | ios::binary);
	Context::GetActive()->TransmitFilm(out, true, false);
	out.close();

	if (!out.fail()) {
		remove(file.c_str());
		if (rename(tempfile.c_str(), file.c_str())) {
			LOG(LUX_ERROR, LUX_SYSTEM) << 
				"Failed to rename new film file, leaving new film file as '" << tempfile << "'";
			file = tempfile;
		}

		LOG( LUX_DEBUG,LUX_NOERROR) << "Transmitting film samples from file '" << file << "'";
		ifstream in(file.c_str(), ios::in | ios::binary);

		boost::iostreams::copy(in, stream);

		if (in.fail())
			LOG(LUX_ERROR, LUX_SYSTEM) << "There was an error while transmitting from file '" << file << "'";

		in.close();
	} else {
		LOG(LUX_ERROR, LUX_SYSTEM) << "There was an error while writing file '" << tempfile << "'";
	}
}

static void processCommandParams(bool isLittleEndian,
		ParamSet &params, basic_istream<char> &stream) {
	stringstream uzos(stringstream::in | stringstream::out | stringstream::binary);
	{
		// Read the size of the compressed chunk
		uint32_t size = osReadLittleEndianUInt(isLittleEndian, stream);

		// Uncompress the chunk
		filtering_stream<input> in;
		in.push(gzip_decompressor());
		in.push(boost::iostreams::restrict(stream, 0, size));
		boost::iostreams::copy(in, uzos);
	}

	// Deserialize the parameters
	boost::archive::text_iarchive ia(uzos);
	ia >> params;
}

static void processCommandFilm(bool isLittleEndian,
		void (Context::*f)(const string &, const ParamSet &), basic_istream<char> &stream)
{
	string type;
	getline(stream, type);

	if((type != "fleximage") && (type != "multiimage")) {
		LOG( LUX_ERROR,LUX_SYSTEM) << "Unsupported film type for server rendering: " << type;
		return;
	}

	ParamSet params;
	processCommandParams(isLittleEndian, params, stream);

	// Dade - overwrite some option for the servers

	params.EraseBool("write_exr");
	params.EraseBool("write_exr_ZBuf");
	params.EraseBool("write_png");
	params.EraseBool("write_png_ZBuf");
	params.EraseBool("write_tga");
	params.EraseBool("write_tga_ZBuf");
	params.EraseBool("write_resume_flm");

	bool no = false;
	params.AddBool("write_exr", &no);
	params.AddBool("write_exr_ZBuf", &no);
	params.AddBool("write_png", &no);
	params.AddBool("write_png_ZBuf", &no);
	params.AddBool("write_tga", &no);
	params.AddBool("write_tga_ZBuf", &no);
	params.AddBool("write_resume_flm", &no);

	(Context::GetActive()->*f)(type, params);
}

static void processFile(const string &fileParam, ParamSet &params, vector<string> &tmpFileList, basic_istream<char> &stream)
{
	string originalFile = params.FindOneString(fileParam, "");
	if (originalFile.size()) {
		// Dade - look for file extension
		string fileExt = "";
		size_t idx = originalFile.find_last_of('.');
		if (idx != string::npos)
			fileExt = originalFile.substr(idx);

		// Dade - replace the file name with a temporary name
		char buf[64];
		// MSVC doesn't support z length modifier (size_t) for snprintf, so workaround by casting to unsigned long...
		if (tmpFileList.size())
			snprintf(buf, 64, "%5s_%08lu%s", tmpFileList[0].c_str(),
				((unsigned long)tmpFileList.size()), fileExt.c_str()); // %08zu should be ued but it could be not supported by the compiler
		else
			snprintf(buf, 64, "00000_%08lu%s",
				((unsigned long)tmpFileList.size()), fileExt.c_str()); // %08zu should be ued but it could be not supported by the compiler
		string file = string(buf);

		// Dade - replace the filename parameter
		params.AddString(fileParam, &file);

		// Read the file size
		string slen;
		getline(stream, slen); // Eat the \n
		getline(stream, slen);
		// Limiting the file size to 2G should be a problem
		int len = atoi(slen.c_str());

		LOG( LUX_INFO,LUX_NOERROR) << "Receiving file: '" << originalFile << "' (in '" <<
			file << "' size: " << (len / 1024) << " Kbytes)";

		// Dade - fix for bug 514: avoid to create the file if it is empty
		if (len > 0) {
			ofstream out(file.c_str(), ios::out | ios::binary);

			std::streamsize written = boost::iostreams::copy(
				boost::iostreams::restrict(stream, 0, len), out);

			out.flush();
			tmpFileList.push_back(file);

			if (out.fail() || written != len) {
				LOG( LUX_ERROR,LUX_SYSTEM) << "There was an error while writing file '" << file << "'";
			}
		}
	}
}

static void processCommand(bool isLittleEndian,
	void (Context::*f)(const string &, const ParamSet &),
	vector<string> &tmpFileList, basic_istream<char> &stream)
{
	string type;
	getline(stream, type);

	ParamSet params;
	processCommandParams(isLittleEndian, params, stream);

	processFile("mapname", params, tmpFileList, stream);
	processFile("iesname", params, tmpFileList, stream);
	processFile("filename", params, tmpFileList, stream);

	(Context::GetActive()->*f)(type, params);
}

static void processCommand(void (Context::*f)(const string &), basic_istream<char> &stream)
{
	string type;
	getline(stream, type);

	(Context::GetActive()->*f)(type);
}

static void processCommand(void (Context::*f)(float, float), basic_istream<char> &stream)
{
	float x, y;
	stream >> x;
	stream >> y;
	(Context::GetActive()->*f)(x, y);
}

static void processCommand(void (Context::*f)(float, float, float), basic_istream<char> &stream)
{
	float ax, ay, az;
	stream >> ax;
	stream >> ay;
	stream >> az;
	(Context::GetActive()->*f)(ax, ay, az);
}

static void processCommand(void (Context::*f)(float[16]), basic_istream<char> &stream)
{
	float t[16];
	for (int i = 0; i < 16; ++i)
		stream >> t[i];
	(Context::GetActive()->*f)(t);
}

static void processCommand(void (Context::*f)(u_int, float*), basic_istream<char> &stream)
{
	u_int n;
	stream >> n;
	vector<float> data;
	for (u_int i = 0; i < n; ++i) {
		float v;
		stream >> v;
		data.push_back(v);
	}
	(Context::GetActive()->*f)(n, &data[0]);
}

static void processCommand(void (Context::*f)(const string &, float, float, const string &), basic_istream<char> &stream)
{
	string name, transform;
	float a, b;

	getline(stream, name);
	stream >> a;
	stream >> b;
	stream.ignore(2, '\n');
	getline(stream, transform);

	(Context::GetActive()->*f)(name, a, b, transform);
}


void RenderServer::createNewSessionID() {
	currentSID = boost::uuids::random_generator()();
}

bool RenderServer::validateAccess(basic_istream<char> &stream) const {
	if (serverThread->renderServer->state != RenderServer::BUSY) {
		LOG( LUX_INFO,LUX_NOERROR)<< "Slave does not have an active session";
		return false;
	}

	string sidstr;
	if (!getline(stream, sidstr))
		return false;

	boost::uuids::uuid sid = boost::uuids::string_generator()(sidstr);

	LOG( LUX_DEBUG,LUX_NOERROR) << "Validating SID: " << sid << " = " << currentSID;

	return (sid == currentSID);
}

// command handlers
void cmd_NOOP(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
	// do nothing
}
void cmd_ServerDisconnect(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_SERVER_DISCONNECT:
	if (!serverThread->renderServer->validateAccess(stream))
		return;

	LOG( LUX_INFO,LUX_NOERROR) << "Master ended session, cleaning up";

	// Dade - stop the rendering and cleanup
	luxExit();
	luxWait();
	luxCleanup();

	// Dade - remove all temporary files
	for (size_t i = 1; i < tmpFileList.size(); i++)
		remove(tmpFileList[i]);

	serverThread->renderServer->setServerState(RenderServer::READY);
	LOG( LUX_INFO,LUX_NOERROR) << "Server ready";
}
void cmd_ServerConnect(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_SERVER_CONNECT:
	if (serverThread->renderServer->getServerState() == RenderServer::READY) {
		serverThread->renderServer->setServerState(RenderServer::BUSY);
		stream << "OK" << endl;

		// Send version string
		stream << LUX_SERVER_VERSION_STRING << endl;

		// Dade - generate the session ID
		serverThread->renderServer->createNewSessionID();
		LOG( LUX_INFO,LUX_NOERROR) << "New session ID: " << serverThread->renderServer->getCurrentSID();
		stream << serverThread->renderServer->getCurrentSID() << endl;

		tmpFileList.clear();
		char buf[6];
		snprintf(buf, 6, "%05d", serverThread->renderServer->getTcpPort());
		tmpFileList.push_back(string(buf));

		// now perform handshake
		if (!stream.good() || !serverThread->renderServer->validateAccess(stream)) {
			LOG( LUX_WARNING,LUX_SYSTEM)<< "Connection handshake failed, session aborted";
			serverThread->renderServer->setServerState(RenderServer::READY);
			return;
		}

		stream << "CONNECTED" << endl;
	} else
		stream << "BUSY" << endl;
}
void cmd_ServerReconnect(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_SERVER_RECONNECT:
	if (serverThread->renderServer->validateAccess(stream)) {
		stream << "CONNECTED" << endl;
	} else if (serverThread->renderServer->getServerState() == RenderServer::BUSY) {
		// server is busy, but validation failed, means the master's SID didn't match ours.
		stream << "DENIED" << endl;
	} else {
		// server doesn't have an active session
		stream << "IDLE" << endl;
	}
}
void cmd_luxInit(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXINIT:
	LOG( LUX_SEVERE,LUX_BUG)<< "Server already initialized";
}
void cmd_luxTranslate(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXTRANSLATE:
	processCommand(&Context::Translate, stream);
}
void cmd_luxRotate(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXROTATE:
	float angle, ax, ay, az;
	stream >> angle;
	stream >> ax;
	stream >> ay;
	stream >> az;
	luxRotate(angle, ax, ay, az);
}
void cmd_luxScale(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXSCALE:
	processCommand(&Context::Scale, stream);
}
void cmd_luxLookAt(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXLOOKAT:
	float ex, ey, ez, lx, ly, lz, ux, uy, uz;
	stream >> ex;
	stream >> ey;
	stream >> ez;
	stream >> lx;
	stream >> ly;
	stream >> lz;
	stream >> ux;
	stream >> uy;
	stream >> uz;
	luxLookAt(ex, ey, ez, lx, ly, lz, ux, uy, uz);
}
void cmd_luxConcatTransform(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXCONCATTRANSFORM:
	processCommand(&Context::ConcatTransform, stream);
}
void cmd_luxTransform(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXTRANSFORM:
	processCommand(&Context::Transform, stream);
}
void cmd_luxIdentity(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXIDENTITY:
	luxIdentity();
}
void cmd_luxCoordinateSystem(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXCOORDINATESYSTEM:
	processCommand(&Context::CoordinateSystem, stream);
}
void cmd_luxCoordSysTransform(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXCOORDSYSTRANSFORM:
	processCommand(&Context::CoordSysTransform, stream);
}
void cmd_luxPixelFilter(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXPIXELFILTER:
	processCommand(isLittleEndian, &Context::PixelFilter, tmpFileList, stream);
}
void cmd_luxFilm(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXFILM:
	// Dade - Servers use a special kind of film to buffer the
	// samples. I overwrite some option here.

	processCommandFilm(isLittleEndian, &Context::Film, stream);
}
void cmd_luxSampler(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXSAMPLER:
	processCommand(isLittleEndian, &Context::Sampler, tmpFileList, stream);
}
void cmd_luxAccelerator(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXACCELERATOR:
	processCommand(isLittleEndian, &Context::Accelerator, tmpFileList, stream);
}
void cmd_luxSurfaceIntegrator(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXSURFACEINTEGRATOR:
	processCommand(isLittleEndian, &Context::SurfaceIntegrator, tmpFileList, stream);
}
void cmd_luxVolumeIntegrator(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXVOLUMEINTEGRATOR:
	processCommand(isLittleEndian, &Context::VolumeIntegrator, tmpFileList, stream);
}
void cmd_luxCamera(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXCAMERA:
	processCommand(isLittleEndian, &Context::Camera, tmpFileList, stream);
}
void cmd_luxWorldBegin(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXWORLDBEGIN:
	luxWorldBegin();
}
void cmd_luxAttributeBegin(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXATTRIBUTEBEGIN:
	luxAttributeBegin();
}
void cmd_luxAttributeEnd(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXATTRIBUTEEND:
	luxAttributeEnd();
}
void cmd_luxTransformBegin(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXTRANSFORMBEGIN:
	luxTransformBegin();
}
void cmd_luxTransformEnd(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXTRANSFORMEND:
	luxTransformEnd();
}
void cmd_luxTexture(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXTEXTURE:
	string name, type, texname;
	ParamSet params;
	// Dade - fixed in bug 562: "Luxconsole -s (Linux 64) fails to network render when material names contain spaces"
	getline(stream, name);
	getline(stream, type);
	getline(stream, texname);

	processCommandParams(isLittleEndian, params, stream);

	processFile("filename", params, tmpFileList, stream);

	Context::GetActive()->Texture(name, type, texname, params);
}
void cmd_luxMaterial(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXMATERIAL:
	processCommand(isLittleEndian, &Context::Material, tmpFileList, stream);
}
void cmd_luxMakeNamedMaterial(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXMAKENAMEDMATERIAL:
	processCommand(isLittleEndian, &Context::MakeNamedMaterial, tmpFileList, stream);
}
void cmd_luxNamedMaterial(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXNAMEDMATERIAL:
	processCommand(&Context::NamedMaterial, stream);
}
void cmd_luxLightGroup(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXLIGHTGROUP:
	processCommand(isLittleEndian, &Context::LightGroup, tmpFileList, stream);
}
void cmd_luxLightSource(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXLIGHTSOURCE:
	processCommand(isLittleEndian, &Context::LightSource, tmpFileList, stream);
}
void cmd_luxAreaLightSource(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXAREALIGHTSOURCE:
	processCommand(isLittleEndian, &Context::AreaLightSource, tmpFileList, stream);
}
void cmd_luxPortalShape(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXPORTALSHAPE:
	processCommand(isLittleEndian, &Context::PortalShape, tmpFileList, stream);
}
void cmd_luxShape(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXSHAPE:
	processCommand(isLittleEndian, &Context::Shape, tmpFileList, stream);
}
void cmd_luxReverseOrientation(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXREVERSEORIENTATION:
	luxReverseOrientation();
}
void cmd_luxMakeNamedVolume(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXMAKENAMEDVOLUME:
	string id, name;
	ParamSet params;
	getline(stream, id);
	getline(stream, name);

	processCommandParams(isLittleEndian,
		params, stream);

	Context::GetActive()->MakeNamedVolume(id, name, params);
}
void cmd_luxVolume(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXVOLUME:
	processCommand(isLittleEndian, &Context::Volume, tmpFileList, stream);
}
void cmd_luxExterior(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXEXTERIOR:
	processCommand(&Context::Exterior, stream);
}
void cmd_luxInterior(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXINTERIOR:
	processCommand(&Context::Interior, stream);
}
void cmd_luxObjectBegin(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXOBJECTBEGIN:
	processCommand(&Context::ObjectBegin, stream);
}
void cmd_luxObjectEnd(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXOBJECTEND:
	luxObjectEnd();
}
void cmd_luxObjectInstance(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXOBJECTINSTANCE:
	processCommand(&Context::ObjectInstance, stream);
}
void cmd_luxPortalInstance(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_PORTALINSTANCE:
	processCommand(&Context::PortalInstance, stream);
}
void cmd_luxMotionBegin(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXMOTIONBEGIN:
	processCommand(&Context::MotionBegin, stream);
}
void cmd_luxMotionEnd(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXMOTIONEND:
	luxMotionEnd();
}
void cmd_luxMotionInstance(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_MOTIONINSTANCE:
	processCommand(&Context::MotionInstance, stream);
}
void cmd_luxWorldEnd(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXWORLDEND:
	serverThread->engineThread = new boost::thread(&luxWorldEnd);

	// Wait the scene parsing to finish
	while (!luxStatistics("sceneIsReady")) {
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		xt.sec += 1;
		boost::thread::sleep(xt);
	}

	// Dade - start the info thread only if it is not already running
	if(!serverThread->infoThread)
		serverThread->infoThread = new boost::thread(&printInfoThread);

	// Add rendering threads
	int threadsToAdd = serverThread->renderServer->getThreadCount();
	while (--threadsToAdd)
		luxAddThread();
}
void cmd_luxGetFilm(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXGETFILM:
	// Dade - check if we are rendering something
	if (serverThread->renderServer->getServerState() == RenderServer::BUSY) {
		if (!serverThread->renderServer->validateAccess(stream)) {
			LOG( LUX_ERROR,LUX_SYSTEM)<< "Unknown session ID";
			stream.close();
			return;
		}

		LOG( LUX_INFO,LUX_NOERROR)<< "Transmitting film samples";

		if (serverThread->renderServer->getWriteFlmFile()) {
			string file = "server_resume";
			if (tmpFileList.size())
			file += "_" + tmpFileList[0];
			file += ".flm";

			writeTransmitFilm(stream, file);
		} else {
			Context::GetActive()->TransmitFilm(stream);
		}
		stream.close();

		LOG( LUX_INFO,LUX_NOERROR)<< "Finished film samples transmission";
	} else {
		LOG( LUX_ERROR,LUX_SYSTEM)<< "Received a GetFilm command after a ServerDisconnect";
		stream.close();
	}
}
void cmd_luxGetLog(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXGETLOG:
	// Dade - check if we are rendering something
	if (serverThread->renderServer->getServerState() == RenderServer::BUSY) {
		if (!serverThread->renderServer->validateAccess(stream)) {
			LOG( LUX_ERROR,LUX_SYSTEM)<< "Unknown session ID";
			stream.close();
			return;
		}

		LOG( LUX_DEBUG,LUX_NOERROR)<< "Transmitting log";

		for (vector<RenderServer::ErrorMessage>::iterator it = serverThread->renderServer->errorMessages.begin(); it != serverThread->renderServer->errorMessages.end(); ++it) {
			stringstream ss("");
			ss << it->code << " " << it->severity << " " << it->message << "\n";
			stream << ss.str();
		}

		stream.close();

		serverThread->renderServer->errorMessages.clear();

		LOG( LUX_DEBUG,LUX_NOERROR)<< "Finished log transmission";
	} else {
		LOG( LUX_ERROR,LUX_SYSTEM)<< "Received a GetLog command after a ServerDisconnect";
		stream.close();
	}
}
void cmd_luxSetEpsilon(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXSETEPSILON:
	processCommand(&Context::SetEpsilon, stream);
}
void cmd_luxRenderer(bool isLittleEndian, NetworkRenderServerThread *serverThread, tcp::iostream& stream, vector<string> &tmpFileList) {
//case CMD_LUXRENDERER:
	processCommand(isLittleEndian, &Context::Renderer, tmpFileList, stream);
}

// Dade - TODO: support signals
void NetworkRenderServerThread::run(int ipversion, NetworkRenderServerThread *serverThread)
{
	boost::mutex::scoped_lock initLock(serverThread->initMutex);

	const int listenPort = serverThread->renderServer->tcpPort;
	const bool isLittleEndian = osIsLittleEndian();

	vector<string> tmpFileList;

	typedef boost::function<void (tcp::iostream&)> cmdfunc_t;
	#define INSERT_CMD(CmdName) cmds.insert(std::pair<string, cmdfunc_t>(#CmdName, boost::bind(cmd_##CmdName, isLittleEndian, serverThread, _1, boost::ref(tmpFileList))))

	map<string, cmdfunc_t> cmds;

	// Insert command handlers

	//case CMD_VOID:
	cmds.insert(std::pair<string, cmdfunc_t>("", boost::bind(cmd_NOOP, isLittleEndian, serverThread, _1, boost::ref(tmpFileList))));
	//case CMD_SPACE:
	cmds.insert(std::pair<string, cmdfunc_t>(" ", boost::bind(cmd_NOOP, isLittleEndian, serverThread, _1, boost::ref(tmpFileList))));

	INSERT_CMD(ServerDisconnect);
	INSERT_CMD(ServerConnect);
	INSERT_CMD(ServerReconnect);
	INSERT_CMD(luxInit);
	INSERT_CMD(luxTranslate);
	INSERT_CMD(luxRotate);
	INSERT_CMD(luxScale);
	INSERT_CMD(luxLookAt);
	INSERT_CMD(luxConcatTransform);
	INSERT_CMD(luxTransform);
	INSERT_CMD(luxIdentity);
	INSERT_CMD(luxCoordinateSystem);
	INSERT_CMD(luxCoordSysTransform);
	INSERT_CMD(luxPixelFilter);
	INSERT_CMD(luxFilm);
	INSERT_CMD(luxSampler);
	INSERT_CMD(luxAccelerator);
	INSERT_CMD(luxSurfaceIntegrator);
	INSERT_CMD(luxVolumeIntegrator);
	INSERT_CMD(luxCamera);
	INSERT_CMD(luxWorldBegin);
	INSERT_CMD(luxAttributeBegin);
	INSERT_CMD(luxAttributeEnd);
	INSERT_CMD(luxTransformBegin);
	INSERT_CMD(luxTransformEnd);
	INSERT_CMD(luxTexture);
	INSERT_CMD(luxMaterial);
	INSERT_CMD(luxMakeNamedMaterial);
	INSERT_CMD(luxNamedMaterial);
	INSERT_CMD(luxLightGroup);
	INSERT_CMD(luxLightSource);
	INSERT_CMD(luxAreaLightSource);
	INSERT_CMD(luxPortalShape);
	INSERT_CMD(luxShape);
	INSERT_CMD(luxReverseOrientation);
	INSERT_CMD(luxMakeNamedVolume);
	INSERT_CMD(luxVolume);
	INSERT_CMD(luxExterior);
	INSERT_CMD(luxInterior);
	INSERT_CMD(luxObjectBegin);
	INSERT_CMD(luxObjectEnd);
	INSERT_CMD(luxObjectInstance);
	INSERT_CMD(luxPortalInstance);
	INSERT_CMD(luxMotionBegin);
	INSERT_CMD(luxMotionEnd);
	INSERT_CMD(luxMotionInstance);
	INSERT_CMD(luxWorldEnd);
	INSERT_CMD(luxGetFilm);
	INSERT_CMD(luxGetLog);
	INSERT_CMD(luxSetEpsilon);
	INSERT_CMD(luxRenderer);

	#undef INSERT_CMD

	try {
		const bool reuse_addr = true;

		boost::asio::io_service io_service;
		tcp::endpoint endpoint(ipversion == 4 ? tcp::v4() : tcp::v6(), listenPort);
		tcp::acceptor acceptor(io_service);

		acceptor.open(endpoint.protocol());
		if (reuse_addr)
			acceptor.set_option(boost::asio::socket_base::reuse_address(true));
		if (endpoint.protocol() != tcp::v4())
			acceptor.set_option(boost::asio::ip::v6_only(true));
		acceptor.bind(endpoint);
		acceptor.listen();
		
		LOG(LUX_INFO,LUX_NOERROR) << "Server listening on " << endpoint;
		
		// release init lock
		initLock.unlock();

		vector<char> buffer(2048);
		while (serverThread->signal == SIG_NONE) {
			tcp::iostream stream;
			stream.rdbuf()->pubsetbuf(&buffer[0], buffer.size());
			acceptor.accept(*stream.rdbuf());
			stream.setf(ios::scientific, ios::floatfield);
			stream.precision(16);

			//reading the command
			string command;
			LOG( LUX_DEBUG,LUX_NOERROR) << "Server receiving commands...";
			while (getline(stream, command)) {

				if ((command != "") && (command != " ")) {
					LOG(LUX_DEBUG,LUX_NOERROR) << "... processing command: '" << command << "'";
				}

				if (cmds.find(command) != cmds.end()) {
					cmdfunc_t cmdhandler = cmds.find(command)->second;
					cmdhandler(stream);
				} else {
					LOG( LUX_SEVERE,LUX_BUG) << "Unknown command '" << command << "'. Ignoring";
				}

				//END OF COMMAND PROCESSING
			}
		}
	} catch (boost::system::system_error& e) {
		if (e.code() != boost::asio::error::address_family_not_supported)
			LOG(LUX_ERROR,LUX_BUG) << "Internal error: " << e.what();
		else
			LOG(LUX_INFO,LUX_NOERROR) << "IPv" << ipversion << " not available";
	} catch (exception& e) {
		LOG(LUX_ERROR,LUX_BUG) << "Internal error: " << e.what();
	}
}
