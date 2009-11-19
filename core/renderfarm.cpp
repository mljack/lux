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

// This include must come before lux.h
#include <boost/serialization/vector.hpp>

#include "lux.h"
#include "scene.h"
#include "api.h"
#include "error.h"
#include "paramset.h"
#include "renderfarm.h"
#include "camera.h"
#include "osfunc.h"

#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>

using namespace boost::iostreams;
using namespace boost::posix_time;
using namespace lux;
using namespace std;
using boost::asio::ip::tcp;

void FilmUpdaterThread::updateFilm(FilmUpdaterThread *filmUpdaterThread) {
	// Dade - thread to update the film with data from servers

	boost::xtime reft;
	boost::xtime_get(&reft, boost::TIME_UTC);

	while (filmUpdaterThread->signal == SIG_NONE) {
		// Dade - check signal every 1 sec

		for(;;) {
			// Dade - sleep for 1 sec
			boost::xtime xt;
			boost::xtime_get(&xt, boost::TIME_UTC);
			xt.sec += 1;
			boost::thread::sleep(xt);

			if (filmUpdaterThread->signal == SIG_EXIT)
				break;

			if (xt.sec - reft.sec > filmUpdaterThread->renderFarm->serverUpdateInterval) {
				reft = xt;
				break;
			}
		}

		if (filmUpdaterThread->signal == SIG_EXIT)
			break;

		filmUpdaterThread->renderFarm->updateFilm(filmUpdaterThread->scene);
	}
}

// Dade - used to periodically update the film
void RenderFarm::startFilmUpdater(Scene *scene) {
	if (filmUpdateThread == NULL) {
		filmUpdateThread = new FilmUpdaterThread(this, scene);
		filmUpdateThread->thread = new boost::thread(boost::bind(
			FilmUpdaterThread::updateFilm, filmUpdateThread));
	} else {
		luxError(LUX_ILLSTATE,LUX_ERROR,"RenderFarm::startFilmUpdater() called but update thread already started.");
	}
}

void RenderFarm::stopFilmUpdater() {
	if (filmUpdateThread != NULL) {
		filmUpdateThread->interrupt();
		delete filmUpdateThread;
		filmUpdateThread = NULL;
	}
	// Dade - stopFilmUpdater() is called multiple times at the moment (for instance
	// haltspp + luxconsole)
	/*else {
		luxError(LUX_ILLSTATE,LUX_ERROR,"RenderFarm::stopFilmUpdater() called but update thread not started.");
	}*/
}

void RenderFarm::decodeServerName(const string &serverName, string &name, string &port) {
	// Dade - check if the server name includes the port
	size_t idx = serverName.find_last_of(':');
	if (idx != string::npos) {
		// Dade - the server name includes the port number

		name = serverName.substr(0, idx);
		port = serverName.substr(idx + 1);
	} else {
		name = serverName;
		port = "18018";
	}
}

bool RenderFarm::connect(const string &serverName) {
	// Dade - connect to the rendering server

	stringstream ss;
	try {
		ss.str("");
		ss << "Connecting server: " << serverName;
		luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

		string name, port;
		decodeServerName(serverName, name, port);

		tcp::iostream stream(name, port);
		stream << "ServerConnect" << endl;

		// Dede - check if the server accepted the connection

		string result;
		if (!getline(stream, result)) {
			ss.str("");
			ss << "Unable to connect server: " << serverName;
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

			return false;
		}

		ss.str("");
		ss << "Server connect result: " << result;
		luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

		string sid;
		if ("OK" != result) {
			ss.str("");
			ss << "Unable to connect server: " << serverName;
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

			return false;
		} else {
			// Dade - read the session ID
			if (!getline(stream, result)) {
				ss.str("");
				ss << "Unable read session ID from server: " << serverName;
				luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

				return false;
			}

			sid = result;
			ss.str("");
			ss << "Server session ID: " << sid;
			luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());
		}

		serverInfoList.push_back(ExtRenderingServerInfo(name, port, sid));
	} catch (exception& e) {
		ss.str("");
		ss << "Unable to connect server: " << serverName;
		luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
		return false;
	}

	if (netBuffer.rdbuf()->in_avail() > 0)
		flush();  // Only flush if commands are written already.

	return true;
}

void RenderFarm::disconnectAll() {
	for (size_t i = 0; i < serverInfoList.size(); i++)
		disconnect(serverInfoList[i]);
	serverInfoList.clear();
}

void RenderFarm::disconnect(const string &serverName) {
	string name, port;
	decodeServerName(serverName, name, port);

	for (vector<ExtRenderingServerInfo>::iterator it = serverInfoList.begin(); it < serverInfoList.end(); it++ ) {
		if (name.compare(it->name) == 0 && port.compare(it->port) == 0) {
			disconnect(*it);
			serverInfoList.erase(it);
			break;
		}
	}
}

void RenderFarm::disconnect(const ExtRenderingServerInfo &serverInfo) {
	stringstream ss;
	try {
		ss.str("");
		ss << "Disconnect from server: " <<
				serverInfo.name << ":" << serverInfo.port;
		luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

		tcp::iostream stream(serverInfo.name, serverInfo.port);
		stream << "ServerDisconnect" << endl;
		stream << serverInfo.sid << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::flush() {
	stringstream ss;
	// Dade - the buffers with all commands
	string commands = netBuffer.str();

	ss.str("");
	ss << "Compiled scene size: " << (commands.size() / 1024) << "KBytes";
	luxError(LUX_NOERROR, LUX_DEBUG, ss.str().c_str());

	//flush network buffer
	for (size_t i = 0; i < serverInfoList.size(); i++) {
		if(serverInfoList[i].flushed == false) {
			try {
				ss.str("");
				ss << "Sending commands to server: " <<
						serverInfoList[i].name << ":" << serverInfoList[i].port;
				luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

				tcp::iostream stream(serverInfoList[i].name, serverInfoList[i].port);
				stream << commands << endl;

				serverInfoList[i].flushed = true;
			} catch (exception& e) {
				luxError(LUX_SYSTEM, LUX_ERROR, e.what());
			}
		}
	}

	// Dade - write info only if there was the communication with some server
	if (serverInfoList.size() > 0) {
		ss.str("");
		ss << "All servers are aligned";
		luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());
	}
}

void RenderFarm::updateFilm(Scene *scene) {
	// Dade - network rendering supports only FlexImageFilm
	Film *film = scene->camera->film;

	stringstream ss;
	for (size_t i = 0; i < serverInfoList.size(); i++) {
		try {
			ss.str("");
			ss << "Getting samples from: " <<
					serverInfoList[i].name << ":" << serverInfoList[i].port;
			luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

			// I'm using directly a tcp::socket here in order to be able to enable
			// keep alive option and to set keep alive parameters
			boost::asio::io_service ioService;
			tcp::resolver resolver(ioService);
			tcp::resolver::query query(serverInfoList[i].name, serverInfoList[i].port);
			tcp::resolver::iterator iterator = resolver.resolve(query);

			// Connect to the server
			tcp::socket slaveSocket(ioService);
			slaveSocket.connect(*iterator);

			// Enable keep alive option
			boost::asio::socket_base::keep_alive option(true);
			slaveSocket.set_option(option);
#if defined(__linux__) || defined(__MACOSX__)
			// Set keep alive parameters on *nix platforms
			const int nativeSocket = static_cast<int>(slaveSocket.native());
			int optval = 3; // Retry count
			const socklen_t optlen = sizeof(optval);
			setsockopt(nativeSocket, SOL_TCP, TCP_KEEPCNT, &optval, optlen);
			optval = 30; // Keep alive interval
			setsockopt(nativeSocket, SOL_TCP, TCP_KEEPIDLE, &optval, optlen);
			optval = 5; // Time between retries
			setsockopt(nativeSocket, SOL_TCP, TCP_KEEPINTVL, &optval, optlen);
#endif

			// Send the command to get the film
			ss.str("");
			ss << "luxGetFilm" << std::endl;
			boost::asio::write(slaveSocket, boost::asio::buffer(ss.str().c_str(), ss.str().size()));
			ss.str("");
			ss << serverInfoList[i].sid << std::endl;
			boost::asio::write(slaveSocket, boost::asio::buffer(ss.str().c_str(), ss.str().size()));

			// Receive the film in a compressed format
			std::stringstream compressedStream(std::stringstream::in |
					std::stringstream::out | std::stringstream::binary);
			boost::array<char, 16 * 1024 > buf;
			std::streamsize compressedSize = 0;
			for (bool done = false; !done;) {
				boost::system::error_code error;
				std::size_t count = boost::asio::read(
						slaveSocket, boost::asio::buffer(buf),
						boost::asio::transfer_all(), error);
				if (error == boost::asio::error::eof)
					done = true;
				else if (error)
					throw boost::system::system_error(error);

				compressedStream.write(buf.c_array(), count);
				compressedSize += count;
			}

			slaveSocket.close();

			// Decopress and merge the film
			serverInfoList[i].numberOfSamplesReceived += film->UpdateFilm(compressedStream);

			ss.str("");
			ss << "Samples received from '" <<
					serverInfoList[i].name << ":" << serverInfoList[i].port << "' (" <<
					(compressedSize / 1024) << " Kbytes)";
			luxError(LUX_NOERROR, LUX_INFO, ss.str().c_str());

			serverInfoList[i].timeLastContact = second_clock::local_time();
		} catch (std::exception& e) {
			ss.str("");
			ss << "Error while communicating with server: " <<
					serverInfoList[i].name << ":" << serverInfoList[i].port;
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());
			luxError(LUX_SYSTEM, LUX_ERROR, e.what());
		}
	}
}

void RenderFarm::sendParams(const ParamSet &params) {
	// Serialize the parameters
	stringstream zos(stringstream::in | stringstream::out | stringstream::binary);
	std::streamsize size;
	{
		stringstream os(stringstream::in | stringstream::out | stringstream::binary);
		{
			// Serialize the parameters
			boost::archive::text_oarchive oa(os);
			oa << params;
		}

		// Compress the parameters
		filtering_streambuf<input> in;
		in.push(gzip_compressor(9));
		in.push(os);
		size = boost::iostreams::copy(in , zos);
	}

	// Write the size of the compressed chunk
	osWriteLittleEndianUInt(isLittleEndian, netBuffer, size);
	// Copy the compressed parameters to the newtwork buffer
	netBuffer << zos.str();
}

void RenderFarm::send(const string &command) {
	netBuffer << command << endl;
}

void RenderFarm::sendFile(std::string file) {
	std::string s;
	std::ifstream in(file.c_str(), std::ios::in | std::ios::binary);

	// Get length of file:
	in.seekg (0, std::ifstream::end);
	// Limiting the file size to 2G should be a problem
	const int len = static_cast<int>(in.tellg());
	in.seekg (0, std::ifstream::beg);

	if (in.fail()) {
		std::stringstream ss;
		ss << "There was an error while checking the size of file '" << file;
		luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

		// Send an empty file ot the slave
		netBuffer << "0\n";
	} else {
		// Allocate a buffer to read all the file
		char *buf = new char[len];

		in.read(buf, len);

		if (in.fail()) {
			std::stringstream ss;
			ss << "There was an error while reading file '" << file << "'";
			luxError(LUX_SYSTEM, LUX_ERROR, ss.str().c_str());

			// Send an empty file to the slave
			netBuffer << "0\n";
		} else {
			// Send the file length
			netBuffer << len << "\n";

			// Send the file
			netBuffer.write(buf, len);
		}

		delete buf;
	}

	in.close();
}

void RenderFarm::send(const std::string &command, const std::string &name,
		const ParamSet &params) {
	try {
		netBuffer << command << endl << name << endl;
		sendParams(params);
		netBuffer << "\n";

		//send the files
		string file;
		file = "";
		file = params.FindOneString(std::string("mapname"), file);
		if (file.size())
			sendFile(file);

		file = "";
		file = params.FindOneString(string("iesname"), file);
		if (file.size())
			sendFile(file);
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, const string &name) {
	try {
		netBuffer << command << endl << name << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, float x, float y, float z) {
	try {
		netBuffer << command << endl << x << ' ' << y << ' ' << z << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, float x, float y) {
	try {
		netBuffer << command << endl << x << ' ' << y << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, float a, float x, float y,
		float z) {
	try {
		netBuffer << command << endl << a << ' ' << x << ' ' << y << ' ' << z << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, float ex, float ey, float ez,
		float lx, float ly, float lz, float ux, float uy, float uz) {
	try {
		netBuffer << command << endl << ex << ' ' << ey << ' ' << ez << ' ' << lx << ' ' << ly << ' ' << lz << ' ' << ux << ' ' << uy << ' ' << uz << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, float tr[16]) {
	try {
		netBuffer << command << endl; //<<x<<' '<<y<<' '<<z<<' ';
		for (int i = 0; i < 16; i++)
			netBuffer << tr[i] << ' ';
		netBuffer << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, const string &name,
		const string &type, const string &texname, const ParamSet &params) {
	try {
		netBuffer << command << endl << name << endl << type << endl << texname << endl;
		sendParams(params);
		netBuffer << "\n";

		//send the file
		std::string file = "";
		file = params.FindOneString(std::string("filename"), file);
		if (file.size())
			sendFile(file);
	} catch (std::exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

void RenderFarm::send(const string &command, const string &name, float a, float b, const string &transform) {
	try {
		netBuffer << command << endl << name << ' ' << a << ' ' << b << ' ' << transform << endl;
	} catch (exception& e) {
		luxError(LUX_SYSTEM, LUX_ERROR, e.what());
	}
}

u_int RenderFarm::getServersStatus(RenderingServerInfo *info, u_int maxInfoCount) {
	ptime now = second_clock::local_time();
	for (size_t i = 0; i < min<size_t>(serverInfoList.size(), maxInfoCount); ++i) {
		info[i].serverIndex = i;
		info[i].name = serverInfoList[i].name.c_str();
		info[i].port = serverInfoList[i].port.c_str();
		info[i].sid = serverInfoList[i].sid.c_str();

		time_duration td = now - serverInfoList[i].timeLastContact;
		info[i].secsSinceLastContact = td.seconds();
		info[i].numberOfSamplesReceived = serverInfoList[i].numberOfSamplesReceived;
	}

	return serverInfoList.size();
}
