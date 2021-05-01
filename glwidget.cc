// Author: Marc Comino 2020
#define STB_IMAGE_IMPLEMENTATION
#include <glwidget.h>
#include <stb_image.h>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "./mesh_io.h"
#include "./triangle_mesh.h"

namespace {

const double kFieldOfView = 60;
const double kZNear = 0.0001;
const double kZFar = 10;

const char kReflectionVertexShaderFile[] = "../shaders/reflection.vert";
const char kReflectionFragmentShaderFile[] = "../shaders/reflection.frag";
const char kBRDFVertexShaderFile[] = "../shaders/brdf.vert";
const char kBRDFFragmentShaderFile[] = "../shaders/brdf.frag";
const char kPBRVertexShaderFile[] = "../shaders/pbr.vert";
const char kPBRFragmentShaderFile[] = "../shaders/pbr.frag";
const char kSkyVertexShaderFile[] = "../shaders/sky.vert";
const char kSkyFragmentShaderFile[] = "../shaders/sky.frag";
const char kCubemapVertexShaderFile[] = "../shaders/cubemap.vert";
const char kEquiToCubeFragmentShaderFile[] = "../shaders/equirectangular_to_cubemap.frag";
const char kIrradianceFragmentShaderFile[] = "../shaders/irradiance.frag";
const char kPrefilterFragmentShaderFile[] = "../shaders/prefilter.frag";

const int kVertexAttributeIdx = 0;
const int kNormalAttributeIdx = 1;

const int maxMipLevels = 5;

// pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
// ----------------------------------------------------------------------------------------------
glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
glm::mat4 captureViews[] =
{
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
};

bool ReadFile(const std::string filename, std::string *shader_source) {
  std::ifstream infile(filename.c_str());

  if (!infile.is_open() || !infile.good()) {
    std::cerr << "Error " + filename + " not found." << std::endl;
    return false;
  }

  std::stringstream stream;
  stream << infile.rdbuf();
  infile.close();

  *shader_source = stream.str();
  return true;
}

bool LoadImage(const std::string &path, GLuint cube_map_pos) {
  QImage image;
  bool res = image.load(path.c_str());
  if (res) {
    QImage gl_image = image.mirrored();
    glTexImage2D(cube_map_pos, 0, GL_RGBA, image.width(), image.height(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
  }
  return res;
}

bool LoadCubeMap(const QString &dir) {
  std::string path = dir.toUtf8().constData();
  bool res = LoadImage(path + "/right.png", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  res = res && LoadImage(path + "/left.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  res = res && LoadImage(path + "/top.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  res = res && LoadImage(path + "/bottom.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  res = res && LoadImage(path + "/back.png", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  res = res && LoadImage(path + "/front.png", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

  if (res) {
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    std::cerr << "Cubemap loaded " << std::endl;
  }

  return res;
}

bool LoadProgram(const std::string &vertex, const std::string &fragment,
                 QOpenGLShaderProgram *program) {
  std::string vertex_shader, fragment_shader;
  bool res =
      ReadFile(vertex, &vertex_shader) && ReadFile(fragment, &fragment_shader);

  if (res) {
    program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                     vertex_shader.c_str());
    program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                     fragment_shader.c_str());
    program->bindAttributeLocation("vertex", kVertexAttributeIdx);
    program->bindAttributeLocation("normal", kNormalAttributeIdx);
    program->link();
    std::cerr << "Program loaded: " + vertex + "   " + fragment << std::endl;
  }
  else {
      std::cerr << "ERROR LOADING: " + vertex + "   " + fragment << std::endl;
  }
  return res;
}

}  // namespace

GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(parent),
      initialized_(false),
      width_(0.0),
      height_(0.0),
      reflection_(true),
      fresnel_(0.2, 0.2, 0.2) {
  setFocusPolicy(Qt::StrongFocus);
}

GLWidget::~GLWidget() {
  if (initialized_) {
    glDeleteTextures(1, &specular_map_);
    glDeleteTextures(1, &diffuse_map_);
  }
}

bool GLWidget::LoadModel(const QString &filename) {
  std::string file = filename.toUtf8().constData();
  size_t pos = file.find_last_of(".");
  std::string type = file.substr(pos + 1);

  std::unique_ptr<data_representation::TriangleMesh> mesh =
      std::make_unique<data_representation::TriangleMesh>();

  bool res = false;
  if (type.compare("ply") == 0) {
    res = data_representation::ReadFromPly(file, mesh.get());
  }
  std::cout << "..................." << std::endl;
  if (res) {
    mesh_.reset(mesh.release());
    camera_.UpdateModel(mesh_->min_, mesh_->max_);
    std::cout << "Model has " <<  mesh_->buffer_.size() << " elements in buffer" << std::endl;
    // TODO(students): Create / Initialize buffers.
    glGenVertexArrays(1, &modelVAO);
    glGenBuffers(1, &modelVBO);
    glGenBuffers(1, &modelEBO);
    glBindVertexArray(modelVAO);
    glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
    glBufferData(GL_ARRAY_BUFFER, mesh_->buffer_.size()* sizeof(float), &mesh_->buffer_[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_->faces_.size() * sizeof(int), &mesh_->faces_[0], GL_STATIC_DRAW);
    // END.

    emit SetFaces(QString(std::to_string(mesh_->faces_.size() / 3).c_str()));
    emit SetVertices(
        QString(std::to_string(mesh_->vertices_.size() / 3).c_str()));
    std::cerr << "Model loaded " + file << std::endl;
    return true;
  }
  std::cerr << "ERROR loading model " + file << std::endl;
  return false;
}

bool GLWidget::LoadSpecularMap(const QString &dir) {
  glBindTexture(GL_TEXTURE_CUBE_MAP, specular_map_);
  bool res = LoadCubeMap(dir);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  return res;
}

bool GLWidget::LoadDiffuseMap(const QString &dir) {
  glBindTexture(GL_TEXTURE_CUBE_MAP, diffuse_map_);
  bool res = LoadCubeMap(dir);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  return res;
}

void GLWidget::initializeGL() {
  glewExperimental=true;
  metalnessParameter = 0.0;
  roughnessParameter = 0.0;
  glewInit();

  glEnable(GL_NORMALIZE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_DEPTH_TEST);

  glGenTextures(1, &specular_map_);
  glGenTextures(1, &diffuse_map_);

  reflection_program_ = std::make_unique<QOpenGLShaderProgram>();
  brdf_program_ = std::make_unique<QOpenGLShaderProgram>();
  sky_program_ = std::make_unique<QOpenGLShaderProgram>();
  equirect_to_cubemap_program_ = std::make_unique<QOpenGLShaderProgram>();
  irradiance_program_ = std::make_unique<QOpenGLShaderProgram>();
  pbr_program_ = std::make_unique<QOpenGLShaderProgram>();
  prefilter_program_ = std::make_unique<QOpenGLShaderProgram>();

  bool res =
      LoadProgram(kReflectionVertexShaderFile, kReflectionFragmentShaderFile,
                  reflection_program_.get());
  res = res && LoadProgram(kBRDFVertexShaderFile, kBRDFFragmentShaderFile,
                           brdf_program_.get());
  res = res && LoadProgram(kSkyVertexShaderFile, kSkyFragmentShaderFile,
                           sky_program_.get());
  res = res && LoadProgram(kCubemapVertexShaderFile, kEquiToCubeFragmentShaderFile,
                           equirect_to_cubemap_program_.get());
  res = res && LoadProgram(kCubemapVertexShaderFile, kIrradianceFragmentShaderFile,
                           irradiance_program_.get());
  res = res && LoadProgram(kPBRVertexShaderFile, kPBRFragmentShaderFile,
                           pbr_program_.get());
  res = res && LoadProgram(kCubemapVertexShaderFile, kPrefilterFragmentShaderFile,
                           prefilter_program_.get());

  
  if (!res) exit(0);
  std::cerr << "All programs loaded" << std::endl;
  float skyboxVertices[] = {

          // positions
          -1.0f,  1.0f, -1.0f,
          -1.0f, -1.0f, -1.0f,
           1.0f, -1.0f, -1.0f,
           1.0f, -1.0f, -1.0f,
           1.0f,  1.0f, -1.0f,
          -1.0f,  1.0f, -1.0f,

          -1.0f, -1.0f,  1.0f,
          -1.0f, -1.0f, -1.0f,
          -1.0f,  1.0f, -1.0f,
          -1.0f,  1.0f, -1.0f,
          -1.0f,  1.0f,  1.0f,
          -1.0f, -1.0f,  1.0f,

           1.0f, -1.0f, -1.0f,
           1.0f, -1.0f,  1.0f,
           1.0f,  1.0f,  1.0f,
           1.0f,  1.0f,  1.0f,
           1.0f,  1.0f, -1.0f,
           1.0f, -1.0f, -1.0f,

          -1.0f, -1.0f,  1.0f,
          -1.0f,  1.0f,  1.0f,
           1.0f,  1.0f,  1.0f,
           1.0f,  1.0f,  1.0f,
           1.0f, -1.0f,  1.0f,
          -1.0f, -1.0f,  1.0f,

          -1.0f,  1.0f, -1.0f,
           1.0f,  1.0f, -1.0f,
           1.0f,  1.0f,  1.0f,
           1.0f,  1.0f,  1.0f,
          -1.0f,  1.0f,  1.0f,
          -1.0f,  1.0f, -1.0f,

          -1.0f, -1.0f, -1.0f,
          -1.0f, -1.0f,  1.0f,
           1.0f, -1.0f, -1.0f,
           1.0f, -1.0f, -1.0f,
          -1.0f, -1.0f,  1.0f,
           1.0f, -1.0f,  1.0f
      };
  std::cerr << "skyboxVertices" << std::endl;
  glGenVertexArrays(1, &skyboxVAO);
  std::cerr << "glGenVertexArrays" << std::endl;
  glGenBuffers(1, &skyboxVBO);
  std::cerr << "glGenBuffers" << std::endl;
  glBindVertexArray(skyboxVAO);
  std::cerr << "glBindVertexArray" << std::endl;
  glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
  std::cerr << "glBindBuffer" << std::endl;
  glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
  std::cerr << "glBufferData" << std::endl;
  glEnableVertexAttribArray(0);
  std::cerr << "glEnableVertexAttribArray" << std::endl;
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  std::cerr << "glVertexAttribPointer" << std::endl;
  initialized_ = true;
  std::cerr << "Gl init OK" << std::endl;

  glDepthFunc(GL_LEQUAL);
  // enable seamless cubemap sampling for lower mip levels in the pre-filter map.
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);


  loadCubemapFileHDR("../textures/Tropical_Beach/Tropical_Beach_3k.hdr");

  LoadModel("../models/sphere.ply");
  std::cerr << "Default model loaded OK" << std::endl;
}
bool GLWidget::loadCubemapFileHDR(const QString &path)
{
  setupFramebuffer();
  std::cerr << "Framebuffer OK" << std::endl;
  loadHDRenvMap(path);  //TODO move this to a better place, allow change image while running
  std::cerr << "Envmap load OK" << std::endl;
  setupEnvMap();
  std::cerr << "Envmap processed OK" << std::endl;
  setupIrrMap();
  std::cerr << "Irradiance map processed OK" << std::endl;

  setupPrefilterMap();
  std::cerr << "Irradiance map processed OK" << std::endl;

  setupBRDF();
  std::cerr << "BRDF map processed OK" << std::endl;
  return true;
}
void GLWidget::reloadShaders()
{
    reflection_program_.reset();
    reflection_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kReflectionVertexShaderFile, kReflectionFragmentShaderFile,
                reflection_program_.get());

    brdf_program_.reset();
    brdf_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kBRDFVertexShaderFile, kBRDFFragmentShaderFile,
                brdf_program_.get());

    sky_program_.reset();
    sky_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kSkyVertexShaderFile, kSkyFragmentShaderFile,
                sky_program_.get());

    equirect_to_cubemap_program_.reset();
    equirect_to_cubemap_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kCubemapVertexShaderFile, kEquiToCubeFragmentShaderFile,
                equirect_to_cubemap_program_.get());

    irradiance_program_.reset();
    irradiance_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kCubemapVertexShaderFile, kIrradianceFragmentShaderFile,
                irradiance_program_.get());

    prefilter_program_.reset();
    prefilter_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kCubemapVertexShaderFile, kPrefilterFragmentShaderFile,
                prefilter_program_.get());

    pbr_program_.reset();
    pbr_program_ = std::make_unique<QOpenGLShaderProgram>();
    LoadProgram(kPBRVertexShaderFile, kPBRFragmentShaderFile,
                pbr_program_.get());
}
void GLWidget::setupEnvMap()
{
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: convert HDR equirectangular environment map to cubemap equivalent
    // ----------------------------------------------------------------------
    //equirectangularToCubemapShader.use();
    equirect_to_cubemap_program_->bind();
    //equirectangularToCubemapShader.setInt("equirectangularMap", 0);
    GLint equirectangular_map_location = equirect_to_cubemap_program_->uniformLocation("equirectangularMap");
    glUniform1i(equirectangular_map_location, 0);
    //equirectangularToCubemapShader.setMat4("projection", captureProjection);
    GLint projection_location = equirect_to_cubemap_program_->uniformLocation("projection");
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        //equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        GLint view_location = equirect_to_cubemap_program_->uniformLocation("view");
        glm::mat4 currentView = captureViews[i];
        glUniformMatrix4fv(view_location, 1, GL_FALSE, &currentView[0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        //glDepthFunc(GL_LESS); // set depth function back to default

    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLWidget::setupIrrMap()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    irradiance_program_->bind();
    GLint equirectangular_map_location = irradiance_program_->uniformLocation("environmentMap");
    glUniform1i(equirectangular_map_location, 0);
    GLint projection_location = irradiance_program_->uniformLocation("projection");
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

    glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        //equirectangularToCubemapShader.setMat4("view", captureViews[i]);
        GLint view_location = irradiance_program_->uniformLocation("view");
        glm::mat4 currentView = captureViews[i];
        glUniformMatrix4fv(view_location, 1, GL_FALSE, &currentView[0][0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        //glDepthFunc(GL_LESS); // set depth function back to default

    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLWidget::setupPrefilterMap()
{

    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minifcation filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);


    prefilter_program_->bind();
    GLint env_map_location = prefilter_program_->uniformLocation("environmentMap");
    glUniform1i(env_map_location, 0);
    GLint projection_location = prefilter_program_->uniformLocation("projection");
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, &captureProjection[0][0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);


    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        //std::cerr << "Doing mip " << mip << std::endl;
        unsigned int mipWidth  = 128 * std::pow(0.5, mip);
        unsigned int mipHeight = 128 * std::pow(0.5, mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);

        //prefilterShader.setFloat("roughness", roughness);
        //std::cerr << "roughness " << roughness << std::endl;
        GLint roughness_location = prefilter_program_->uniformLocation("roughness");
        glUniform1f(roughness_location, roughness);
        for(unsigned int i = 0; i < 6; ++i)
        {
          GLint view_location = prefilter_program_->uniformLocation("view");
          glm::mat4 currentView = captureViews[i];
          glUniformMatrix4fv(view_location, 1, GL_FALSE, &currentView[0][0]);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

          glBindVertexArray(skyboxVAO);
          glDrawArrays(GL_TRIANGLES, 0, 36);
          glBindVertexArray(0);
          //glDepthFunc(GL_LESS); // set depth function back to default 
        }
        

    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLWidget::setupBRDF()
{
    glGenTextures(1, &brdfLUTTexture);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, 512, 512);

    brdf_program_->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    //RENDER QUAD
    unsigned int quadVAO = 0;
    unsigned int quadVBO;

    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    ///////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void GLWidget::loadHDRenvMap(const QString &path)
{
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float *data = stbi_loadf(path.toStdString().c_str(), &width, &height, &nrComponents, 0);
    
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cerr << "Failed to load HDR image." << std::endl;
    }
}



void GLWidget::setupFramebuffer()
{
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
}
void GLWidget::resizeGL(int w, int h) {
  if (h == 0) h = 1;
  width_ = w;
  height_ = h;

  camera_.SetViewport(0, 0, w, h);
  camera_.SetProjection(kFieldOfView, kZNear, kZFar);
}

void GLWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    camera_.StartRotating(event->x(), event->y());
  }
  if (event->button() == Qt::RightButton) {
    camera_.StartZooming(event->x(), event->y());
  }
  updateGL();
}

void GLWidget::mouseMoveEvent(QMouseEvent *event) {
  camera_.SetRotationX(event->y());
  camera_.SetRotationY(event->x());
  camera_.SafeZoom(event->y());
  updateGL();
}

void GLWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    camera_.StopRotating(event->x(), event->y());
  }
  if (event->button() == Qt::RightButton) {
    camera_.StopZooming(event->x(), event->y());
  }
  updateGL();
}

void GLWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Up) camera_.Zoom(-1);
  if (event->key() == Qt::Key_Down) camera_.Zoom(1);

  if (event->key() == Qt::Key_Left) camera_.Rotate(-1);
  if (event->key() == Qt::Key_Right) camera_.Rotate(1);

  if (event->key() == Qt::Key_W) camera_.Zoom(-1);
  if (event->key() == Qt::Key_S) camera_.Zoom(1);

  if (event->key() == Qt::Key_A) camera_.Rotate(-1);
  if (event->key() == Qt::Key_D) camera_.Rotate(1);

  if (event->key() == Qt::Key_R) {
    reloadShaders();
  }

  updateGL();
}

void GLWidget::paintGL() {
  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (initialized_) {
    camera_.SetViewport();

    Eigen::Matrix4f projection = camera_.SetProjection();
    Eigen::Matrix4f view = camera_.SetView();
    Eigen::Matrix4f model = camera_.SetModel();

    Eigen::Matrix4f t = view* model;
    Eigen::Matrix4f t2 = view;
    t2 = t2.inverse().transpose();
    Eigen::Vector3f cameraPos(t2(3,0), t2(3,1), t2(3,2));
    //std::cerr <<  "The mat" << std::endl;
    //std::cerr << t2(0,0) << " " << t2(0,1) << " " << t2(0,2) << " "<< t2(0,3) << std::endl;
    //std::cerr << t2(1,0) << " " << t2(1,1) << " " << t2(1,2) << " "<< t2(1,3)<< std::endl;
    //std::cerr << t2(2,0) << " " << t2(2,1) << " " << t2(2,2) << " "<< t2(2,3)<< std::endl;
    //std::cerr << t2(3,0) << " " << t2(3,1) << " " << t2(3,2) << " "<< t2(3,3)<< std::endl;
    Eigen::Matrix3f normal;
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) normal(i, j) = t(i, j);

    normal = normal.inverse().transpose();

    if (mesh_ != nullptr) {
      GLint projection_location, view_location, model_location,
          normal_matrix_location, env_map_location,
          prefilter_map_location,brdf_lut_location,camera_position_location;

      if (reflection_) {
        reflection_program_->bind();
        projection_location =
            reflection_program_->uniformLocation("projection");
        view_location = reflection_program_->uniformLocation("view");
        model_location = reflection_program_->uniformLocation("model");
        normal_matrix_location =
            reflection_program_->uniformLocation("normal_matrix");
        env_map_location =
            reflection_program_->uniformLocation("reflection_map");
        camera_position_location = reflection_program_->uniformLocation("camera_pos");
      } else {
        pbr_program_->bind();
        projection_location = pbr_program_->uniformLocation("projection");
        view_location = pbr_program_->uniformLocation("view");
        model_location = pbr_program_->uniformLocation("model");
        normal_matrix_location =
            pbr_program_->uniformLocation("normal_matrix");
        env_map_location = pbr_program_->uniformLocation("irradiance_map");
        camera_position_location = pbr_program_->uniformLocation("camera_pos");
        prefilter_map_location = pbr_program_->uniformLocation("prefilter_map");
        brdf_lut_location = pbr_program_->uniformLocation("brdfLUT");
        //fresnel_location = pbr_program_->uniformLocation("fresnel");
      }

      glUniformMatrix4fv(projection_location, 1, GL_FALSE, projection.data());
      glUniformMatrix4fv(view_location, 1, GL_FALSE, view.data());
      glUniformMatrix4fv(model_location, 1, GL_FALSE, model.data());
      glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, normal.data());
      glUniform3fv(camera_position_location, 1, cameraPos.data());


      if(reflection_)
      {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
      }else {
        glUniform1i(env_map_location,0);
        glUniform1i(prefilter_map_location, 1);
        glUniform1i(brdf_lut_location, 2);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        
        GLint roughness_location = pbr_program_->uniformLocation("roughness");
        GLint metalness_location = pbr_program_->uniformLocation("metalness");
        glUniform1f(roughness_location, roughnessParameter);
        glUniform1f(metalness_location, metalnessParameter);
      }


      glBindVertexArray(modelVAO);
      glDrawElements(GL_TRIANGLES, mesh_->faces_.size(), GL_UNSIGNED_INT, 0);
      glBindVertexArray(0);

    //DEBUG BRDF 2D texture
     /* brdf_program_->bind();
      //RENDER QUAD
      unsigned int quadVAO = 0;
      unsigned int quadVBO;

      if (quadVAO == 0)
      {
          float quadVertices[] = {
              // positions        // texture Coords
              -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
              -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
               1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
               1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
          };
          // setup plane VAO
          glGenVertexArrays(1, &quadVAO);
          glGenBuffers(1, &quadVBO);
          glBindVertexArray(quadVAO);
          glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
          glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
          glEnableVertexAttribArray(0);
          glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
          glEnableVertexAttribArray(1);
          glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
      }
      glBindVertexArray(quadVAO);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      glBindVertexArray(0);*/
      ///////////////////////////


    }

    model = camera_.SetIdentity();

    sky_program_->bind();
    GLint projection_location = sky_program_->uniformLocation("projection");
    GLint view_location = sky_program_->uniformLocation("view");
    GLint model_location = sky_program_->uniformLocation("model");
    GLint normal_matrix_location =
        sky_program_->uniformLocation("normal_matrix");
    GLint specular_map_location = sky_program_->uniformLocation("specular_map");

    glUniformMatrix4fv(projection_location, 1, GL_FALSE, projection.data());
    glUniformMatrix4fv(view_location, 1, GL_FALSE, view.data());
    glUniformMatrix4fv(model_location, 1, GL_FALSE, model.data());
    glUniformMatrix3fv(normal_matrix_location, 1, GL_FALSE, normal.data());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glUniform1i(specular_map_location, 0);

    // TODO(students): implement the rendering of a bounding cube displaying the
    // environment map.
    glBindVertexArray(skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    //glDepthFunc(GL_LESS); // set depth function back to default
    // END.
  }
}

void GLWidget::SetReflection(bool set) {
  reflection_ = set;
  updateGL();
}

void GLWidget::SetBRDF(bool set) {
  reflection_ = !set;
  updateGL();
}

void GLWidget::SetRoughness(double r) {
  roughnessParameter = r;
  updateGL();
}

void GLWidget::SetMetalness(double m) {
  metalnessParameter = m;
  updateGL();
}

