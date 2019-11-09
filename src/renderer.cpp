#include "../include/renderer.h"
#include "Math.h"

Renderer::Renderer() {
    mesh_attrib_flag = 0;
    active_shader_id = 0;
    buffer_width = 0;
    buffer_height = 0;
}

void Renderer::init() {
    z_buffer = new float[buffer_width * buffer_height];
    // init z_buffer depth value to a huge number
    for (int j = 0; j < buffer_height; j++) {
        for (int i = 0; i < buffer_width; i++) {
            z_buffer[buffer_width * j + i] = 1.f;
        }
    }
    // setup viewport matrix here
    viewport = Mat4x4<float>::viewport(buffer_width, buffer_height);
    // setup available shader here
}

void Renderer::alloc_backbuffer(Window& window) {
    buffer_width = window.width;
    buffer_height = window.height;
    backbuffer = new unsigned char[4 * buffer_width * buffer_height];
}

// @: Flipped the y coordinates to match OpenGL's screen space coordinates
//    since I later pass this buffer as a texture data for OpenGL to render
void Renderer::draw_pixel(int x, int y, Vec4<int>& color) {
    uint32_t screenY = buffer_height - y - 1;
    backbuffer[(screenY * buffer_width + x) * 4] = (unsigned char)color.x;     // r
    backbuffer[(screenY * buffer_width + x) * 4 + 1] = (unsigned char)color.y; // g
    backbuffer[(screenY * buffer_width + x) * 4 + 2] = (unsigned char)color.z; // b
    backbuffer[(screenY * buffer_width + x) * 4 + 3] = (unsigned char)color.w; // a
}

void Renderer::clearBuffer() {
    for (int pixel = 0; pixel < buffer_width * buffer_height; pixel++) {
        backbuffer[pixel] = 0;
        backbuffer[pixel + 1] = 0;
        backbuffer[pixel + 2] = 0;
        backbuffer[pixel + 3] = 0;
    }
}

void Renderer::drawScene(Scene& scene) {
    for (auto& instance : scene.instance_list) {
        for (auto directionalLight : scene.directionalLightList) {
            Mesh& mesh = scene.mesh_list[instance.mesh_id];
            // bind the texture to active shader
            shader_list[active_shader_id]->texture_ = &scene.texture_list[mesh.textureID];  
            draw_instance(&directionalLight, mesh);
        }
    }
}

// TODO: @ Frustum culling
// TODO: @ Backface culling
// TODO: @ PBR
// TODO: @ Should deal with instance xform in here
void Renderer::draw_instance(Light* light, Mesh& mesh) {    
    Shader_Base* active_shader = shader_list[active_shader_id];
    if (mesh.texture_uv_buffer) {
        mesh_attrib_flag = mesh_attrib_flag | 1;
    }
    if (mesh.normal_buffer) {
        mesh_attrib_flag = mesh_attrib_flag | (1 << 1);
    }

    // TODO: @ Clean up
    // TODO: @ Debug rendering
    // TODO: @ OpenMP
    #pragma omp parallel
    // render face by face
    for (int f_idx = 0; f_idx < mesh.num_faces; f_idx++) {
        for (int v = 0; v < 3; v++) {
            // vertex transform
            triangle_clip[v] = active_shader->vertex_shader(mesh_manager.get_vertex(mesh, f_idx * 3 + v));
            // perspective division
            //triangle_clip[v] = triangle_clip[v] / triangle_clip[v].w;
            // viewport transform
            Vec4<float> v_screen = viewport * (triangle_clip[v] / triangle_clip[v].w);
            triangle_screen[v].x = v_screen.x;
            triangle_screen[v].y = v_screen.y;
            // has texture uv attrib 
            if (mesh_attrib_flag & 0x0001) {
                triangle_uv[v] = mesh_manager.get_vt(mesh, f_idx * 3 + v); 
            } 
            // has normal attrib 
            if (mesh_attrib_flag & 0x0002) {
                triangle_normal[v] = mesh_manager.get_vn(mesh, f_idx * 3 + v);
            } 
        }

        // TODO: @ frustum culling
        // if projected triangle is partially out of screen, discard it for now
        
        // backface cull

        // -------------

        // rasterization
        fill_triangle(active_shader, light);
    }
}

bool Renderer::depth_test(int fragmentX, int fragmentY, Vec3<float> _bary_coord) {
    int index = fragmentY * buffer_width + fragmentX;
    // Maybe this is wrong here, I was using the z after perspective division
    float fragmentZ = 1 / (_bary_coord.x / triangle_clip[0].z + _bary_coord.y / triangle_clip[1].z + _bary_coord.z / triangle_clip[2].z);
    if (fragmentZ < z_buffer[index]) {
        z_buffer[index] = fragmentZ;
        return true;
    }
    return false;
}

// TODO: Improve normal mapping
// TODO: Create multiple fragmentShader instance
// TODO: Maybe something like fragmentShaderPools[bufferWidth][bufferHeight]
void Renderer::fill_triangle(Shader_Base* active_shader_ptr, Light* light) {
    // TODO: @ Clean up using a determinant()
    Vec2<float> e1 = triangle_screen[1] - triangle_screen[0];
    Vec2<float> e2 = triangle_screen[2] - triangle_screen[0];
    float denom = e1.x * e2.y - e2.x * e1.y;
    // discard the triangle if its degenerated into a line
    if (denom == 0.f) {
        return;
    }

    float bbox[4] = {
        triangle_screen[0].x,
        triangle_screen[0].x,
        triangle_screen[0].y,
        triangle_screen[0].y,
    }; 
    Math::bound_triangle(triangle_screen, bbox);

    // TODO: @ OpenMP
    #pragma omp parallel
    for (int x = bbox[0]; x < bbox[1]; x++) {
        for (int y = bbox[2]; y < bbox[3]; y++) {
            // compute barycentric coord
            Vec3<float> bary_coord = Math::barycentric(triangle_screen, x, y, denom); // overlapping test
            if (bary_coord.x < 0.f || bary_coord.y < 0.f || bary_coord.z < 0.f) {
                continue;
            }
            // depth test
            if (!depth_test(x, y, bary_coord)) {
                continue;
            }
            // get fragment depth

            // TODO: may not even need to bother checking
            // TODO: if normal is not provided, pass in interpolated normal
            // interpolate given vertex attribute
            uint32_t attribIdx = y * buffer_width + x; 
            // TODO: @ pass in light to fragment shader
            if (mesh_attrib_flag & 0x0001) {
                active_shader_ptr->fragmentAttribBuffer[attribIdx].textureCoord = Math::bary_interpolate(triangle_uv, bary_coord);
            }
            if (mesh_attrib_flag & 0x0002) {
                active_shader_ptr->fragmentAttribBuffer[attribIdx].normal = Math::bary_interpolate(triangle_normal, bary_coord);
            }
            // TODO: Bulletproof this setup for lighting computation
            // point light
            if (light->getPosition()) {
                // compute per fragment light direction
            } else {
                active_shader_ptr->lightingParamBuffer[attribIdx].color = light->color;
                active_shader_ptr->lightingParamBuffer[attribIdx].intensity = light->intensity;
                active_shader_ptr->lightingParamBuffer[attribIdx].direction = *(light->getDirection());
                // view
            }
            // compute fragment color
            Vec4<int> fragmentColor = active_shader_ptr->fragment_shader(x, y);
            // write to backbuffer
            draw_pixel(x, y, fragmentColor);
            // -------------------
        }
    }
}