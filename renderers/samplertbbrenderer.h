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

#ifndef LUX_SAMPLERTBBRENDERER_H
#define LUX_SAMPLERTBBRENDERER_H

#include <vector>
#include <boost/thread.hpp>

#include "lux.h"
#include "renderer.h"
#include "fastmutex.h"
#include "timer.h"
#include "dynload.h"
#include <tbb/tbb.h>

namespace lux
{

class SamplerTBBRenderer;
class SRTBBHostDescription;

template<class T>
class Pool
{
	public:
		Pool(){}

		T* Get()
		{
			fast_mutex::scoped_lock lockStats(lock);
		
			if(data.size() > 0)
			{
				T* res = data.back();	
				data.pop_back();
				return res;
			}
			return NULL;
		}	

		void Release(T* t)
		{
			fast_mutex::scoped_lock lockStats(lock);
			data.push_back(t);
		}
	private:
		std::vector<T*> data;
		fast_mutex lock;
};

//------------------------------------------------------------------------------
// SRTBBDeviceDescription
//------------------------------------------------------------------------------

class SRTBBDeviceDescription : protected RendererDeviceDescription {
public:
	const string &GetName() const { return name; }

	unsigned int GetAvailableUnitsCount() const {
		return 1u; // not implemented with TBB
	}
	unsigned int GetUsedUnitsCount() const;
	void SetUsedUnitsCount(const unsigned int units);

	friend class SamplerTBBRenderer;
	friend class SRTBBHostDescription;

private:
	SRTBBDeviceDescription(SRTBBHostDescription *h, const string &n) :
		host(h), name(n) { }
	~SRTBBDeviceDescription() { }

	SRTBBHostDescription *host;
	string name;
};

//------------------------------------------------------------------------------
// SRTBBHostDescription
//------------------------------------------------------------------------------

class SRTBBHostDescription : protected RendererHostDescription {
public:
	const string &GetName() const { return name; }

	vector<RendererDeviceDescription *> &GetDeviceDescs() { return devs; }

	friend class SamplerTBBRenderer;
	friend class SRTBBDeviceDescription;

private:
	SRTBBHostDescription(SamplerTBBRenderer *r, const string &n);
	~SRTBBHostDescription();

	SamplerTBBRenderer *renderer;
	string name;
	vector<RendererDeviceDescription *> devs;
};

//------------------------------------------------------------------------------
// SamplerTBBRenderer
//------------------------------------------------------------------------------

class SamplerTBBRenderer : public Renderer {
public:
	SamplerTBBRenderer();
	~SamplerTBBRenderer();

	RendererType GetType() const;

	RendererState GetState() const;
	vector<RendererHostDescription *> &GetHostDescs();
	void SuspendWhenDone(bool v);

	void Render(Scene *scene);

	void Pause();
	void Resume();
	void Terminate();

	friend class SRTBBDeviceDescription;
	friend class SRTBBHostDescription;
	friend class SRStatistics;

	static Renderer *CreateRenderer(const ParamSet &params);
	void RenderImpl(tbb::blocked_range<u_int> const & range);

private:
	//--------------------------------------------------------------------------
	// RenderThread
	//--------------------------------------------------------------------------

	class RenderThread : public boost::noncopyable {
	public:
		RenderThread(u_int index, SamplerTBBRenderer *renderer);
		~RenderThread();


		u_int  n;
		SamplerTBBRenderer *renderer;
		boost::thread *thread; // keep pointer to delete the thread object
		double samples, blackSamples;
		fast_mutex statLock;
	};

	void CreateRenderThread();
	void RemoveRenderThread();

	//--------------------------------------------------------------------------

	mutable boost::mutex classWideMutex;
	mutable boost::mutex renderThreadsMutex;

	RendererState state;
	vector<RendererHostDescription *> hosts;
	vector<RenderThread *> renderThreads;
	Scene *scene;

	fast_mutex sampPosMutex;
	u_int sampPos;

	// Put them last for better data alignment
	// used to suspend render threads until the preprocessing phase is done
	bool preprocessDone;
	bool suspendThreadsWhenDone;

	Pool<Sample> samplePool;
};

}//namespace lux

#endif // LUX_SAMPLERRENDERER_H
