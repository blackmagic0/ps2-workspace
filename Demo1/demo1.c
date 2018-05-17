#include <kernel.h>
#include <stdlib.h>
#include <tamtypes.h>
#include <math3d.h>

#include <packet.h>

#include <dma_tags.h>
#include <gif_tags.h>
#include <gs_psm.h>

#include <dma.h>

#include <graph.h>

#include <draw.h>
#include <draw3d.h>

#include "Controller.c"

#include <demo1.h>

void init_gs(framebuffer_t *frame, zbuffer_t *z) {
	// Setup frame parameters
	frame->width = 1280;
	frame->height = 720;
	frame->mask = 0;
	frame->psm = GS_PSM_32;
	frame->address = graph_vram_allocate(frame->width, frame->height, frame->psm, GRAPH_ALIGN_PAGE);

	// Enable the zbuffer.
	z->enable = DRAW_ENABLE;
	z->mask = 0;
	z->method = ZTEST_METHOD_GREATER_EQUAL;
	z->zsm = GS_ZBUF_32;
	z->address = graph_vram_allocate(frame->width, frame->height, z->zsm, GRAPH_ALIGN_PAGE);

	// Setup graphics for 720p video (TODO: can't seem to get 1080 working at this time)
	graph_set_mode(GRAPH_MODE_NONINTERLACED, GRAPH_MODE_HDTV_720P, GRAPH_MODE_FRAME, GRAPH_DISABLE);
	graph_set_screen(0, 0, 1280, 720);
	graph_set_bgcolor(0, 0, 0);
	graph_set_framebuffer_filtered(frame->address, frame->width, frame->psm, 0, 0);
	graph_enable_output();
}

void init_drawing_environment(framebuffer_t *frame, zbuffer_t *z) {
	packet_t *mesh_packet = packet_init(16, PACKET_NORMAL);

	// This is our generic qword pointer.
	qword_t *q = mesh_packet->data;

	// This will setup a default drawing environment.
	q = draw_setup_environment(q, 0, frame, z);

	// Now reset the primitive origin to 2048-width/2,2048-height/2.
	q = draw_primitive_xyoffset(q, 0, (2048 - 640), (2048 - 360));

	// Finish setting up the environment.
	q = draw_finish(q);

	// Now send the packet, no need to wait since it's the first.
	dma_channel_send_normal(DMA_CHANNEL_GIF, mesh_packet->data, q - mesh_packet->data, 0, 0);
	dma_wait_fast();

	packet_free(mesh_packet);
}

int render(framebuffer_t *frame, zbuffer_t *z) {
	int i;
	int context = 0;

	// Matrices to setup the 3D environment and camera
	MATRIX local_world;
	MATRIX world_view;
	MATRIX view_screen;
	MATRIX local_screen;

	VECTOR *temp_vertices;

	prim_t prim;
	color_t color;

	xyz_t   *verts;
	color_t *colors;

	// The data packets for double buffering dma sends.
	packet_t *packets[2];
	packet_t *current;
	qword_t *q;
	qword_t *dmatag;

	packets[0] = packet_init(100, PACKET_NORMAL);
	packets[1] = packet_init(100, PACKET_NORMAL);

	// Allocate calculation space.
	temp_vertices = memalign(128, sizeof(VECTOR) * vertex_count);

	// Allocate register space.
	verts  = memalign(128, sizeof(vertex_t) * vertex_count);
	colors = memalign(128, sizeof(color_t)  * vertex_count);

	// Define the triangle primitive we want to use.
	prim.type = PRIM_TRIANGLE;
	prim.shading = PRIM_SHADE_GOURAUD;
	prim.mapping = DRAW_DISABLE;
	prim.fogging = DRAW_DISABLE;
	prim.blending = DRAW_DISABLE;
	prim.antialiasing = DRAW_ENABLE;
	prim.mapping_type = PRIM_MAP_ST;
	prim.colorfix = PRIM_UNFIXED;

	color.r = 0x80;
	color.g = 0x80;
	color.b = 0x80;
	color.a = 0x80;
	color.q = 1.0f;

	// Create the view_screen matrix.
	create_view_screen(view_screen, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

	// Wait for any previous dma transfers to finish before starting.
	dma_wait_fast();

	// Distance to move / direction
	float move_dist_x = 0.3f;
	float move_dist_y = 0.25f;
	float move_dist_z = 0.3f;

	// The main loop...
	for (;;) {
		current = packets[context];

		// Check for input
		handleInput(con);

		// Moves the object
		if(object_position[0] <= -100.0f && move_dist_x < 0.0f) {
            move_dist_x = 0.3f;
		} else if(object_position[0] >= 100.0f && move_dist_x > 0.0f) {
		    move_dist_x = -0.3f;
		}
		object_position[0] += move_dist_x;

		if(object_position[1] <= 0.0f && move_dist_y < 0.0f) {
            move_dist_y = 0.25f;
		} else if(object_position[1] >= 100.0f && move_dist_y > 0.0f) {
		    move_dist_y = -0.25f;
		}
		object_position[1] += move_dist_y;

		if(object_position[2] <= -100.0f && move_dist_z < 0.0f) {
            move_dist_z = 0.3f;
		} else if(object_position[2] >= 20.0f && move_dist_z > 0.0f) {
		    move_dist_z = -0.3f;
		}
		object_position[2] += move_dist_z;

		// update camera position if following
		if(follow == 1) {
			camera_position[0] = object_position[0] + follow_offset[0];
			camera_position[1] = object_position[1] + follow_offset[1];
			camera_position[2] = object_position[2] + follow_offset[2];
		}

		// Spin the cube a bit.
		object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
		object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }

		// Create the local_world matrix.
		create_local_world(local_world, object_position, object_rotation);

		// Create the world_view matrix.
		create_world_view(world_view, camera_position, camera_rotation);

		// Create the local_screen matrix.
		create_local_screen(local_screen, local_world, world_view, view_screen);

		// Calculate the vertex values.
		calculate_vertices(temp_vertices, vertex_count, vertices, local_screen);

		// Convert floating point vertices to fixed point and translate to center of screen.
		draw_convert_xyz(verts, 2048, 2048, 32, vertex_count, (vertex_f_t*)temp_vertices);

		// Convert floating point colours to fixed point.
		draw_convert_rgbq(colors, vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)colours, 0x80);

		// Grab our dmatag pointer for the dma chain.
		dmatag = current->data;

		// Now grab our qword pointer and increment past the dmatag.
		q = dmatag;
		q++;

		// Clear framebuffer but don't update zbuffer.
		q = draw_disable_tests(q, 0, z);
		q = draw_clear(q, 0, 2048.0f - 640.0f, 2048.0f - 360.0f, frame->width, frame->height, 0x80, 0x80, 0x80);
		q = draw_enable_tests(q, 0, z);

		// Draw the triangles using triangle primitive type.
		q = draw_prim_start(q, 0, &prim, &color);

		for(i = 0; i < points_count; i++) {
			q->dw[0] = colors[points[i]].rgbaq;
			q->dw[1] = verts[points[i]].xyz;
			q++;
		}

		q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);

		// Setup a finish event.
		q = draw_finish(q);

		// Define our dmatag for the dma chain.
		DMATAG_END(dmatag, (q - current->data) - 1, 0, 0, 0);

		// Now send our current dma chain.
		dma_wait_fast();
		dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);

		// Now switch our packets so we can process data while the DMAC is working.
		context ^= 1;

		// Wait for scene to finish drawing
		draw_wait_finish();

		graph_wait_vsync();
	}

	packet_free(packets[0]);
	packet_free(packets[1]);

	return 0;
}

int main(int argc, char **argv) {
	// The buffers to be used.
	framebuffer_t frame;
	zbuffer_t z;

	// Init Controller
	initController();
	con.onPressLeft = &moveLeft;
	con.onPressRight = &moveRight;
	con.onPressUp = &moveUp;
	con.onPressDown = &moveDown;
	con.onPressCross = &toggleFollow;

	// Init GIF dma channel.
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	// Init the GS, framebuffer, and zbuffer.
	init_gs(&frame, &z);

	// Init the drawing environment and framebuffer.
	init_drawing_environment(&frame, &z);

	// Render the cube
	render(&frame, &z);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}

void moveLeft() {
	camera_position[0] -= 10.0f;
}

void moveRight() {
	camera_position[0] += 10.0f;
}

void moveUp() {
	camera_position[1] += 10.0f;
}

void moveDown() {
	camera_position[1] -= 10.0f;
}

void toggleFollow() {
	if(follow == 1) follow = 0;
	else {
		follow = 1;
		follow_offset[0] = camera_position[0] - object_position[0];
		follow_offset[1] = camera_position[1] - object_position[1];
		follow_offset[2] = camera_position[2] - object_position[2];
	}
}