#!/usr/bin/env python3
# -*- coding: utf8 -*-
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Authors:
# Doug Hammond
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# ***** END GPL LICENCE BLOCK *****
#
#------------------------------------------------------------------------------ 
# System imports
#------------------------------------------------------------------------------ 
import optparse, datetime, multiprocessing, os, signal, sys, threading, time

#------------------------------------------------------------------------------ 
# Lux imports
#------------------------------------------------------------------------------ 
try:
	import pylux
except ImportError as err:
	print('This script requires pylux: %s'%err)
	sys.exit()

#------------------------------------------------------------------------------
# Class Definitions
#------------------------------------------------------------------------------ 

class TimerThread(threading.Thread):
	'''
	Periodically call self.kick()
	'''
	STARTUP_DELAY = 0
	KICK_PERIOD = 8
	
	active = True
	timer = None
	
	LocalStorage = None
	
	def __init__(self, LocalStorage=dict()):
		threading.Thread.__init__(self)
		self.LocalStorage = LocalStorage
	
	def set_kick_period(self, period):
		self.KICK_PERIOD = period + self.STARTUP_DELAY
	
	def stop(self):
		self.active = False
		if self.timer is not None:
			self.timer.cancel()
			
	def run(self):
		'''
		Timed Thread loop
		'''
		
		while self.active:
			self.timer = threading.Timer(self.KICK_PERIOD, self.kick_caller)
			self.timer.start()
			if self.timer.isAlive(): self.timer.join()
	
	def kick_caller(self):
		if self.STARTUP_DELAY > 0:
			self.KICK_PERIOD -= self.STARTUP_DELAY
			self.STARTUP_DELAY = 0
		
		self.kick()
	
	def kick(self):
		'''
		sub-classes do their work here
		'''
		pass

def format_elapsed_time(t):
	'''
	Format a duration in seconds as an HH:MM:SS format time
	'''
	
	td = datetime.timedelta(seconds=t)
	min = td.days*1440  + td.seconds/60.0
	hrs = td.days*24	+ td.seconds/3600.0
	
	return '%i:%02i:%02i' % (hrs, min%60, td.seconds%60)

class LuxAPIStats(TimerThread):
	'''
	Periodically get lux stats
	'''
	
	KICK_PERIOD = 0.5
	
	stats_string = ''
	
	px = 0
	secelapsed = 0
	localsamples = 0
	netsamples = 0
	
	def stop(self):
		self.active = False
		if self.timer is not None:
			self.timer.cancel()
			
	def kick(self):
		ctx = self.LocalStorage['lux_context']
		
		if self.px == 0:
			self.px = ctx.getAttribute('film', 'xResolution') * ctx.getAttribute('film', 'yResolution')
		
		self.secelapsed = ctx.statistics('secElapsed')
		self.localsamples = ctx.getAttribute('film', 'numberOfLocalSamples')
		
		self.stats_string  = '%s [Local: %0.2f S/Px - %0.2f S/Sec]' % (
			format_elapsed_time(self.secelapsed),
			self.localsamples / self.px,
			self.localsamples / self.secelapsed
		)
		
		network_servers = ctx.getServerCount()
		if network_servers > 0:
			self.netsamples = ctx.getAttribute('film', 'numberOfSamplesFromNetwork')
			self.stats_string += ' [Net (%i): %0.2f S/Px - %0.2f S/Sec]' % (
				network_servers,
				self.netsamples / self.px,
				self.netsamples / self.secelapsed
			)

class RenderEndException(Exception):
	pass

class RenderSkipException(Exception):
	pass

class luxconsole(object):
	
	# SIGBREAK only on windows and SIGHUP only on *nix
	SIGSKIP = signal.SIGBREAK if 'SIGBREAK' in dir(signal) else signal.SIGHUP
	
	stop_queue = False
	stats_thread = None
	
	@staticmethod
	def log(str, module_name='Lux'):
		print("[%s %s] %s" % (module_name, time.strftime('%Y-%b-%d %H:%M:%S'), str))
	
	@classmethod
	def set_interrupt(cls, sig, frame):
		if sig == cls.SIGSKIP:
			# Move on to the next queued file
			raise RenderSkipException('Caught signal %s'%sig)
		else:
			# Stop all rendering
			raise RenderEndException('Caught signal %s'%sig)
	
	@classmethod
	def print_stats(cls):
		if cls.stats_thread is not None:
			cls.stats_thread.kick()
			cls.log(cls.stats_thread.stats_string)
	
	@staticmethod
	def context_factory(options, name):
		ctx = pylux.Context(name)
		
		if options.debug:
			ctx.enableDebugMode()
		
		if options.fixedseed and not options.server:
			ctx.disableRandomMode()
		
		if options.serverinterval is not None:
			ctx.setNetworkServerUpdateInterval(options.serverinterval)
		
		if options.useserver is not None:
			for srv in options.useserver:
				ctx.addServer(srv)
		
		return ctx
	
if __name__ == '__main__':
	# Set up command line options
	parser = optparse.OptionParser(
		description = 'LuxRender console/command line interface',
		usage = 'usage: %prog [options] <scene file> [<scene file> ...]',
		version = '%%prog version %s'%pylux.version()
	)
	# Generic
	parser.add_option(
		'-V', '--verbose',
		action = 'store_true', default = False,
		dest = 'verbose',
		help = 'Increase output verbosity (show DEBUG messages)'
	)
	parser.add_option(
		'-q', '--quiet',
		action = 'store_true', default = False,
		dest = 'quiet',
		help = 'Reduce output verbosity (hide INFO messages)'
	)
	parser.add_option(
		'-d', '--debug',
		action = 'store_true', default = False,
		dest = 'debug',
		help = 'Enable debug mode'
	)
	parser.add_option(
		'-f', '--fixedseed',
		action = 'store_true', default = False,
		dest = 'fixedseed',
		help = 'Disable random seed mode'
	)
	
	# TODO add Epsilon settings
	
	# Server mode
	parser.add_option(
		'-s', '--server',
		action = 'store_true', default = False,
		dest = 'server',
		help = 'Launch in server mode'
	)
	parser.add_option(
		'-p', '--serverport',
		metavar = 'P', type = int,
		dest = 'serverport',
		help = 'Specify the tcp port used in server mode'
	)
	parser.add_option(
		'-W', '--serverwriteflm',
		action = 'store_true', default = False,
		dest = 'serverwriteflm',
		help = 'Write film to disk before transmitting in server mode'
	)
	# Client mode
	parser.add_option(
		'-u', '--useserver',
		metavar = 'ADDR', type = str,
		action = 'append',
		dest = 'useserver',
		help = 'Specify the address of a rendering server to use'
	)
	parser.add_option(
		'-i', '--serverinterval',
		metavar = 'S', type = int,
		dest = 'serverinterval',
		help = 'Specify the number of seconds between requests to rendering servers'
	)
	# Rendering options
	parser.add_option(
		'-t', '--threads',
		metavar = 'T', type = int,
		dest = 'threads',
		help = 'Number of rendering threads'
	)
	
	(options, args) = parser.parse_args()
	
	if len(sys.argv) < 2:
		parser.print_help()
		sys.exit()
	
	# luxErrorFilter is not exposed in pylux yet, so verbose and quiet cannot be set
	if options.verbose or options.quiet:
		print('--verbose and --quiet options are not yet implemented, ignoring.')
	
	if options.server:
		print('--server mode not yet implemented, ignoring.')
	
	#print(options)
	#print(args)
	
	if len(args) < 1:
		luxconsole.log('No files to render!')
		sys.exit()
	
	if options.threads is None:
		threads = multiprocessing.cpu_count()
	else:
		threads = options.threads
	
	# External signal handlers
	signal.signal(signal.SIGINT,		luxconsole.set_interrupt)
	signal.signal(signal.SIGTERM,		luxconsole.set_interrupt)
	signal.signal(luxconsole.SIGSKIP,	luxconsole.set_interrupt)
	
	# All options are now set, lets start rendering... !
	for scene_file in args:
		if not os.path.exists(scene_file):
			luxconsole.log('Scene file to render "%s" does not exist, skipping.'%scene_file)
			continue
		
		# Create and (later) delete a context for each render
		ctx = luxconsole.context_factory(options, scene_file)
		
		try:
			aborted = False
			
			scene_path = os.path.dirname(scene_file)
			if scene_path !='':
				os.chdir(scene_path)
			
			luxconsole.stats_thread = LuxAPIStats({
				'lux_context': ctx
			})
			
			ctx.parse(scene_file, True) # asynchronous parse (ie. don't wait)
			
			# wait here for parsing to complete
			while ctx.statistics('sceneIsReady') != 1.0:
				time.sleep(0.3)
				if not ctx.parseSuccessful():
					raise RenderSkipException('Skipping bad scene file')
			
			# Get the resolution for use later
			xres = ctx.getAttribute('film', 'xResolution')
			yres = ctx.getAttribute('film', 'yResolution')
			
			# TODO: add support to pylux for reporting parse errors after async parse
			
			for i in range(threads-1):
				ctx.addThread()
			
			# Render wait loop
			while ctx.statistics('filmIsReady') != 1.0 and \
				  ctx.statistics('terminated') != 1.0 and \
				  ctx.statistics('enoughSamples') != 1.0:
				time.sleep(5)
				luxconsole.print_stats()
		except RenderSkipException as StopReason:
			luxconsole.log('Stopping this render... (%s)' % StopReason)
			# continue
			aborted = True
		except RenderEndException as StopReason:
			luxconsole.log('Stopping all rendering... (%s)' % StopReason)
			aborted = True
			luxconsole.stop_queue = True
		
		ctx.exit()
		ctx.wait()
		
#		if not aborted:
#			# Calculate actual overall render speed (network inclusive)
#			sec = luxconsole.stats_thread.secelapsed
#			samples = luxconsole.stats_thread.localsamples + luxconsole.stats_thread.netsamples
#			luxconsole.log(
#				'Image rendered to %0.2f S/Px after %s at %0.2f Samples/Sec' % (
#					samples/luxconsole.stats_thread.px,
#					format_elapsed_time(sec),
#					samples/sec
#				)
#			)
		
		ctx.cleanup()
		
		# deleting the context will disconnect all connected servers and free memory
		del ctx
		
		if luxconsole.stop_queue:
			break
