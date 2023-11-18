#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "array.h"
#include "display.h"
#include "vector.h"
#include "matrix.h"
#include "mesh.h"

// Array of triangles that should be rendered frame by frame
triangle_t* triangles_to_render = NULL;

vec3_t camera_position = { 0, 0, 0 }; // 9x9x9 cube
//vec3_t cube_rotation = {.x = 0, .y = 0, .z = 0};


// Global variables for execution status and game loop
bool is_running = false;
int previous_frame_time = 0;
float fov_factor = 640;

// Setup function to initialize variables and game objects
void setup(void) {
    
    // Initialize render mode and triangle culling method
    render_method = RENDER_WIRE;
    cull_method = CULL_BACKFACE;
    
    
    
	// Alloc the memory in bytes to hold the color buffer
	color_buffer = (uint32_t*) malloc(sizeof(uint32_t) * window_width * window_height);
	// Creating a SDL texture that is used to display the color
	color_buffer_texture = SDL_CreateTexture(
		renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		window_width,
		window_height
	);
    
    // Loads the vertex and face values for the mesh
    // Loads the cube values in the mesh data structure
    load_cube_mesh_data();
    
    //load_obj_file_data("./assets/cube.obj");
    
    

}


void process_input(void) {
	SDL_Event event;
	SDL_PollEvent(&event);
	
	switch (event.type) {
		case SDL_QUIT:
			is_running = false;
			break;
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE)
				is_running = false;
            if (event.key.keysym.sym == SDLK_1)
                render_method = RENDER_WIRE_VERTEX;
            if (event.key.keysym.sym == SDLK_2)
                render_method = RENDER_WIRE;
            if (event.key.keysym.sym == SDLK_3)
                render_method = RENDER_FILL_TRIANGLE;
            if (event.key.keysym.sym == SDLK_4)
                render_method = RENDER_FILL_TRIANGLE_WIRE;
            if (event.key.keysym.sym == SDLK_c)
                cull_method = CULL_BACKFACE;
            if (event.key.keysym.sym == SDLK_d)
                cull_method = CULL_NONE;
            break;
	}
	
}

////////////////////////////////////////////////////////////////////////////////
// Function that receives a 3D vector and returns a projected 2D point
////////////////////////////////////////////////////////////////////////////////

vec2_t project(vec3_t point) {
    vec2_t projected_point = {
        .x = (fov_factor * point.x) / point.z,
        .y = (fov_factor * point.y) / point.z
    };
    return projected_point;
}



void update(void) {
    
    // Wait some time until the reach the target frame time in milliseconds
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - previous_frame_time);

    // Only delay execution if we are running too fast
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }
    
    previous_frame_time = SDL_GetTicks();
    
    
    // Initialize the array of triangles to render
    triangles_to_render = NULL;
    
    // Change the mesh scale/rotation values per animation frame
    mesh.rotation.x += 0.01;
    mesh.rotation.y += 0.01;
    mesh.rotation.z += 0.01;
    
    //mesh.scale.x += 0.000;
    //mesh.scale.y += 0.000;
    
    mesh.translation.x += 0.01;
    //mesh.translation.x += 0.0;
    
    // create a scale and translation matrix to multiply mesh vertices
    mat4_t scale_matrix = mat4_make_scale(mesh.scale.x, mesh.scale.y, mesh.scale.z);
    
    mat4_t translation_matrix = mat4_make_translation(mesh.translation.x, mesh.translation.y, mesh.translation.z);
    
    mat4_t rotation_matrix_x = mat4_make_rotation_x(mesh.rotation.x);
    mat4_t rotation_matrix_y = mat4_make_rotation_y(mesh.rotation.y);
    mat4_t rotation_matrix_z = mat4_make_rotation_z(mesh.rotation.z);
    
    // Loop all triangle faces of our mesh
    int num_faces = array_length(mesh.faces);
    for (int i = 0; i < num_faces; i++) {
        face_t mesh_face = mesh.faces[i];
        
        vec3_t face_vertices[3];
        face_vertices[0] = mesh.vertices[mesh_face.a - 1];
        face_vertices[1] = mesh.vertices[mesh_face.b - 1];
        face_vertices[2] = mesh.vertices[mesh_face.c - 1];
        
        
        // Loop all three vertices of this current face and perform transformation
        
        vec4_t transformed_vertices[3];
        
        for (int j = 0; j < 3; j++) {
            
            
            // Use a matrix to scale our original vertex
            // TODO: multiply the scale_matrix by the vertex
            vec4_t transformed_vertex = vec4_from_vec3(face_vertices[j]);
            
            // Create World Matrix combining scale, rotation and translation
            // Order matters: 1. scale, 2. rotate, 3. translate [T]*[R]*[S]*v
            mat4_t world_matrix = mat4_identity();
            world_matrix = mat4_mul_mat4(scale_matrix, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_z, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_y, world_matrix);
            world_matrix = mat4_mul_mat4(rotation_matrix_x, world_matrix);
            world_matrix = mat4_mul_mat4(translation_matrix, world_matrix);
            
            //multiply all matrices and load in one operation
            
            transformed_vertex = mat4_mul_vec4(world_matrix, transformed_vertex);
            
            // translate vertex from the camera
            transformed_vertex.z += 5;
            
            // Save transformed vertex in the array of transformed vertices
            transformed_vertices[j] = transformed_vertex;
        }
        
        // TODO: Check backface culling
        // loop all faces
        vec3_t vector_a = vec3_from_vec4(transformed_vertices[0]); /*   A   */
        vec3_t vector_b = vec3_from_vec4(transformed_vertices[1]); /*  / \  */
        vec3_t vector_c = vec3_from_vec4(transformed_vertices[2]); /* C---B */
        
        // Get the vector subtraction of B-A and C-A
        vec3_t vector_ab = vec3_sub(vector_b, vector_a);
        vec3_t vector_ac = vec3_sub(vector_c, vector_a);
        vec3_normalize(&vector_ab);
        vec3_normalize(&vector_ac);
        
        // Compute the face normal (using cross product to find perpendicular)
        vec3_t normal = vec3_cross(vector_ab, vector_ac);
        
        // Normalize the face normal vector
        vec3_normalize(&normal);
        
        
        // Find the vector between a point in the triangle and the camera origin
        vec3_t camera_ray = vec3_sub(camera_position, vector_a);
        
        // calculate how aligned is the camera ray with the face...
        float dot_normal_camera = vec3_dot(normal, camera_ray);
        
        
        // bypass the triangles that are looking away from the camera
        if ( dot_normal_camera < 0 && cull_method == CULL_BACKFACE ) {
            continue;
        }
        
        
        vec2_t projected_points[3];
        
        // Loop all three vertices to perform projection
        for (int j = 0; j < 3; j++) {
            // project the current vertices
            projected_points[j] = project(vec3_from_vec4(transformed_vertices[j]));
            
            // Scale and translate the projected point to the middle of the screen
            projected_points[j].x += (window_width / 2);
            projected_points[j].y += (window_height / 2);
            
            //projected_triangle.points[j] = projected_point;
        }
        
        // calculate average depth for each face
        //float avg_depth = (transformed_vertices[0].z > transformed_vertices[1].z) ? transformed_vertices[1].z : transformed_vertices[0].z;
                           
        //avg_depth = (avg_depth > transformed_vertices[2].z) ? transformed_vertices[2].z : avg_depth;
        
        float avg_depth = (transformed_vertices[0].z + transformed_vertices[1].z + transformed_vertices[2].z) / 3.0;
        
        triangle_t projected_triangle = {
            .points = {
                {projected_points[0].x, projected_points[0].y},
                {projected_points[1].x, projected_points[1].y},
                {projected_points[2].x, projected_points[2].y}
            },
            .color = mesh_face.color,
            .avg_depth = avg_depth
            // TODO:
        };
        
        // save the projected triangle in an array of triangles to render
        //triangles_to_render[i] = projected_triangle;
        array_push(triangles_to_render, projected_triangle);
        
        
        // sort by bubbles sort method....
        
        int n = array_length(triangles_to_render);
        
            for (int i = 0; i < n - 1; i++) {
                for (int j = 0; j < n - i - 1; j++) {
                    if (triangles_to_render[j].avg_depth < triangles_to_render[j + 1].avg_depth) {
                        // Swap arr[j] and arr[j + 1]
                        triangle_t temp = triangles_to_render[j];
                        triangles_to_render[j] = triangles_to_render[j + 1];
                        triangles_to_render[j + 1] = temp;
                    }
                }
            }
        
        
    }
    
    // TODO: sort the triangles to render according average depth
    
    
    
    /*
        for (int i = 0; i < N_POINTS; i++) {
        vec3_t point = cube_points[i];
        
        vec3_t transformed_point = vec3_rotate_x(point, cube_rotation.x);
        transformed_point = vec3_rotate_y(transformed_point, cube_rotation.y);
        transformed_point = vec3_rotate_z(transformed_point, cube_rotation.z);
        
        // Translate the point away from the camera
        transformed_point.z -= camera_position.z;
        
        // project the current point
        vec2_t projected_point = project(transformed_point);
        // save the projected point in the array of projected points
        projected_points[i] = projected_point;
        
    }
     */
}

void render(void) {
    SDL_RenderClear(renderer);

	//draw_grid();
    
 //   /*
    // loop all projected triangles and render them
    int num_triangles = array_length(triangles_to_render);
    
    for (int i = 0; i < num_triangles; i++) {
        
        triangle_t triangle = triangles_to_render[i];
        
        
        if (render_method == RENDER_WIRE) {
            draw_triangle(
                          triangle.points[0].x,
                          triangle.points[0].y,
                          triangle.points[1].x,
                          triangle.points[1].y,
                          triangle.points[2].x,
                          triangle.points[2].y,
                          0xFFFFFF00);
        } else if (render_method == RENDER_FILL_TRIANGLE) {
            draw_filled_triangle(
            triangle.points[0].x, triangle.points[0].y,
            triangle.points[1].x, triangle.points[1].y,
            triangle.points[2].x, triangle.points[2].y,
                          triangle.color);
        } else if (render_method == RENDER_FILL_TRIANGLE_WIRE) {
            draw_filled_triangle(
            triangle.points[0].x, triangle.points[0].y,
            triangle.points[1].x, triangle.points[1].y,
            triangle.points[2].x, triangle.points[2].y,
                          triangle.color);
            
            draw_triangle(
            triangle.points[0].x,
            triangle.points[0].y,
            triangle.points[1].x,
            triangle.points[1].y,
            triangle.points[2].x,
            triangle.points[2].y,
                          0xFFFFFF00 );
        } else if (render_method == RENDER_WIRE_VERTEX) {
            draw_triangle(
            triangle.points[0].x,
            triangle.points[0].y,
            triangle.points[1].x,
            triangle.points[1].y,
            triangle.points[2].x,
            triangle.points[2].y,
                          0xFFFFFF00 );
            
            draw_rect(triangle.points[0].x, triangle.points[0].y, 3, 3, 0xFFFF0000);
            draw_rect(triangle.points[1].x, triangle.points[1].y, 3, 3, 0xFFFF0000);
            draw_rect(triangle.points[2].x, triangle.points[2].y, 3, 3, 0xFFFF0000);
            
        }
    }
    
    if (render_method == RENDER_FILL_TRIANGLE_WIRE) {
        for (int i = 0; i < num_triangles; i++) {
            triangle_t triangle = triangles_to_render[i];
            draw_triangle(
            triangle.points[0].x,
            triangle.points[0].y,
            triangle.points[1].x,
            triangle.points[1].y,
            triangle.points[2].x,
            triangle.points[2].y,
                          0xFFFFFF00 );
        }
    }

        
   /*
        triangle_t triangle = triangles_to_render[i];
        draw_rect(triangle.points[0].x, triangle.points[0].y, 3, 3, 0xFFFF0000);
        draw_rect(triangle.points[1].x, triangle.points[1].y, 3, 3, 0xFFFF0000);
        draw_rect(triangle.points[2].x, triangle.points[2].y, 3, 3, 0xFFFF0000);
        
        
        draw_filled_triangle(
        triangle.points[0].x, triangle.points[0].y,
        triangle.points[1].x, triangle.points[1].y,
        triangle.points[2].x, triangle.points[2].y,
                      0xFF00FF00);
        
        draw_triangle(
        triangle.points[0].x,
        triangle.points[0].y,
        triangle.points[1].x,
        triangle.points[1].y,
        triangle.points[2].x,
        triangle.points[2].y,
                      0xFFFFFF00 );
    }
    
//  */
    
    
    
    
    
    // Clear the array of triangles to render every frame loop
    array_free(triangles_to_render);
    
	render_color_buffer();	
	clear_color_buffer(0xFF000000);

	SDL_RenderPresent(renderer);
	
}

// Free the memory that was dynamically allocated by the progra
void free_resources(void) {
    free(color_buffer);
    array_free(mesh.faces);
    array_free(mesh.vertices);
}


int main(void) {
	/* TODO: Create a SDL window */
	
	is_running = initialize_window();
	
	setup();
    
    
    
	while (is_running) {
		process_input();
		update();
		render();
	}
    
    destroy_window();
    free_resources();
	
	return 0;	

}
