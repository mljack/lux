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

#define NDEBUG 1

#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <exception>
#include <iostream>

#include "api.h"
#include "error.h"
#include "server/renderserver.h"

#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#if defined(WIN32) && !defined(__CYGWIN__) /* We need the following two to set stdout to binary */
#include <io.h>
#include <fcntl.h>
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
#include "direct.h"
#define chdir _chdir
#endif

using namespace lux;
namespace po = boost::program_options;

std::string sceneFileName;
int threads;
bool parseError;
RenderServer *renderServer;

void engineThread() {
	// NOTE - lordcrc - initialize rand()
	srand(time(NULL));

    luxParse(sceneFileName.c_str());
    if (luxStatistics("sceneIsReady") == 0.)
        parseError = true;
}

void infoThread() {
	std::vector<char> buf(1 << 16, '\0');
	while (!boost::this_thread::interruption_requested()) {
		try {
			boost::this_thread::sleep(boost::posix_time::seconds(5));

			luxUpdateStatisticsWindow();
			luxGetStringAttribute("renderer_statistics_formatted_short", "_recommended_string", &buf[0], static_cast<unsigned int>(buf.size()));
			LOG(LUX_INFO,LUX_NOERROR) << std::string(buf.begin(), buf.end());
		} catch(boost::thread_interrupted&) {
			break;
		}
	}
}

void addNetworkSlavesThread(std::vector<std::string> slaves) {
	for (std::vector<std::string>::iterator i = slaves.begin(); i < slaves.end(); i++) {
		try {
			if (boost::this_thread::interruption_requested())
				break;
			luxAddServer((*i).c_str());
		} catch(boost::thread_interrupted&) {
			break;
		}
	}
}

LuxErrorHandler prevErrorHandler;

void serverErrorHandler(int code, int severity, const char *msg) {
	if (renderServer)
		renderServer->errorHandler(code, severity, msg);

	prevErrorHandler(code, severity, msg);
}

int main(int ac, char *av[]) {
	// Dade - initialize rand() number generator
	srand(time(NULL));

	luxInit();

	try {
		// Declare a group of options that will be
		// allowed only on command line
		po::options_description generic("Generic options");
		generic.add_options()
				("version,v", "Print version string")
				("help,h", "Produce help message")
				("resume,r", po::value< std::string >()->implicit_value(""), "Resume from FLM")
				("overrideresume,R", po::value< std::string >(), "Resume from specified FLM")
				("output,o", po::value< std::string >(), "Output filename")
				("server,s", "Launch in server mode")
				("resetserver", po::value< std::vector<std::string> >()->composing(), "Force the specified rendering server to reset")
				("bindump,b", "Dump binary RGB framebuffer to stdout when finished")
				("debug,d", "Enable debug mode")
				("fixedseed,f", "Disable random seed mode")
				("minepsilon,e", po::value< float >(), "Set minimum epsilon")
				("maxepsilon,E", po::value< float >(), "Set maximum epsilon")
				("verbose,V", "Increase output verbosity (show DEBUG messages)")
				("quiet,q", "Reduce output verbosity (hide INFO messages)")
				("very-quiet,x", "Reduce output verbosity even more (hide WARNING messages)")
				("configfile,C", po::value< std::string >(), "Specify the configuration file to use")
				;

		// Declare a group of options that will be
		// allowed both on command line and in
		// config file
		po::options_description config("Configuration");
		config.add_options()
				("threads,t", po::value < int >(), "Specify the number of threads that Lux will run in parallel.")
				("useserver,u", po::value< std::vector<std::string> >()->composing(), "Specify the address of a rendering server to use.")
				("serverinterval,i", po::value < int >(), "Specify the number of seconds between requests to rendering servers.")
				("serverport,p", po::value < int >(), "Specify the tcp port used in server mode.")
				("serverwriteflm,W", "Write film to disk before transmitting in server mode.")
				("password,P", po::value< std::string >()->default_value(""), "Specify the servers reset password.")
				("cachedir,c", po::value< std::string >(), "Specify the cache directory to use")
				;

		// Hidden options, will be allowed both on command line and
		// in config file, but will not be shown to the user.
		po::options_description hidden("Hidden options");
		hidden.add_options()
				("input-file", po::value< vector<string> >(), "input file")
				("test", "debug test mode")
				;

		po::options_description cmdline_options;
		cmdline_options.add(generic).add(config).add(hidden);

		po::options_description config_file_options;
		config_file_options.add(config).add(hidden);

		po::options_description visible("Allowed options");
		visible.add(generic).add(config);

		po::positional_options_description p;

		p.add("input-file", -1);

		po::variables_map vm;
		// disable guessing of option names
		int cmdstyle = po::command_line_style::default_style & ~po::command_line_style::allow_guessing;
		store(po::command_line_parser(ac, av).
			style(cmdstyle).options(cmdline_options).positional(p).run(), vm);

		std::string configFile("luxconsole.cfg");
		if (vm.count("configfile"))
			configFile = vm["configfile"].as<std::string>();
		std::ifstream ifs(configFile.c_str());
		store(parse_config_file(ifs, config_file_options), vm);
		notify(vm);

		if (vm.empty()) {
			LOG(LUX_ERROR,LUX_SYSTEM)<< "Usage: luxconsole [options] file...\n" << visible;
			return 0;
		}
		
		if (vm.count("help")) {
			LOG(LUX_INFO,LUX_NOERROR)<< "Usage: luxconsole [options] file...\n" << visible;
			return 0;
		}

		LOG(LUX_INFO,LUX_NOERROR) << "Lux version " << luxVersion() << " of " << __DATE__ << " at " << __TIME__;
		
		if (vm.count("version"))
			return 0;

		if (vm.count("verbose")) {
			luxErrorFilter(LUX_DEBUG);
		}

		if (vm.count("quiet")) {
			luxErrorFilter(LUX_WARNING);
		}

		if (vm.count("very-quiet")) {
			luxErrorFilter(LUX_ERROR);
		}

		if (vm.count("resetserver")) {
			std::vector<std::string> slaves = vm["resetserver"].as<std::vector<std::string> >();
			std::string password = vm["password"].as<std::string>();
			for (std::vector<std::string>::iterator i = slaves.begin(); i < slaves.end(); i++) {
				luxResetServer((*i).c_str(), password.c_str());
			}
			return 0;
		}

		if (vm.count("threads"))
			threads = vm["threads"].as<int>();
		else {
			// Dade - check for the hardware concurrency available
			threads = boost::thread::hardware_concurrency();
			if (threads == 0)
				threads = 1;
		}

		LOG(LUX_INFO,LUX_NOERROR) << "Threads: " << threads;

		if (vm.count("debug")) {
			LOG(LUX_INFO,LUX_NOERROR)<< "Debug mode enabled";
			luxEnableDebugMode();
		}

		if (vm.count("fixedseed")) {
			if (!vm.count("server"))
				luxDisableRandomMode();
			else // Slaves should always have a different seed than the master
				LOG(LUX_WARNING,LUX_CONSISTENCY)<< "Using random seed for server";
		}

		int serverInterval;
		if (vm.count("serverinterval")) {
			serverInterval = vm["serverinterval"].as<int>();
			luxSetIntAttribute("render_farm", "pollingInterval", serverInterval);
		} else
			serverInterval = luxGetIntAttribute("render_farm", "pollingInterval");

		std::vector<std::string> slaves;
		if (vm.count("useserver")) {
			// add slaves later, so we can start rendering first
			slaves = vm["useserver"].as<std::vector<std::string> >();

			LOG(LUX_INFO,LUX_NOERROR) << "Server request interval: " << serverInterval << " secs";
		}

		int serverPort = luxGetIntAttribute("render_farm", "defaultTcpPort");
		if (vm.count("serverport"))
			serverPort = vm["serverport"].as<int>();

		// Any call to Lux API must be done _after_ luxAddServer
		if (vm.count("minepsilon")) {
			const float mine = vm["minepsilon"].as<float>();
			if (vm.count("maxepsilon")) {
				const float maxe = vm["maxepsilon"].as<float>();
				luxSetEpsilon(mine, maxe);
			} else
				luxSetEpsilon(mine, -1.f);
		} else {
			if (vm.count("maxepsilon")) {
				const float maxe = vm["maxepsilon"].as<float>();
				luxSetEpsilon(-1.f, maxe);
			} else
				luxSetEpsilon(-1.f, -1.f);
		}

		if (vm.count("resume")) {
			luxOverrideResumeFLM("");
		}

		if (vm.count("overrideresume")) {
			std::string resumefile = vm["overrideresume"].as<std::string>();

			boost::filesystem::path resumePath(resumefile);
			if (!boost::filesystem::exists(resumePath)) {
				LOG(LUX_WARNING,LUX_NOFILE) << "Could not find resume file '" << resumefile << "', using filename in scene";
				resumefile = "";
			}

			luxOverrideResumeFLM(resumefile.c_str());
		}

		if (vm.count("output")) {
			std::string outputfile = vm["output"].as<std::string>();

			boost::filesystem::path outputPath(outputfile);
			if (!boost::filesystem::exists(outputPath.parent_path())) {
				LOG(LUX_WARNING,LUX_NOFILE) << "Could not find output path '" << outputPath.parent_path() << "', using filename in scene";
			} else
				luxOverrideFilename(outputfile.c_str());
		}

		if (vm.count("input-file")) {
			std::vector<std::string> v = vm["input-file"].as < vector<string> > ();

			// Resolve relative paths before changing directories
			for (unsigned int i = 0; i < v.size(); i++)
				if (v[i] != "-")
					v[i] = boost::filesystem::system_complete(v[i]).string();

			for (unsigned int i = 0; i < v.size(); i++) {
				//change the working directory
				boost::filesystem::path fullPath(v[i]);

				if (v[i] != "-") {
					if (!boost::filesystem::exists(fullPath)) {
						LOG(LUX_SEVERE,LUX_NOFILE) << "Unable to open scenefile '" << fullPath.string() << "'";
						continue;
					}

					if (chdir(fullPath.parent_path().string().c_str())) {
						LOG(LUX_SEVERE,LUX_NOFILE) << "Unable to go into directory '" << fullPath.parent_path().string() << "'";
					}
				}

				sceneFileName = fullPath.filename().string();
				parseError = false;
				boost::thread engine(&engineThread);

				// add slaves, need to do this for each scene file
				boost::thread addSlaves(boost::bind(addNetworkSlavesThread, slaves));

				//wait the scene parsing to finish
				while (!luxStatistics("sceneIsReady") && !parseError) {
					boost::this_thread::sleep(boost::posix_time::seconds(1));
				}

				if (parseError) {
					LOG(LUX_SEVERE,LUX_BADFILE) << "Skipping invalid scenefile '" << fullPath.string() << "'";
					continue;
				}

				//add rendering threads
				int threadsToAdd = threads;
				while (--threadsToAdd)
					luxAddThread();

				//launch info printing thread
				boost::thread info(&infoThread);

				// Dade - wait for the end of the rendering
				luxWait();

				// We have to stop the info thread before to call luxExit()/luxCleanup()
				info.interrupt();
				// Stop adding slaves before proceeding
				addSlaves.interrupt();

				info.join();
				addSlaves.join();

				luxExit();

				// Dade - print the total rendering time
				boost::posix_time::time_duration td(0, 0,
						(int) luxStatistics("secElapsed"), 0);

				LOG(LUX_INFO,LUX_NOERROR) << "100% rendering done [" << threads << " threads] " << td;

				if (vm.count("bindump")) {
					// Get pointer to framebuffer data if needed
					unsigned char* fb = luxFramebuffer();

					int w = luxGetIntAttribute("film", "xPixelCount");
					int h = luxGetIntAttribute("film", "yPixelCount");
					luxUpdateFramebuffer();

#if defined(WIN32) && !defined(__CYGWIN__) /* On WIN32 we need to set stdout to binary */
					_setmode(_fileno(stdout), _O_BINARY);
#endif

					// Dump RGB imagebuffer data to stdout
					for (int i = 0; i < w * h * 3; i++)
						std::cout << fb[i];
				}

				luxCleanup();
			}
		} else if (vm.count("server")) {
			bool writeFlmFile = vm.count("serverwriteflm") != 0;

			std::string cachedir;
			if (vm.count("cachedir")) {
				cachedir = vm["cachedir"].as<std::string>();
				boost::filesystem::path cachePath(cachedir);
				try {
					if (!boost::filesystem::is_directory(cachePath))
						boost::filesystem::create_directory(cachePath);

					boost::filesystem::current_path(cachePath);
				} catch (std::exception &e) {
					LOG(LUX_SEVERE,LUX_NOFILE) << "Unable to use cache directory '" << cachedir << "': " << e.what();
					return 1;
				}
				LOG(LUX_INFO,LUX_NOERROR) << "Using cache directory '" << cachedir << "'";
			}

			std::string password = vm["password"].as<std::string>();
			renderServer = new RenderServer(threads, password, serverPort, writeFlmFile);

			prevErrorHandler = luxError;
			luxErrorHandler(serverErrorHandler);

			renderServer->start();

			// add slaves, no need to do this in a separate thread since we're just waiting for
			// the server to finish
			for (std::vector<std::string>::iterator i = slaves.begin(); i < slaves.end(); i++)
				luxAddServer((*i).c_str());

			renderServer->join();
			delete renderServer;
		} else {
			LOG(LUX_ERROR,LUX_SYSTEM) << "luxconsole: no input file";
		}

	} catch (std::exception & e) {
		LOG(LUX_SEVERE,LUX_SYNTAX) << "Command line argument parsing failed with error '" << e.what() << "', please use the --help option to view the allowed syntax.";
		return 1;
	}
	return 0;
}
