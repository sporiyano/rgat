#include "stdafx.h"
#include "basicblock_handler.h"
#include "trace_handler.h"
#include "render_preview_thread.h"
#include "traceStructs.h"
#include "traceMisc.h"
#include "module_handler.h"
#include "render_heatmap_thread.h"
#include "render_conditional_thread.h"
#include "GUIManagement.h"
#include "rendering.h"
#include "b64.h"
#include "preview_pane.h"
#include "serialise.h"
#include "diff_plotter.h"

//possible name: rgat
//ridiculous/runtime graph analysis tool
//run rgat -f malware.exe

#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "OpenGL32.lib")

//#include <allegro5/allegro_primitives.h>

int handle_event(ALLEGRO_EVENT *ev, VISSTATE *clientstate);

void launch_saved_PID_threads(int PID, PID_DATA *piddata, VISSTATE *clientState)
{
	DWORD threadID;
	graph_renderer *render_thread = new graph_renderer;
	render_thread->clientState = clientState;
	render_thread->PID = PID;
	render_thread->piddata = piddata;

	HANDLE hOutThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)render_thread->ThreadEntry,
		(LPVOID)render_thread, 0, &threadID);

	heatmap_renderer *heatmap_thread = new heatmap_renderer;
	heatmap_thread->clientState = clientState;
	heatmap_thread->piddata = piddata;

	HANDLE hHeatThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)heatmap_thread->ThreadEntry,
		(LPVOID)heatmap_thread, 0, &threadID);

	conditional_renderer *conditional_thread = new conditional_renderer;
	conditional_thread->clientState = clientState;
	conditional_thread->piddata = piddata;

	Sleep(200);
	HANDLE hConditionThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)conditional_thread->ThreadEntry,
		(LPVOID)conditional_thread, 0, &threadID);

}
//todo: make this a thread/mainloop check that listens for new processes
void launch_running_PID_threads(int PID, std::map<int, PID_DATA *> *glob_piddata_map, HANDLE pidmutex, VISSTATE *clientState) {
	PID_DATA *piddata = new PID_DATA;
	piddata->PID = PID;

	if (!obtainMutex(pidmutex, "Launch PID threads")) return;
	glob_piddata_map->insert_or_assign(PID, piddata);
	ReleaseMutex(pidmutex);

	DWORD threadID;

	module_handler *tPIDThread = new module_handler;
	tPIDThread->clientState = clientState;
	tPIDThread->PID = PID;
	tPIDThread->piddata = piddata;

	HANDLE hPIDmodThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)tPIDThread->ThreadEntry,
		(LPVOID)tPIDThread, 0, &threadID);

	basicblock_handler *tBBThread = new basicblock_handler;
	tBBThread->clientState = clientState;
	tBBThread->PID = PID;
	tBBThread->piddata = piddata;

	HANDLE hPIDBBThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)tBBThread->ThreadEntry,
		(LPVOID)tBBThread, 0, &threadID);

	
	graph_renderer *render_preview_thread = new graph_renderer;
	render_preview_thread->clientState = clientState;
	render_preview_thread->PID = PID;
	render_preview_thread->piddata = piddata;

	HANDLE hPreviewThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)render_preview_thread->ThreadEntry,
		(LPVOID)render_preview_thread, 0, &threadID);
	
	heatmap_renderer *heatmap_thread = new heatmap_renderer;
	heatmap_thread->clientState = clientState;
	heatmap_thread->piddata = piddata;

	HANDLE hHeatThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)heatmap_thread->ThreadEntry,
		(LPVOID)heatmap_thread, 0, &threadID);
	
	conditional_renderer *conditional_thread = new conditional_renderer;
	conditional_thread->clientState = clientState;
	conditional_thread->piddata = piddata;

	Sleep(200);
	HANDLE hConditionThread = CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)conditional_thread->ThreadEntry,
		(LPVOID)conditional_thread, 0, &threadID);

}



int GUI_init(ALLEGRO_EVENT_QUEUE ** evq, VISSTATE *clientState) {
	clientState->maindisplay = displaySetup();
	if (!clientState->maindisplay) return 0;
	if (!controlSetup()) return 0;

	*evq = al_create_event_queue();
	al_register_event_source(*evq, (ALLEGRO_EVENT_SOURCE*)al_get_mouse_event_source());
	al_register_event_source(*evq, (ALLEGRO_EVENT_SOURCE*)al_get_keyboard_event_source());
	al_register_event_source(*evq, create_menu(clientState->maindisplay));
	al_register_event_source(*evq, al_get_display_event_source(clientState->maindisplay));
	return 1;
}

void windows_execute_tracer(string executable) {
	string drpath = "C:\\Users\\nia\\Documents\\tracevis\\DynamoRIO-Windows-6.1.0-2\\bin32\\drrun.exe -c ";
	string runpath = drpath + "C:\\Users\\nia\\Documents\\tracevis\\traceclient\\Debug\\traceclient.dll -- ";
	runpath.append(executable);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	printf("Starting execution of target...\n");
	CreateProcessA(NULL, (char *)runpath.c_str(), NULL, NULL, false, 0, NULL, NULL, &si, &pi);
}

int launch_target() {
	//todo: posibly worry about pre-existing if pidthreads dont work
	HANDLE hPipe = CreateNamedPipe(L"\\\\.\\pipe\\BootstrapPipe",
		PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT,
		255, 65536, 65536, 300, NULL);

	if (hPipe == INVALID_HANDLE_VALUE)
	{
		_tprintf(TEXT("CreateNamedPipe failed, GLE=%d.\n"), GetLastError());
		return -1;
	}

	//runpath.append("C:\\Users\\nia\\Documents\\tracevis\\vulnerableapp.exe");
	//runpath.append("C:\\Users\\nia\\Desktop\\retools\\netcat-1.11\\nc.exe -l -p 9989");
	string executable("C:\\tracing\\testdllloader.exe");
	windows_execute_tracer(executable);
	Sleep(800);

	//todo: genericise this?
	DWORD bread = 0;
	char buf[40];
	bool res = ReadFile(hPipe, buf, 30, &bread, NULL);
	if (!bread){
		printf("Read 0 when waiting for PID. Try again\n");
		return -1;
	}
	buf[bread] = 0;

	int PID = 0;
	if (!extract_integer(buf, string("PID"), &PID))
	{
		//todo: fail here soemtimes
		printf("ERROR: Something bad happen in extract_integer, string is: %s\n", buf);
		return -1;
	}
	return PID;
}

void updateMainRender(VISSTATE *clientState)
{
	ALLEGRO_DISPLAY *display = clientState->maindisplay;
	render_main_graph(clientState);

	//todo: change to on size change?
	if (clientState->wireframe_sphere)
		delete clientState->wireframe_sphere;

	clientState->wireframe_sphere = new GRAPH_DISPLAY_DATA(WFCOLBUFSIZE * 2);
	plot_wireframe(clientState);

	plot_colourpick_sphere(clientState);
	updateTitle_NumPrimitives(display, clientState, clientState->activeGraph->get_mainverts()->get_numVerts(),
		clientState->activeGraph->get_mainlines()->get_renderedEdges());
	clientState->rescale = false;

}


void change_mode(VISSTATE *clientstate, int mode)
{
	switch (mode)
	{
	case EV_BTN_WIREFRAME:
		clientstate->modes.wireframe = !clientstate->modes.wireframe;
		//todo: change icon
		return;

	case EV_BTN_CONDITION:
		clientstate->modes.conditional = !clientstate->modes.conditional;
		if (clientstate->modes.conditional) clientstate->modes.heatmap = false;
		//todo: change icon
		return;

	case EV_BTN_HEATMAP:

		clientstate->modes.heatmap = !clientstate->modes.heatmap;
		clientstate->modes.nodes = !clientstate->modes.heatmap;
		if (clientstate->modes.heatmap) clientstate->modes.conditional = false;
		//todo: change icon
		return;

	case EV_BTN_PREVIEW:

		al_destroy_bitmap(clientstate->mainGraphBMP);
		if (clientstate->modes.preview)
			clientstate->mainGraphBMP = al_create_bitmap(clientstate->size.width, clientstate->size.height);
		else
			clientstate->mainGraphBMP = al_create_bitmap(clientstate->size.width - PREVIEW_PANE_WIDTH, clientstate->size.height);
		clientstate->modes.preview = !clientstate->modes.preview;
		//todo: change icon
		return;

	case EV_BTN_DIFF:
		clientstate->modes.heatmap = false;
		clientstate->modes.conditional = false;
		return;

	}

}







void processDiff(VISSTATE *clientState, ALLEGRO_FONT *font, diff_plotter **diffRenderer)
{
	if (clientState->modes.diff == DIFF_STARTED) 
		display_graph_diff(clientState, *diffRenderer);
	else if (clientState->modes.diff == DIFF_SELECTED)//diff button clicked
	{
		change_mode(clientState, EV_BTN_DIFF);
		clientState->modes.diff = DIFF_STARTED;
		TraceVisGUI *widgets = (TraceVisGUI *)clientState->widgets;
		widgets->showHideDiffFrame();

		thread_graph_data *graph1 = widgets->diffWindow->get_graph(1);
		thread_graph_data *graph2 = widgets->diffWindow->get_graph(2);
		*diffRenderer = new diff_plotter(graph1, graph2, clientState);
		((diff_plotter*)*diffRenderer)->render();
	}

	((diff_plotter*)*diffRenderer)->display_diff_summary(20, 10, font, clientState);
}

int main(int argc, char **argv)
{

	VISSTATE clientstate;
	printf("Starting visualiser\n");

	ALLEGRO_EVENT_QUEUE *event_queue = 0;
	if (!GUI_init(&event_queue, &clientstate)) {
		printf("GUI init failed\n");
		return 0;
	}
	clientstate.guidata = init_GUI_Colours();
	clientstate.size.height = al_get_display_height(clientstate.maindisplay);
	clientstate.size.width = al_get_display_width(clientstate.maindisplay);
	clientstate.mainGraphBMP = al_create_bitmap(clientstate.size.width - PREVIEW_PANE_WIDTH, clientstate.size.height);

	clientstate.modes.wireframe = true;
	clientstate.activeGraph = 0;

	al_set_target_backbuffer(clientstate.maindisplay);

	TITLE windowtitle;
	clientstate.title = &windowtitle;

	updateTitle_Mouse(clientstate.maindisplay, &windowtitle, 0, 0);
	updateTitle_Zoom(clientstate.maindisplay, &windowtitle, clientstate.zoomlevel);

	bool buildComplete = false;

	//this is a pain in the neck to have, see wireframe code
	GLint *wireframeStarts = (GLint *)malloc(WIREFRAMELOOPS * sizeof(GLint));
	GLint *wireframeSizes = (GLint *)malloc(WIREFRAMELOOPS * sizeof(GLint));
	for (int i = 0; i < WIREFRAMELOOPS; i++)
	{
		wireframeStarts[i] = i*POINTSPERLINE;
		wireframeSizes[i] = POINTSPERLINE;
	}

	int bufsize = 0;

	//setup frame limiter/fps clock
	double fps, fps_unlocked, frame_start_time;

	ALLEGRO_EVENT tev;
	ALLEGRO_TIMER *frametimer = al_create_timer(1.0 / 60.0);
	ALLEGRO_EVENT_QUEUE *frame_timer_queue = al_create_event_queue();
	al_register_event_source(frame_timer_queue, al_get_timer_event_source(frametimer));
	al_start_timer(frametimer);

	//edge_picking_colours() is a hefty call, but doesn't need calling often
	ALLEGRO_TIMER *updatetimer = al_create_timer(40.0 / 60.0);
	ALLEGRO_EVENT_QUEUE *pos_update_timer_queue = al_create_event_queue();
	al_register_event_source(pos_update_timer_queue, al_get_timer_event_source(updatetimer));
	al_start_timer(updatetimer);

	if (!frametimer || !updatetimer) printf("Failed timer creation\n");

	al_init_font_addon();
	al_init_ttf_addon();
	char dirbuf[255];
	GetCurrentDirectoryA(255, dirbuf);
	printf("GetCurrentDirectoryA dir: %s\n", dirbuf);
	
	clientstate.standardFont = al_load_ttf_font("VeraSe.ttf", 10, 0);
	ALLEGRO_FONT *PIDFont = al_load_ttf_font("VeraSe.ttf", 14, 0);
	if (!clientstate.standardFont) {
		fprintf(stderr, "Could not load 'VeraSe.ttf'.\n");
		return -1;
	}

	TraceVisGUI* widgets = new TraceVisGUI(&clientstate);
	clientstate.widgets = (void *)widgets;
	widgets->widgetSetup();

	//preload glyphs in cache
	al_get_text_width(clientstate.standardFont, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
	al_get_text_width(PIDFont, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	clientstate.zoomlevel = 100000;
	clientstate.previewPaneBMP = al_create_bitmap(PREVIEW_PANE_WIDTH, clientstate.size.height);
	initial_gl_setup(&clientstate);

	//for rendering graph diff
	diff_plotter *diffRenderer;

	GRAPH_DISPLAY_DATA *vertsdata = NULL;
	GRAPH_DISPLAY_DATA *linedata = NULL;
	map<int, node_data>::iterator vertit;

	SCREEN_EDGE_PIX TBRG;
	ALLEGRO_EVENT ev;
	int previewRenderFrame = 0;
	map <int, pair<int, int>> graphPositions;

	bool running = true;
	while (running)
	{
		frame_start_time = al_get_time();

		//no active graph but a process exists
		//this is in the main loop so the GUI works at the start
		//todo set to own function when we OOP this
		if (!clientstate.activeGraph && clientstate.glob_piddata_map.size() > 0)
		{
			if (!obtainMutex(clientstate.pidMapMutex, "Main Loop")) return 0;

			PID_DATA *activePid = clientstate.glob_piddata_map.begin()->second;
			
			widgets->setActivePID(activePid->PID);
			clientstate.activePid = activePid;
			map<int, void *>::iterator graphIt;
			graphIt = activePid->graphs.begin();

			for (;graphIt != activePid->graphs.end();graphIt++)
			{
				thread_graph_data * graph = (thread_graph_data *)graphIt->second;
				if (!graph->edgeDict.size()) continue;
				clientstate.activeGraph = graph;
				widgets->controlWindow->setAnimEnabled(!clientstate.activeGraph->active);
				break;
			}
			ReleaseMutex(clientstate.pidMapMutex);
		}

		//active graph changed
		if (clientstate.newActiveGraph)
		{
			clientstate.activeGraph = (thread_graph_data *)clientstate.newActiveGraph;
			widgets->controlWindow->setAnimEnabled(!clientstate.activeGraph->active);
			clientstate.activeGraph->blockInstruction = 0;
			clientstate.newActiveGraph = 0;
		}

		if (clientstate.activeGraph)
		{

			al_set_target_bitmap(clientstate.mainGraphBMP);

			frame_gl_setup(&clientstate);

			//todo: move to a clearcolour variable in state
			if (clientstate.modes.conditional)
				al_clear_to_color(al_map_rgb(240, 240, 240));
			else
				al_clear_to_color(al_map_rgb(0, 0, 0));

			//note where on the sphere the camera is pointing
			if (!al_is_event_queue_empty(pos_update_timer_queue))
			{
				al_flush_event_queue(pos_update_timer_queue);
				edge_picking_colours(&clientstate, &TBRG, true);
				clientstate.leftcolumn = (int)floor(ADIVISIONS * TBRG.leftgreen) - 1;
				clientstate.rightcolumn = (int)floor(ADIVISIONS * TBRG.rightgreen) - 1;
			}

			if (clientstate.modes.wireframe)
				draw_wireframe(&clientstate, wireframeStarts, wireframeSizes);

			if (clientstate.modes.diff)
				processDiff(&clientstate, PIDFont, &diffRenderer);
			else
			{
				thread_graph_data *graph = clientstate.activeGraph;
				if (
					(graph->get_mainverts()->get_numVerts() < graph->get_num_verts()) ||
					(graph->get_mainlines()->get_renderedEdges() < graph->edgeList.size()) ||
					clientstate.rescale)
				{
					printf("updating render! %d<%d or %d<%d\n", graph->get_mainverts()->get_numVerts(),
						graph->get_num_verts(), graph->get_mainlines()->get_renderedEdges(), graph->edgeList.size());
					updateMainRender(&clientstate);
				}

				if (!graph->active)
				{
					if (clientstate.animationUpdate)
					{
						int result = graph->updateAnimation(&clientstate.activePid->disassembly,
							clientstate.animationUpdate, clientstate.stepBBs,
							clientstate.modes.animation);

						if (!clientstate.modes.animation)
							clientstate.animationUpdate = 0;
						else
						{
							if (result == ANIMATION_ENDED)
							{
								clientstate.animationUpdate = 0;
								clientstate.modes.animation = false;
								widgets->controlWindow->notifyAnimFinished();
							}
							else
								graph->update_animation_render(&clientstate.activePid->disassembly, clientstate.stepBBs);
						}
					}
					//the draw_text in the graph drawing screws this up if we do it afterwards
					draw_anim_line(graph->get_active_node(), graph);
				}

				//printf("VertdictsizE: %d\n", graph->vertDict.size());
				if (clientstate.modes.heatmap)
					display_big_heatmap(&clientstate);
				else if (clientstate.modes.conditional)
					display_big_conditional(&clientstate);
				else
				{
					if (!graph->active)
					{
						if (graph->terminated)
						{
							graph->reset_animation(&clientstate.activePid->disassembly);
							clientstate.modes.animation = false;
							graph->terminated = false;
						}
						display_graph(&clientstate, graph);
					}
					else {
						clientstate.modes.animation = true;
						graph->animate_to_last(&clientstate.activePid->disassembly, clientstate.stepBBs);
						display_graph(&clientstate, graph);
						draw_anim_line(graph->get_active_node(), graph);
					}
				}

				display_activeGraph_summary(20, 10, PIDFont, &clientstate);
			}

			frame_gl_teardown();

			if (clientstate.modes.preview)
			{

				if (previewRenderFrame++ % (60 / PREVIEW_RENDER_FPS))
				{
					drawPreviewGraphs(&clientstate, &graphPositions);
					previewRenderFrame = 0;
				}

				al_set_target_backbuffer(clientstate.maindisplay);
				al_draw_bitmap(clientstate.previewPaneBMP, clientstate.size.width - PREVIEW_PANE_WIDTH, 0, 0);
			}
			else
				al_set_target_backbuffer(clientstate.maindisplay);
			al_draw_bitmap(clientstate.mainGraphBMP, 0, 0, 0);

			widgets->updateRenderWidgets(clientstate.activeGraph);

			al_flip_display();
		}

		//ui events
		while (al_get_next_event(event_queue, &ev))
		{
			int eventResult = handle_event(&ev, &clientstate);
			if (!eventResult) continue;
			switch (eventResult)
			{
			case EV_KEYBOARD:
				printf("EV KEY!\n");
				widgets->processEvent(&ev);
				break;

			case EV_MOUSE:
				widgets->processEvent(&ev);
				if (clientstate.newPID > -1)
				{
					printf("TODO: CHANGE PID HERE TO %d\n", clientstate.newPID);
					clientstate.newPID = -1;
				}
				break;

			case EV_BTN_RUN:
			{
				printf("run clicked! need to use file dialogue or cmdline?");
				int PID = launch_target();
				launch_running_PID_threads(PID, &clientstate.glob_piddata_map, clientstate.pidMapMutex, &clientstate);
				break;
			}
			case EV_BTN_QUIT:
				running = false;
				break;

			default:
				printf("WARNING! Unhandled event %d\n", eventResult);
			}
		}

		fps_unlocked = 1 / (al_get_time() - frame_start_time);
		al_wait_for_event(frame_timer_queue, &tev);
		fps = 1 / (al_get_time() - frame_start_time);
		//if (fps < 50) printf("FPS WARNING! %f (unlocked: %f)\n", fps, fps_unlocked);
		updateTitle_FPS(clientstate.maindisplay, clientstate.title, fps, fps_unlocked);

		
	}

	free(wireframeStarts);
	free(wireframeSizes);

	cleanup_for_exit(clientstate.maindisplay);
	return 0;
}



bool loadTrace(VISSTATE *clientstate, string filename) {

	ifstream loadfile;
	loadfile.open(filename, std::ifstream::binary);
	//load process data
	string s1;

	loadfile >> s1;//
	if (s1 != "PID") {
		printf("Corrupt save, start = %s\n", s1.c_str());
		return false;
	}

	int PID;
	loadfile >> PID;
	if (PID < 0 || PID > 100000) { printf("Corrupt save (pid=%d)\n", PID); return false; }
	else printf("Loading saved PID: %d\n", PID);
	loadfile.seekg(1, ios::cur);

	map<unsigned long, INS_DATA*> insdict;
	PID_DATA *newpiddata = new PID_DATA;
	newpiddata->PID = PID;
	if (!loadProcessData(clientstate, &loadfile, newpiddata, &insdict))
		return false;

	if (!loadProcessGraphs(clientstate, &loadfile, newpiddata, &insdict))
	{
		printf("Process Graph load failed\n");
		return false;
	}

	printf("Loading completed successfully\n");
	loadfile.close();

	if (!obtainMutex(clientstate->pidMapMutex, "load graph")) return 0;
	clientstate->glob_piddata_map[PID] = newpiddata;
	ReleaseMutex(clientstate->pidMapMutex);

	launch_saved_PID_threads(PID, newpiddata, clientstate);
	return true;
}

void set_active_graph(VISSTATE *clientState, int PID, int TID)
{
	PID_DATA* target_pid = clientState->glob_piddata_map[PID];
	clientState->newActiveGraph = target_pid->graphs[TID];

	thread_graph_data * graph = (thread_graph_data *)target_pid->graphs[TID];
	if (graph->modPath.empty())	graph->assign_modpath(target_pid);

	TraceVisGUI *widgets = (TraceVisGUI *)clientState->widgets;
	widgets->diffWindow->setDiffGraph(graph);

	if (clientState->modes.diff)
		clientState->modes.diff = 0;
}

int handle_event(ALLEGRO_EVENT *ev, VISSTATE *clientstate) {
	ALLEGRO_DISPLAY *display = clientstate->maindisplay;
	if (ev->type == ALLEGRO_EVENT_DISPLAY_RESIZE)
	{
		//TODO! REMAKE BITMAP
		clientstate->size.height = ev->display.height;
		clientstate->size.width = ev->display.width;
		handle_resize(clientstate);
		al_acknowledge_resize(display);
		printf("display resize handled\n");
		return EV_RESIZE;
	}

	

	
	if (ev->type == ALLEGRO_EVENT_MOUSE_AXES)
	{
		if (!clientstate->activeGraph) return 0;
		MULTIPLIERS *mainscale = clientstate->activeGraph->m_scalefactors;
		float diam = mainscale->radius;
		long maxZoomIn = diam + 5; //prevent zoom into globe
		long slowRotateThresholdLow = diam + 8000;  // move very slow beyond this much zoom in 
		long slowRotateThresholdHigh = diam + 54650;// move very slow beyond this much zoom out

		float zoomdiff = abs(mainscale->radius - clientstate->zoomlevel);

		if (ev->mouse.dz) {
			//adjust speed of zoom depending on how close we are
			int zoomfactor;

			if (clientstate->zoomlevel > 40000)
				zoomfactor = -5000;
			else
				zoomfactor = -1000;

			float newZoom = clientstate->zoomlevel + zoomfactor * ev->mouse.dz;
			if (newZoom >= maxZoomIn)
				clientstate->zoomlevel = newZoom;
			if (clientstate->zoomlevel == 0)
				clientstate->zoomlevel = 1; //delme testing only

			if (clientstate->activeGraph)
				updateTitle_Zoom(display, clientstate->title, (clientstate->zoomlevel - clientstate->activeGraph->zoomLevel));
		}


		if (ev->mouse.dx || ev->mouse.dy) {
			ALLEGRO_MOUSE_STATE state;
			al_get_mouse_state(&state);
			if (clientstate->mouse_dragging)
			{
				//printf("Mouse DRAGGED dx:%d, dy:%d x:%d,y:%d\n", ev->mouse.dx, ev->mouse.dy, ev->mouse.x, ev->mouse.y);
				float dx = ev->mouse.dx;
				float dy = ev->mouse.dy;
				dx = min(1, max(dx, -1));
				dy = min(1, max(dy, -1));

				float slowdownfactor = 0.035; //reduce movement this much for every 1000 pixels zoomed in
				float slowdown = zoomdiff / 1000;
				//printf("zoomdiff: %f slowdown: %f\n", zoomdiff, slowdown);

				// here we control drag speed at various zoom levels
				// todo when we have insturctions to look at
				//todo: fix speed at furhter out zoom levels

				//if (zoomdiff > slowRotateThresholdLow && zoomdiff < slowRotateThresholdHigh) {
				//	printf("non slowed drag! low:%ld -> zd: %f -> high:%ld\n", slowRotateThresholdLow, zoomdiff, slowRotateThresholdHigh);
				//	dx *= 0.1;
				//	dy *= 0.1;
				//}
				//else
				//{
				if (slowdown > 0)
				{
					//printf("slowed drag! low:%ld -> zd: %f -> high:%ld slowdown:%f\n",slowRotateThresholdLow,zoomdiff,slowRotateThresholdHigh,slowdown);
					if (dx != 0) dx *= (slowdown * slowdownfactor);
					if (dy != 0) dy *= (slowdown * slowdownfactor);
				}
				//}
				clientstate->xturn -= dx;
				clientstate->yturn -= dy;
				char tistring[200];
				snprintf(tistring, 200, "xt:%f, yt:%f", fmod(clientstate->xturn, 360), fmod(clientstate->yturn, 360));
				updateTitle_dbg(display, clientstate->title, tistring);
			}

			updateTitle_Mouse(display, clientstate->title, ev->mouse.x, ev->mouse.y);
		}

		return EV_MOUSE;
	}

	switch (ev->type)
	{
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
	{
		//state.buttons & 1 || state.buttons & 4)
		if (!clientstate->modes.preview || ev->mouse.x < (clientstate->size.width - PREVIEW_PANE_WIDTH))
			clientstate->mouse_dragging = true;
		else
		{
			int PID, TID;
			if (find_mouseover_thread(clientstate, ev->mouse.x, ev->mouse.y, &PID, &TID))
				set_active_graph(clientstate, PID, TID);		
		}
		//switch(ev->mouse.button) 
		return EV_MOUSE;
	}

	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
	{
		clientstate->mouse_dragging = false;
		return EV_MOUSE;
	}

	case ALLEGRO_EVENT_KEY_CHAR:
	{
		if (!clientstate->activeGraph) return 0;
		MULTIPLIERS *mainscale = clientstate->activeGraph->m_scalefactors;
		switch (ev->keyboard.keycode)
		{
		case ALLEGRO_KEY_ESCAPE:
			return EV_BTN_QUIT;

		case ALLEGRO_KEY_Y:
			change_mode(clientstate, EV_BTN_WIREFRAME);
			break;

		case ALLEGRO_KEY_K:
			change_mode(clientstate, EV_BTN_HEATMAP);
			break;

		case ALLEGRO_KEY_J:
			change_mode(clientstate, EV_BTN_CONDITION);
			break;

		case ALLEGRO_KEY_LEFT:

			mainscale->userHEDGESEP -= 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_RIGHT:
			mainscale->userHEDGESEP += 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_DOWN:
			mainscale->userVEDGESEP += 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_UP:
			mainscale->userVEDGESEP -= 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_PAD_PLUS:
			mainscale->userDiamModifier += 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_PAD_MINUS:
			mainscale->userDiamModifier -= 0.05;
			clientstate->rescale = true;
			break;
		case ALLEGRO_KEY_T:
			clientstate->show_ins_text++;
			if (clientstate->show_ins_text > INSTEXT_LAST)
				clientstate->show_ins_text = INSTEXT_FIRST;
			switch (clientstate->show_ins_text) {
			case INSTEXT_NONE:
				printf("Instruction text off");
				break;
			case INSTEXT_AUTO:
				printf("Instruction text auto");
				break;
			case INSTEXT_ALL_ALWAYS:
				printf("Instruction text always on");
				break;
			}
			break;
		case ALLEGRO_KEY_PAD_7:
			clientstate->zoomlevel += 100;
			break;
		case ALLEGRO_KEY_PAD_1:
			clientstate->zoomlevel -= 100;
			break;
		case ALLEGRO_KEY_PAD_8:
			clientstate->zoomlevel += 10;
			break;
		case ALLEGRO_KEY_PAD_2:
			clientstate->zoomlevel -= 10;
			break;
		case ALLEGRO_KEY_PAD_9:
			clientstate->zoomlevel += 1;
			break;
		case ALLEGRO_KEY_PAD_3:
			clientstate->zoomlevel -= 1;
			break;
		default:
			return EV_KEYBOARD;
		}
		return EV_KEYBOARD;
	}

	case ALLEGRO_EVENT_MENU_CLICK:
	{
		switch (ev->user.data1)
		{
		case EV_BTN_RUN:
			return EV_BTN_RUN;
		case EV_BTN_QUIT:
			return EV_BTN_QUIT;
		case EV_BTN_WIREFRAME:
		case EV_BTN_PREVIEW:
		case EV_BTN_CONDITION:
		case EV_BTN_HEATMAP:
			change_mode(clientstate, ev->user.data1);
			break;
		case EV_BTN_DIFF:
			((TraceVisGUI *)clientstate->widgets)->showHideDiffFrame();
			break;
		case EV_BTN_STEPPING:
			clientstate->stepBBs = !clientstate->stepBBs;
			if (!clientstate->stepBBs)
			{
				clientstate->stepBBs = true;
				clientstate->activeGraph->blockInstruction = 0;
			}
			else
				clientstate->stepBBs = false;
			break;
		case EV_BTN_SAVE:
			if (clientstate->activeGraph)
			{
				printf("Saving process %d to file\n", clientstate->activeGraph->pid);
				saveTrace(clientstate);
			}
			break;
		case EV_BTN_LOAD:
			printf("Opening file dialogue\n");
			loadTrace(clientstate, string("C:\\tracing\\testsave.txt"));
			break;
		default:
			printf("UNHANDLED MENU EVENT? %d\n", ev->user.data1);
			break;
		}
		return EV_NONE;
	}

	case ALLEGRO_EVENT_DISPLAY_CLOSE:
		return EV_BTN_QUIT;
	}

	switch (ev->type) {
	case ALLEGRO_EVENT_DISPLAY_SWITCH_IN:
		printf("event: main display switch in\n");
		return EV_NONE;
	case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
		printf("event: main display switch out\n");
		return EV_NONE;
	case ALLEGRO_EVENT_KEY_DOWN: //agui wont get this
	case ALLEGRO_EVENT_MOUSE_LEAVE_DISPLAY:
	case ALLEGRO_EVENT_KEY_UP:
	case ALLEGRO_EVENT_MOUSE_ENTER_DISPLAY:
	case ALLEGRO_EVENT_KEY_CHAR:
		return EV_NONE;
	}
	printf("unhandled event: %d\n", ev->type);

	return EV_NONE; //usually lose_focus
}