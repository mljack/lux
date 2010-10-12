// Python dosctrings for pylux Module
// Try to keep within 80 columns
// ref: http://epydoc.sourceforge.net/manual-epytext.html
// ref: https://renderman.pixar.com/products/rispec/rispec_pdf/RISpec3_2.pdf
// ref: http://www.luxrender.net/wiki/index.php?title=Scene_file_format
// -----------------------------------------------------------------------------

const char * ds_pylux =
"LuxRender Python Bindings\n\n"
"Provides access to the LuxRender API in python\n\n"
"TODO: Docstrings marked (+) need verification.";

const char * ds_pylux_version =
"Returns the pylux/LuxRender version";

const char * ds_pylux_ThreadSignals =
"Valid states for rendering threads";

const char * ds_pylux_RenderingThreadInfo =
"Container class for information about rendering threads";

const char * ds_pylux_luxComponent =
"(+) LuxRender Components available to modify at render-time";

const char * ds_pylux_luxComponentParameters =
"(+) Parameters of luxComponents available to modify at render time";

const char * ds_pylux_RenderingServerInfo =
"(+) Container class for information about rendering servers";

const char * ds_pylux_errorHandler =
"Specify an alternate error handler (logging) function for LuxRender engine\n"
"output. By default the render engine will print to stdout";

const char * ds_pylux_RenderServerState =
"Valid states for LuxRender Network Server";

const char * ds_pylux_RenderServer =
"An instance of a LuxRender Network Server";

const char * ds_pylux_RenderServer_getServerPort =
"Get the port that this server is listening on";

const char * ds_pylux_RenderServer_getServerState =
"Get the state of this server";

const char * ds_pylux_RenderServer_start = 
"Start this server";

const char * ds_pylux_RenderServer_stop =
"Stop this server";

const char * ds_pylux_RenderServer_join =
"Join this server thread";

const char * ds_pylux_Context =
"An instance of a LuxRender rendering context";

const char * ds_pylux_Context_init =
"Create a new rendering Context object with the given name";

const char * ds_pylux_Context_accelerator =
"Initialise geometry acceleration structure type. Valid types are\n"
"- bruteforce\n"
"- bvh\n"
"- grid\n"
"- qbvh\n"
"- tabreckdtree (or kdtree)";

const char * ds_pylux_Context_addServer =
"Add a (remote) rendering server to the context";

const char * ds_pylux_Context_addThread =
"Add a local rendering thread to the context";

const char * ds_pylux_Context_areaLightSource =
"Attach a light source to the current geometry definition. (See: RiSpec 3.2 p.43)";

const char * ds_pylux_Context_attributeBegin =
"Begin a new Attribute scope, inheriting attributes from parent scope.\n"
"(See: RiSpec 3.2 p.38)";

const char * ds_pylux_Context_attributeEnd =
"End an Attribute scope, reverting attributes to parent scope.\n"
"(See: RiSpec 3.2 p.38)";

const char * ds_pylux_Context_camera =
"Add a camera to the scene. Valid types are:\n"
"- environment\n"
"- orthographic\n"
"- perspective\n"
"- realistic";

const char * ds_pylux_Context_cleanup =
"Clean up and reset the renderer state after rendering.";

const char * ds_pylux_Context_concatTransform =
"Concatenate the given transformation onto the current transformation. The\n"
"transformation is applied before all previously applied transformations, that\n"
"is, before the current transformation. (Ref: RiSpec 3.2 p.56)";

const char * ds_pylux_Context_coordSysTransform =
"Replaces the current transformation matrix with the matrix that forms the named\n"
"coordinate system. This permits objects to be placed directly into special or\n"
"user-defined coordinate systems by their names. (Ref: RiSpec 3.2 p.57)";

const char * ds_pylux_Context_coordinateSystem =
"This function marks the coordinate system defined by the current transformation\n"
"with the name space and saves it. (Ref: RiSpec 3.2 p.58)";

const char * ds_pylux_Context_disableRandomMode =
"Disables random mode in the renderer core.";

const char * ds_pylux_Context_enableDebugMode =
"Puts the renderer core into Debug mode.";

const char * ds_pylux_Context_exit =
"Stop the current rendering.";

const char * ds_pylux_Context_exterior =
"Sets the current Exterior volume shader. (Ref: RiSpec 3.2 p.48)";

const char * ds_pylux_Context_film =
"Initialise the Film for rendering. Valid types are:\n"
"- fleximage.";

const char * ds_pylux_Context_framebuffer =
"Returns the current post-processed LDR framebuffer in RGB888 format as a list.\n"
"It is advisable to call updateFramebuffer() before calling this function.";

const char * ds_pylux_Context_getDefaultParameterValue =
"";

const char * ds_pylux_Context_getDefaultStringParameterValue =
"";

const char * ds_pylux_Context_getHistogramImage =
"";

const char * ds_pylux_Context_getNetworkServerUpdateInterval =
"";

const char * ds_pylux_Context_getOption =
"";

const char * ds_pylux_Context_getOptions =
"";

const char * ds_pylux_Context_getParameterValue =
"";

const char * ds_pylux_Context_getRenderingServersStatus =
"Returns a list of (address, port) tuples of currently connected servers.";

const char * ds_pylux_Context_getRenderingThreadsStatus =
"";

const char * ds_pylux_Context_getServerCount =
"Return the number of remote slaves contributing towards the current rendering.";

const char * ds_pylux_Context_getStringParameterValue =
"";

const char * ds_pylux_Context_identity =
"Set the current transform to the identity matrix.";

const char * ds_pylux_Context_interior =
"Set the current Interior volume shader. (Ref: RiSpec 3.2 p.47)";

const char * ds_pylux_Context_lightGroup =
"Change the named group of all following light sources in this scope.";

const char * ds_pylux_Context_lightSource =
"Create a light source of the specified type. Valid types are:\n"
"- area\n"
"- distant\n"
"- goniometric\n"
"- infinite\n"
"- point\n"
"- projection\n"
"- sky\n"
"- sun\n"
"- sunsky (shortcut for sun + sky)\n"
"- spot";

const char * ds_pylux_Context_loadFLM =
"Load an FLM film file into the context for tonemapping/post-processing purposes.";

const char * ds_pylux_Context_lookAt =
"Specify the position, target and up vector of the scene's camera.";

const char * ds_pylux_Context_makeNamedMaterial =
"Define a named material with the given name and type. See also material().";

const char * ds_pylux_Context_makeNamedVolume =
"Define a named volume with the given name and type. Not to be confused with the\n"
"volume() system.";

const char * ds_pylux_Context_material =
"Define a material of the given type in the current scope. Valid types are:\n"
"- carpaint\n"
"- glass\n"
"- glass2\n"
"- glossy_lossy\n"
"- glossy\n"
"- matte\n"
"- mattetranslucent\n"
"- metal\n"
"- mirror\n"
"- mix\n"
"- null\n"
"- roughglass\n"
"- shinymetal";

const char * ds_pylux_Context_motionInstance =
"Instantiates a moving object. See also objectInstance(). (Sim.: RiSpec 3.2 p.96).";

const char * ds_pylux_Context_namedMaterial =
"Instantiate a named material in the current scope as if it were defined with\n"
"material().";

const char * ds_pylux_Context_objectBegin =
"Define a named object that can be re-used with objectInstance().\n"
"(Ref: RiSpec 3.2 p.94).";

const char * ds_pylux_Context_objectEnd =
"End a named object definition.";

const char * ds_pylux_Context_objectInstance =
"Instatiate a named object defined within an objectBegin()/objectEnd() pair in\n"
"the current scope. (Ref: RiSpec 3.2 p.95)";

const char * ds_pylux_Context_overrideResumeFLM =
"";

const char * ds_pylux_Context_parse =
"Parse the given filename. If done asynchronously, control will pass\n"
"immediately back to the python interpreter, otherwise this function blocks.";

const char * ds_pylux_Context_pause =
"(+) Pause all local rendering threads.";

const char * ds_pylux_Context_renderer =
"Choose the internal Renderer type. Valid types are:\n"
"- sampler (traditional CPU renderer)\n"
"- hybrid (combined CPU+GPU renderer)";

const char * ds_pylux_Context_pixelFilter =
"Initialise the pixel filter to use for rendering. Valid types are:\n"
"- box\n"
"- triangle\n"
"- gaussian\n"
"- mitchell\n"
"- sinc";

const char * ds_pylux_Context_portalShape =
"Define geometry as an \"Exit Portal\" shape. See shape().";

const char * ds_pylux_Context_removeServer =
"Remote a remote rendering slave from the current rendering process.";

const char * ds_pylux_Context_removeThread =
"Remove a local rendering thread from the current rendering process.";

const char * ds_pylux_Context_reverseOrientation =
"Causes the orientation in the current scope to be reversed left->right or\n"
"right->left handed. (Ref: RiSpec 3.2 p.53).";

const char * ds_pylux_Context_rotate =
"Concatenate a rotation of the given angle, about the given axis onto the current\n"
"scope's transformation. (Ref: RiSpec 3.2 p.57)";

const char * ds_pylux_Context_sampler =
"Initialise the sampler to use for rendering. Valid types are:\n"
"- random\n"
"- lowdiscrepancy\n"
"- metropolis\n"
"- erpt";

const char * ds_pylux_Context_saveFLM =
"Save the current virtual film to an FLM file.";

const char * ds_pylux_Context_scale =
"Concatenate a scaling onto the current scope's transformation.\n"
"(Ref: RiSpec 3.2 p.57)";

const char * ds_pylux_Context_setEpsilon =
"";

const char * ds_pylux_Context_setHaltSamplePerPixel =
"";

const char * ds_pylux_Context_setNetworkServerUpdateInterval =
"";

const char * ds_pylux_Context_setOption =
"";

const char * ds_pylux_Context_setParameterValue =
"";

const char * ds_pylux_Context_setStringParameterValue =
"";

const char * ds_pylux_Context_shape =
"Define a geometry primitive in the current scope. Valid types are:\n"
"- cone\n"
"- cylinder\n"
"- disk\n"
"- heightfield\n"
"- lenscomponent\n"
"- loopsubdiv\n"
"- nurbs\n"
"- paraboloid\n"
"- plymesh\n"
"- sphere\n"
"- trianglemesh\n"
"- mesh";

const char * ds_pylux_Context_start =
"(+) Re-start local rendering threads after a pause()";

const char * ds_pylux_Context_statistics =
"Return the named statistic from the current rendering. Valid statistic names are:\n"
"- sceneIsReady : returns truthy if the scene has been built and is ready for rendering\n"
"- filmIsReady : ?\n"
"- terminated : ?\n"
"- secElapsed : number of seconds since the rendering started\n"
"- samplesSec : current local rendering speed in samples per second\n"
"- samplesTotSec : current average total rendering speed in samples per second\n"
"- samplesPx : current number of samples per pixel\n"
"- efficiency : current rendering efficiency in percent\n"
"- displayInterval : current framebuffer update/display interval in seconds\n"
"- filmEV : current film Exposure Value\n"
"- enoughSamples : returns truthy if the rendering has reached a haltspp condition";

const char * ds_pylux_Context_surfaceIntegrator =
"Initialise the surface integrator to use for rendering. Valid types are:\n"
"- bidirectional\n"
"- directlighting\n"
"- exphotonmap\n"
"- path\n"
"- distributedpath";

const char * ds_pylux_Context_texture =
"Define a texture shader in the current scope. Valid types are:\n"
"- bilerp\n"
"- checkerboard\n"
"- constant\n"
"- dots\n"
"- fbm\n"
"- imagemap\n"
"- marble\n"
"- mix\n"
"- scale\n"
"- uv\n"
"- windy\n"
"- wrinkled\n"
"Lux also supports the textures found in Blender, prefixed with the name \"blender_\":\n"
"- blender_blend\n"
"- blender_clouds\n"
"- blender_distortednoise\n"
"- blender_magic\n"
"- blender_marble\n"
"- blender_musgrave\n"
"- blender_noise\n"
"- blender_stucci\n"
"- blender_voronoi\n"
"- blender_wood";

const char * ds_pylux_Context_transform =
"Set the current scope's transformation matrix. (Ref: RiSpec 3.2 p.56)";

const char * ds_pylux_Context_transformBegin =
"Begin a new transformation scope, inheriting the transform from the parent scope.\n"
"(Ref: RiSpec 3.2 p.59).";

const char * ds_pylux_Context_transformEnd =
"End the current transformation scope, reverting to the transformation of the parent\n"
"scope. (Ref: RiSpec 3.2 p.59).";

const char * ds_pylux_Context_translate =
"Concatenate a translation onto the current scope's transformation.\n"
"(Ref: RiSpec 3.2 p.57)";

const char * ds_pylux_Context_updateFilmFromNetwork =
"Update the current virtual film with contributions from attached network slaves.";

const char * ds_pylux_Context_updateFramebuffer =
"Process the raw virtual film through the tonemapping post-processing pipeline\n"
"into the LDR framebuffer. Results can be fetched with the frameBuffer() call.";

const char * ds_pylux_Context_volume =
"Define a bound volume object for volumetric effects. Valid types are:\n"
"- exponential\n"
"- homogenous";

const char * ds_pylux_Context_volumeIntegrator =
"Initialise the volume integrator to use for rendering. Valid types are:\n"
"- emission\n"
"- single";

const char * ds_pylux_Context_wait =
"Wait for rendering threads to complete. Unless some halt condition is set, or\n"
"the process is otherwise interrupted, expect to wait() forever.";

const char * ds_pylux_Context_worldBegin =
"Specify that all rendering options shall be frozen, and that what commences is\n"
"the description for the scene to be rendered. (Ref: RiSpec 3.2 p.17)";

const char * ds_pylux_Context_worldEnd =
"Ends the current scene description and starts the rendering process asynchronously\n"
"in a single local thread. (Ref: RiSpec 3.2 p.17)";
