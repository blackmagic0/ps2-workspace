// The file format for .obj is as follows:
// # - comment
//
// v - vertex coordinates (x, y, z [, w]) if no w, value is 1.0
// v 0.1 0.222 0.0321
//
// vt - texture coordinates (u, v [, w]) if no w, value is 0
// vt 0.5 0.73
//
// vn - vertex normals (x, y, z) (possibly not unit vectors)
// vn 0.123 0.000 0.442
//
// vp - parameter space vertices TODO: add support
//
// f - face, format = vertex_id/texture_id/normal_id
// f 1 2 3				-> face between vertex 1, 2, and 3
// f 1 2 3 4 5			-> face between vertex 1, 2, 3, 4, and 5
// f 2/1 3/2 6/4  		-> face between vertex 2, 3, and 6.  with texture 1, 2, and 4 mapped on the vertices
// f 2/3/4 3/4/5 4/5/6	-> face between vertex 2, 3, and 4.  with texture 3, 4, and 5 mapped on the vertices.  with normals 4, 5, and 6 on the vertices.
// f 7//2 3//1 9//4		-> face between vertex 7, 3, and 9.  with no texture map.  with normals 2, 1, and 4 on the vertices.
//
// l - line builds a line between each vertex in the order they are listed TODO: add support
// l 1 4 5 6 2 5 -> line from 1 to 4 to 5...
//
// mtllib - TODO: add support
// usemtl - TODO: add support
// o & g & s - TODO: really don't need this
//
// support for other BS:
// - relative indexes (will add a fuckload of size to the library..)
//

// TODO: obj pre-processor can decrease load speed.
// TODO: or just create a custom mesh binary file

#include <tamtypes.h>
#include <malloc.h>
#include <stdio.h>
#include <fileXio_rpc.h>

#include <OBJLoader.h>

/**
 * Parses an obj file into mesh data
 * TODO: flags to disable reading certain types, saving space used by mesh, and processing time.
 *
 * file--> The fully qualified path to read the file from.
 * mesh--> The mesh_data object to read the mesh into
 *
 * returns:
 *     0 -> Successfully read file
 *    -1 -> Couldn't open file
 *    -2 -> Couldn't write allocate space for the file data
 */
int parseOBJ(char * file, mesh_data * mesh) {
	// Read the obj file
	obj_file * obj = malloc(sizeof(obj_file));
	int file_handle = fileXioOpen(file, O_RDONLY, 0);
	if(file_handle == -1) return -1;

	obj->size = fileXioLseek(file_handle, 0, SEEK_END);
	fileXioLseek(file_handle, 0, SEEK_SET);

	obj->data = (u8 *) malloc(obj->size);
	if(obj->data == NULL) return -2;

	fileXioRead(file_handle, obj->data, obj->size);
	fileXioClose(file_handle);

	// Start parsing the file
	char * prefix_buffer = (char *) malloc(7);			// 6 is the largest prefix length + 1 for null character space
	int buffer_index = 0;								// The index to write to in the prefix buffer
	int i = 0;											// The index in the data to read from
	int EOL = 0;										// Flag is the end of the line was reached
	while(i++ < obj->size) {							// For every byte in the data
		char value = obj->data[i];							// Store the next byte to check for end of prefix
		if(value == " ") {									// The prefix_buffer contains prefix for this line
			prefix_buffer[buffer_index] = "\0";					// Instead of copying the space character just terminate the string.
			// TODO: make sure this string matching works right
			if(prefix_buffer == "v") 		mesh->vertex_count++;	// Increment the count for the type of data the line represents
			else if(prefix_buffer == "vn")	mesh->normal_count++;
			else if(prefix_buffer == "vt") 	mesh->texture_count++;
			else if(prefix_buffer == "f")	mesh->face_count++;
			//else if(prefix_buffer == "l") 	mesh->line_count++;	// Not implemented
			else { /* Any other prefixes should be ignored as they are not implemented */ }

			while(i++ < obj->size) {							// Seek i to next line as we are only trying to count the number of each type of data here.
				if(obj->data[i] == "\n") {							// If newline is found then we know that we are looking at the next line
					buffer_index = 0;									// Reset the buffer index so we write at the beginning again
					break;												// Start reading the next line
				}
			}
		} else prefix_buffer[buffer_index++] = value;		// The prefix is not fully read yet, so copy the character into the prefix buffer and increment the buffer index
	}

	free(prefix_buffer);		// Free the prefix buffer, wont need it anymore

	// Allocate space for the mesh data in mesh...
	mesh->vertices 		 = (vertex *)	malloc(mesh->vertex_count  * sizeof(vertex));
	mesh->normals		 = (V3 *)		malloc(mesh->normal_count  * sizeof(V3));
	mesh->faces			 = (face *)		malloc(mesh->face_count    * sizeof(face));
	mesh->texture_coords = (texture *)	malloc(mesh->texture_count * sizeof(texture));

	// Start copying data into mesh
	char * line_buffer = (char *) malloc(64);	// Buffer may not contain entire contents of a single line

	int EOL = 0; 				// End of line reached
	int buff_of = 0;			// Buffer has overflowed
	int data_index = 0;			// The index of data to look at

	int vetex_index = 0, 		// The index of which vertex to write to mesh data
		normal_index = 0,  		// The index of which normal to write to mesh data
		face_index = 0,  		// The index of which face to write to mesh data
		texture_index = 0; 		// The index of which texture to write to mesh data

	// Start reading the actual values of each line into the mesh data
	//TODO: don't read dropped lines, and don't increment line_index
	while(data_index < obj->size) {
		int index = 0;
		do {
			char value = obj->data[data_index + index];
			data_index++;
			if(value == "\n") {
				line_buffer[index] = "\0";
				EOL = 1;
				break;
			}
			line_buffer[index] = value;
			index++;
		} while(index < sizeof(line_buffer));

		if(!EOL) {
			/* Process partial line_buffer */
			// TODO: finish
			buff_of = 1;

		} else {
			/* Process full line_buffer */
			// TODO: finish
			if(buff_of) {
				// The buffer has overflowed, so this is continuing from previous buffer.
				// TODO: step back to last space, find out what parameter it is on (param index?), then continue filling the data buffer from there.
			}

			buff_of = 0;
		}
	}

	// Clear buffers
	free(line_buffer);	// Free the line buffer, as we are done parsing
	free(obj->data);	// Free the obj data, as we are done parsing
	return 0;
}