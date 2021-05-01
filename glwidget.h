// Author: Marc Comino 2020

#ifndef GLWIDGET_H_
#define GLWIDGET_H_

#include <GL/glew.h>
#include <QGLWidget>
#include <QImage>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QString>

#include <memory>

#include "./camera.h"
#include "./triangle_mesh.h"

class GLWidget : public QGLWidget {
  Q_OBJECT

 public:
  explicit GLWidget(QWidget *parent = nullptr);
  ~GLWidget();

  /**
   * @brief LoadModel Loads a PLY model at the filename path into the mesh_ data
   * structure.
   * @param filename Path to the PLY model.
   * @return Whether it was able to load the model.
   */
  bool LoadModel(const QString &filename);

  /**
   * @brief LoadSpecularMap Will load load a cube map that will be used for the
   * specular component.
   * @param filename Path to the directory containing the 6 textures (right,
   * left, top, bottom, front back) of the sube map that will be used for the
   * specular component.
   * @return Whether it was able to load the textures.
   */
  bool LoadSpecularMap(const QString &filename);

  /**
   * @brief LoadDiffuseMap Will load load a cube map that will be used for the
   * specular component.
   * @param filename Path to the directory containing the 6 textures (right,
   * left, top, bottom, front back) of the sube map that will be used for the
   * diffuse component.
   * @return Whether it was able to load the textures.
   */
  bool LoadDiffuseMap(const QString &filename);


  bool loadCubemapFileHDR(const QString &filename);


 protected:
  /**
   * @brief initializeGL Initializes OpenGL variables and loads, compiles and
   * links shaders.
   */
  void initializeGL();

  /**
   * @brief resizeGL Resizes the viewport.
   * @param w New viewport width.
   * @param h New viewport height.
   */

  void setupFramebuffer();
  void loadHDRenvMap(const QString &filename);
  void setupEnvMap();
  void setupIrrMap();
  void setupPrefilterMap();
  void setupBRDF();
  
  void resizeGL(int w, int h);

  void reloadShaders();
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void keyPressEvent(QKeyEvent *event);

 private:
  /**
   * @brief program_ The reflection shader program.
   */
  std::unique_ptr<QOpenGLShaderProgram> reflection_program_;

  /**
   * @brief program_ The brdf shader program.
   */
  std::unique_ptr<QOpenGLShaderProgram> brdf_program_;

  /**
   * @brief program_ The skybox shader program.
   */
  std::unique_ptr<QOpenGLShaderProgram> sky_program_;

  std::unique_ptr<QOpenGLShaderProgram> equirect_to_cubemap_program_;

  std::unique_ptr<QOpenGLShaderProgram> irradiance_program_;

  std::unique_ptr<QOpenGLShaderProgram> prefilter_program_;

  std::unique_ptr<QOpenGLShaderProgram> pbr_program_;

  /**
   * @brief camera_ Class that computes the multiple camera transform matrices.
   */
  data_visualization::Camera camera_;

  /**
   * @brief mesh_ Data structure representing a triangle mesh.
   */
  std::unique_ptr<data_representation::TriangleMesh> mesh_;

  /**
   * @brief diffuse_map_ Diffuse cubemap texture.
   */
  GLuint diffuse_map_;

  /**
   * @brief specular_map_ Diffuse cubemap texture.
   */
  GLuint specular_map_;

  /**
   * @brief initialized_ Whether the widget has finished initializations.
   */


GLuint modelVAO;
GLuint modelVBO;
GLuint modelEBO;
GLuint skyboxVAO;
GLuint skyboxVBO;

//famebuffer
GLuint captureFBO;
GLuint captureRBO;

GLuint hdrTexture;
GLuint envCubemap;
GLuint irradianceMap;
GLuint prefilterMap;

GLuint brdfLUTTexture;

  float metalnessParameter;
  float roughnessParameter;
  bool initialized_;

  /**
   * @brief width_ Viewport current width.
   */
  float width_;

  /**
   * @brief height_ Viewport current height.
   */
  float height_;

  /**
   * @brief reflection_ Whether to use the reflection shader or the brdf shader.
   */
  bool reflection_;

  /**
   * @brief fresnel_ Fresnel F0 color components.
   */
  Eigen::Vector3d fresnel_;

 protected slots:
  /**
   * @brief paintGL Function that handles rendering the scene.
   */
  void paintGL();

  /**
   * @brief SetReflection Enables the reflection shader.
   */
  void SetReflection(bool set);

  /**
   * @brief SetReflection Enables the brdf shader.
   */
  void SetBRDF(bool set);


  void SetRoughness(double);


  void SetMetalness(double);



 signals:
  /**
   * @brief SetFaces Signal that updates the interface label "Faces".
   */
  void SetFaces(QString);

  /**
   * @brief SetFaces Signal that updates the interface label "Vertices".
   */
  void SetVertices(QString);

  /**
   * @brief SetFaces Signal that updates the interface label "Framerate".
   */
  void SetFramerate(QString);
};

#endif  //  GLWIDGET_H_
