#pragma once

#include "mesh.h"
#include "Math.h"
#include <vector>

struct Camera {
    Vec3<float> position;
    Vec3<float> target;
    Vec3<float> world_up;

    float fov;
    float z_near;
    float z_far;
};

struct Texture {
    std::string textureName;
    std::string texturePath;
    int textureWidth, textureHeight, numChannels;
    unsigned char* pixels;
};

struct Light {
    Vec3<float> color;
    float intensity;
    virtual Vec3<float>* getPosition() = 0; 
    virtual Vec3<float>* getDirection() = 0;
};

struct DirectionalLight : Light {
    Vec3<float> direction;
    Vec3<float>* getPosition() override; 
    Vec3<float>* getDirection() override;
}; 

struct PointLight : Light {
    Vec3<float> position;
    float attentuation;
    Vec3<float>* getPosition() override; 
    Vec3<float>* getDirection() override;
};

struct Scene {
    std::vector<Mesh> mesh_list;
    std::vector<Mat4x4<float>> xform_list;
    std::vector<Mesh_Instance> instance_list;
    std::vector<Texture> texture_list;

    Camera main_camera;
    std::vector<PointLight> pointLightList;
    std::vector<DirectionalLight> directionalLightList;
};

class Scene_Manager {
public:
    Scene_Manager() {}
    void load_scene_form_file(const char* filename);
    void add_instance(Scene& scene, uint32_t mesh_id);
    void loadTextureFromFile(Scene& scene, std::string& name, const char* filename);
    void findTextureForMesh(Scene& scene, Mesh& mesh);
    Mat4x4<float> get_camera_view(Camera& camera);
};

extern Scene_Manager scene_manager;